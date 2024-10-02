// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/windows_spell_checker.h"

#include <objidl.h>
#include <spellcheck.h>
#include <windows.foundation.collections.h>
#include <windows.globalization.h>
#include <windows.system.userprofile.h>
#include <wrl/client.h>

#include <algorithm>
#include <locale>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_hstring.h"
#include "build/build_config.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/spellcheck_buildflags.h"

namespace windows_spell_checker {

// Helper class that handles calls to the native Windows APIs. All
// invocations of these methods must be posted to the same COM
// |SingleThreadTaskRunner|. This is enforced by checks that all methods run
// on the given |SingleThreadTaskRunner|.
class BackgroundHelper {
 public:
  explicit BackgroundHelper(
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);
  ~BackgroundHelper();

  // Creates the native spell check factory, which is the main entry point to
  // the native spell checking APIs.
  void CreateSpellCheckerFactory();

  // Creates a native |ISpellchecker| for the given language |lang_tag| and
  // returns a boolean indicating success.
  bool CreateSpellChecker(const std::string& lang_tag);

  // Removes the native spell checker for the given language |lang_tag| from
  // the map of active spell checkers.
  void DisableSpellChecker(const std::string& lang_tag);

  // Requests spell checking of string |text| for all active spell checkers
  // (all languages) and returns a vector of |SpellCheckResult| containing the
  // results.
  std::vector<SpellCheckResult> RequestTextCheckForAllLanguages(
      int document_tag,
      const std::u16string& text);

  // Gets spelling suggestions for |word| from all active spell checkers (all
  // languages), keeping the suggestions separate per language, and returns
  // the results in a vector of vector of strings.
  spellcheck::PerLanguageSuggestions GetPerLanguageSuggestions(
      const std::u16string& word);

  // Fills the given vector |optional_suggestions| with a number (up to
  // kMaxSuggestions) of suggestions for the string |wrong_word| using the
  // native spell checker for language |lang_tag|.
  void FillSuggestionList(const std::string& lang_tag,
                          const std::u16string& wrong_word,
                          std::vector<std::u16string>* optional_suggestions);

  // Adds |word| to the native dictionary of all active spell checkers (all
  // languages).
  void AddWordForAllLanguages(const std::u16string& word);

  // Removes |word| from the native dictionary of all active spell checkers
  // (all languages). This requires a newer version of the native spell
  // check APIs, so it may be a no-op on older Windows versions.
  void RemoveWordForAllLanguages(const std::u16string& word);

  // Adds |word| to the ignore list of all active spell checkers (all
  // languages).
  void IgnoreWordForAllLanguages(const std::u16string& word);

  // Returns |true| if a native spell checker is available for the given
  // language |lang_tag|. This is based on the installed language packs in the
  // OS settings.
  bool IsLanguageSupported(const std::string& lang_tag);

  // Returns |true| if an |ISpellCheckerFactory| has been initialized.
  bool IsSpellCheckerFactoryInitialized();

  // Returns |true| if an |ISpellChecker| has been initialized for the given
  // language |lang_tag|.
  bool SpellCheckerReady(const std::string& lang_tag);

  // Returns the |ISpellChecker| pointer for the given language |lang_tag|.
  Microsoft::WRL::ComPtr<ISpellChecker> GetSpellChecker(
      const std::string& lang_tag);

  // Records metrics about spell check support for the user's Chrome locales.
  void RecordChromeLocalesStats(std::vector<std::string> chrome_locales);

  // Records metrics about spell check support for the user's enabled spell
  // check locales.
  void RecordSpellcheckLocalesStats(
      std::vector<std::string> spellcheck_locales);

  // Retrieve language tags for registered Windows OS
  // spellcheckers on the system.
  std::vector<std::string> RetrieveSpellcheckLanguages();

  // Test-only method for adding fake list of Windows spellcheck languages.
  void AddSpellcheckLanguagesForTesting(
      const std::vector<std::string>& languages);

  // Sorts the given locales into four buckets based on spell check support
  // (both native and Hunspell, Hunspell only, native only, none).
  LocalesSupportInfo DetermineLocalesSupport(
      const std::vector<std::string>& locales);

 private:
  // The native factory to interact with spell check APIs.
  Microsoft::WRL::ComPtr<ISpellCheckerFactory> spell_checker_factory_;

  // The map of active spell checkers. Each entry maps a language tag to an
  // |ISpellChecker| (there is one |ISpellChecker| per language).
  std::map<std::string, Microsoft::WRL::ComPtr<ISpellChecker>>
      spell_checker_map_;

  std::vector<std::string> windows_spellcheck_languages_for_testing_;

  // Task runner only used to enforce valid sequencing.
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;
};  // class BackgroundHelper

BackgroundHelper::BackgroundHelper(
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner)
    : background_task_runner_(std::move(background_task_runner)) {}

BackgroundHelper::~BackgroundHelper() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
}

void BackgroundHelper::CreateSpellCheckerFactory() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  base::win::AssertComApartmentType(base::win::ComApartmentType::STA);

  // Mitigate the issues caused by loading DLLs on a background thread
  // (https://issues.chromium.org/issues/41464781).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();
  if (FAILED(::CoCreateInstance(__uuidof(::SpellCheckerFactory), nullptr,
                                (CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER),
                                IID_PPV_ARGS(&spell_checker_factory_)))) {
    spell_checker_factory_ = nullptr;
  }
}

bool BackgroundHelper::CreateSpellChecker(const std::string& lang_tag) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!IsSpellCheckerFactoryInitialized())
    return false;

  if (SpellCheckerReady(lang_tag))
    return true;

  if (!IsLanguageSupported(lang_tag))
    return false;

  Microsoft::WRL::ComPtr<ISpellChecker> spell_checker;
  std::wstring bcp47_language_tag = base::UTF8ToWide(lang_tag);
  HRESULT hr = spell_checker_factory_->CreateSpellChecker(
      bcp47_language_tag.c_str(), &spell_checker);

  if (SUCCEEDED(hr)) {
    spell_checker_map_.insert({lang_tag, spell_checker});
    return true;
  }

  return false;
}

void BackgroundHelper::DisableSpellChecker(const std::string& lang_tag) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!IsSpellCheckerFactoryInitialized())
    return;

  auto it = spell_checker_map_.find(lang_tag);
  if (it != spell_checker_map_.end()) {
    spell_checker_map_.erase(it);
  }
}

std::vector<SpellCheckResult> BackgroundHelper::RequestTextCheckForAllLanguages(
    int document_tag,
    const std::u16string& text) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  // Construct a map to store spellchecking results. The key of the map is a
  // tuple which contains the start index and the word length of the misspelled
  // word. The value of the map is a vector which contains suggestion lists for
  // each available language. This allows to quickly see if all languages agree
  // about a misspelling, and makes it easier to evenly pick suggestions from
  // all the different languages.
  std::map<std::tuple<ULONG, ULONG>, spellcheck::PerLanguageSuggestions>
      result_map;
  std::wstring word_to_check_wide(base::UTF16ToWide(text));

  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    Microsoft::WRL::ComPtr<IEnumSpellingError> spelling_errors;

    HRESULT hr = it->second->ComprehensiveCheck(word_to_check_wide.c_str(),
                                                &spelling_errors);
    if (SUCCEEDED(hr) && spelling_errors) {
      do {
        Microsoft::WRL::ComPtr<ISpellingError> spelling_error;
        ULONG start_index = 0;
        ULONG error_length = 0;
        CORRECTIVE_ACTION action = CORRECTIVE_ACTION_NONE;
        hr = spelling_errors->Next(&spelling_error);
        if (SUCCEEDED(hr) && spelling_error &&
            SUCCEEDED(spelling_error->get_StartIndex(&start_index)) &&
            SUCCEEDED(spelling_error->get_Length(&error_length)) &&
            SUCCEEDED(spelling_error->get_CorrectiveAction(&action)) &&
            (action == CORRECTIVE_ACTION_GET_SUGGESTIONS ||
             action == CORRECTIVE_ACTION_REPLACE)) {
          std::vector<std::u16string> suggestions;
          if (!base::FeatureList::IsEnabled(
                  spellcheck::kWinRetrieveSuggestionsOnlyOnDemand)) {
            // Perform the expensive operation of retrieving suggestions for all
            // misspelled words while performing a text check. If
            // kWinRetrieveSuggestionsOnlyOnDemand is set, suggestions will
            // be retrieved on demand when the context menu is brought up with a
            // misspelled word selected, and the spellcheck results returned by
            // this method will have empty suggestion lists.
            FillSuggestionList(it->first,
                               text.substr(start_index, error_length),
                               &suggestions);
          }

          result_map[std::tuple<ULONG, ULONG>(start_index, error_length)]
              .push_back(suggestions);
        }
      } while (hr == S_OK);
    }
  }

  std::vector<SpellCheckResult> final_results;

  for (auto it = result_map.begin(); it != result_map.end();) {
    if (it->second.size() < spell_checker_map_.size()) {
      // Some languages considered this correctly spelled, so ignore this
      // result.
      it = result_map.erase(it);
    } else {
      std::vector<std::u16string> evenly_filled_suggestions;
      spellcheck::FillSuggestions(/*suggestions_list=*/it->second,
                                  &evenly_filled_suggestions);
      final_results.push_back(SpellCheckResult(
          SpellCheckResult::Decoration::SPELLING, std::get<0>(it->first),
          std::get<1>(it->first), evenly_filled_suggestions));
      ++it;
    }
  }

  return final_results;
}

spellcheck::PerLanguageSuggestions BackgroundHelper::GetPerLanguageSuggestions(
    const std::u16string& word) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  spellcheck::PerLanguageSuggestions suggestions;
  std::vector<std::u16string> language_suggestions;

  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    language_suggestions.clear();
    FillSuggestionList(it->first, word, &language_suggestions);
    suggestions.push_back(language_suggestions);
  }

  return suggestions;
}

void BackgroundHelper::FillSuggestionList(
    const std::string& lang_tag,
    const std::u16string& wrong_word,
    std::vector<std::u16string>* optional_suggestions) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  std::wstring word_wide(base::UTF16ToWide(wrong_word));

  Microsoft::WRL::ComPtr<IEnumString> suggestions;
  HRESULT hr =
      GetSpellChecker(lang_tag)->Suggest(word_wide.c_str(), &suggestions);

  // Populate the vector of WideStrings.
  while (hr == S_OK) {
    base::win::ScopedCoMem<wchar_t> suggestion;
    hr = suggestions->Next(1, &suggestion, nullptr);
    if (hr == S_OK) {
      std::u16string utf16_suggestion;
      if (base::WideToUTF16(suggestion.get(), wcslen(suggestion),
                            &utf16_suggestion)) {
        optional_suggestions->push_back(utf16_suggestion);
      }
    }
  }
}

void BackgroundHelper::AddWordForAllLanguages(const std::u16string& word) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    std::wstring word_to_add_wide(base::UTF16ToWide(word));
    it->second->Add(word_to_add_wide.c_str());
  }
}

void BackgroundHelper::RemoveWordForAllLanguages(const std::u16string& word) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    std::wstring word_to_remove_wide(base::UTF16ToWide(word));
    Microsoft::WRL::ComPtr<ISpellChecker2> spell_checker_2;
    it->second->QueryInterface(IID_PPV_ARGS(&spell_checker_2));
    if (spell_checker_2 != nullptr) {
      spell_checker_2->Remove(word_to_remove_wide.c_str());
    }
  }
}

void BackgroundHelper::IgnoreWordForAllLanguages(const std::u16string& word) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    std::wstring word_to_ignore_wide(base::UTF16ToWide(word));
    it->second->Ignore(word_to_ignore_wide.c_str());
  }
}

bool BackgroundHelper::IsLanguageSupported(const std::string& lang_tag) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!IsSpellCheckerFactoryInitialized()) {
    // The native spellchecker creation failed; no language is supported.
    return false;
  }

  BOOL is_language_supported = (BOOL) false;
  std::wstring bcp47_language_tag = base::UTF8ToWide(lang_tag);

  HRESULT hr = spell_checker_factory_->IsSupported(bcp47_language_tag.c_str(),
                                                   &is_language_supported);
  return SUCCEEDED(hr) && is_language_supported;
}

std::vector<std::string> BackgroundHelper::RetrieveSpellcheckLanguages() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  std::vector<std::string> spellcheck_languages;

  if (!windows_spellcheck_languages_for_testing_.empty())
    return windows_spellcheck_languages_for_testing_;

  if (!IsSpellCheckerFactoryInitialized())
    return spellcheck_languages;

  Microsoft::WRL::ComPtr<IEnumString> supported_languages;
  HRESULT hr =
      spell_checker_factory_->get_SupportedLanguages(&supported_languages);
  DVLOG_IF(1, FAILED(hr)) << "Call to get_SupportedLanguages failed, hr="
                          << logging::SystemErrorCodeToString(hr);
  if (!SUCCEEDED(hr))
    return spellcheck_languages;

  while (hr == S_OK) {
    base::win::ScopedCoMem<wchar_t> supported_language;
    hr = supported_languages->Next(
        1 /* items to retrieve */, &supported_language,
        nullptr /* number of items retrieved, unneeded if only 1 requested */);
    if (hr == S_OK) {
      spellcheck_languages.push_back(
          base::WideToUTF8(supported_language.get()));
    }
  }

  return spellcheck_languages;
}

void BackgroundHelper::AddSpellcheckLanguagesForTesting(
    const std::vector<std::string>& languages) {
  windows_spellcheck_languages_for_testing_ = languages;
}

LocalesSupportInfo BackgroundHelper::DetermineLocalesSupport(
    const std::vector<std::string>& locales) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  size_t locales_supported_by_hunspell_and_native = 0;
  size_t locales_supported_by_hunspell_only = 0;
  size_t locales_supported_by_native_only = 0;
  size_t unsupported_locales = 0;

  for (const auto& lang : locales) {
    bool hunspell_support =
        !spellcheck::GetCorrespondingSpellCheckLanguage(lang).empty();
    bool native_support = this->IsLanguageSupported(lang);

    if (hunspell_support && native_support) {
      locales_supported_by_hunspell_and_native++;
    } else if (hunspell_support && !native_support) {
      locales_supported_by_hunspell_only++;
    } else if (!hunspell_support && native_support) {
      locales_supported_by_native_only++;
    } else {
      unsupported_locales++;
    }
  }

  return LocalesSupportInfo{locales_supported_by_hunspell_and_native,
                            locales_supported_by_hunspell_only,
                            locales_supported_by_native_only,
                            unsupported_locales};
}

bool BackgroundHelper::IsSpellCheckerFactoryInitialized() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  return spell_checker_factory_ != nullptr;
}

bool BackgroundHelper::SpellCheckerReady(const std::string& lang_tag) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  return spell_checker_map_.find(lang_tag) != spell_checker_map_.end();
}

Microsoft::WRL::ComPtr<ISpellChecker> BackgroundHelper::GetSpellChecker(
    const std::string& lang_tag) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(SpellCheckerReady(lang_tag));
  return spell_checker_map_.find(lang_tag)->second;
}

void BackgroundHelper::RecordChromeLocalesStats(
    std::vector<std::string> chrome_locales) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!IsSpellCheckerFactoryInitialized()) {
    // The native spellchecker creation failed. Do not record any metrics.
    return;
  }

  const auto& locales_info = DetermineLocalesSupport(chrome_locales);
  SpellCheckHostMetrics::RecordAcceptLanguageStats(locales_info);
}

void BackgroundHelper::RecordSpellcheckLocalesStats(
    std::vector<std::string> spellcheck_locales) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!IsSpellCheckerFactoryInitialized()) {
    // The native spellchecker creation failed. Do not record any metrics.
    return;
  }

  const auto& locales_info = DetermineLocalesSupport(spellcheck_locales);
  SpellCheckHostMetrics::RecordSpellcheckLanguageStats(locales_info);
}

}  // namespace windows_spell_checker

WindowsSpellChecker::WindowsSpellChecker(
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner)
    : background_task_runner_(background_task_runner) {
  background_helper_ =
      std::make_unique<windows_spell_checker::BackgroundHelper>(
          std::move(background_task_runner));

  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &windows_spell_checker::BackgroundHelper::CreateSpellCheckerFactory,
          base::Unretained(background_helper_.get())));
}

WindowsSpellChecker::~WindowsSpellChecker() {
  // |background_helper_| is deleted on the background thread after all other
  // background tasks complete.
  background_task_runner_->DeleteSoon(FROM_HERE, std::move(background_helper_));
}

void WindowsSpellChecker::CreateSpellChecker(
    const std::string& lang_tag,
    base::OnceCallback<void(bool)> callback) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &windows_spell_checker::BackgroundHelper::CreateSpellChecker,
          base::Unretained(background_helper_.get()), lang_tag),
      std::move(callback));
}

void WindowsSpellChecker::DisableSpellChecker(const std::string& lang_tag) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &windows_spell_checker::BackgroundHelper::DisableSpellChecker,
          base::Unretained(background_helper_.get()), lang_tag));
}

void WindowsSpellChecker::RequestTextCheck(
    int document_tag,
    const std::u16string& text,
    spellcheck_platform::TextCheckCompleteCallback callback) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&windows_spell_checker::BackgroundHelper::
                         RequestTextCheckForAllLanguages,
                     base::Unretained(background_helper_.get()), document_tag,
                     text),
      std::move(callback));
}

void WindowsSpellChecker::GetPerLanguageSuggestions(
    const std::u16string& word,
    spellcheck_platform::GetSuggestionsCallback callback) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &windows_spell_checker::BackgroundHelper::GetPerLanguageSuggestions,
          base::Unretained(background_helper_.get()), word),
      std::move(callback));
}

void WindowsSpellChecker::AddWordForAllLanguages(const std::u16string& word) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &windows_spell_checker::BackgroundHelper::AddWordForAllLanguages,
          base::Unretained(background_helper_.get()), word));
}

void WindowsSpellChecker::RemoveWordForAllLanguages(
    const std::u16string& word) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &windows_spell_checker::BackgroundHelper::RemoveWordForAllLanguages,
          base::Unretained(background_helper_.get()), word));
}

void WindowsSpellChecker::IgnoreWordForAllLanguages(
    const std::u16string& word) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &windows_spell_checker::BackgroundHelper::IgnoreWordForAllLanguages,
          base::Unretained(background_helper_.get()), word));
}

void WindowsSpellChecker::RecordChromeLocalesStats(
    std::vector<std::string> chrome_locales) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &windows_spell_checker::BackgroundHelper::RecordChromeLocalesStats,
          base::Unretained(background_helper_.get()),
          std::move(chrome_locales)));
}

void WindowsSpellChecker::RecordSpellcheckLocalesStats(
    std::vector<std::string> spellcheck_locales) {
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&windows_spell_checker::BackgroundHelper::
                                    RecordSpellcheckLocalesStats,
                                base::Unretained(background_helper_.get()),
                                std::move(spellcheck_locales)));
}

void WindowsSpellChecker::IsLanguageSupported(
    const std::string& lang_tag,
    base::OnceCallback<void(bool)> callback) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &windows_spell_checker::BackgroundHelper::IsLanguageSupported,
          base::Unretained(background_helper_.get()), lang_tag),
      std::move(callback));
}

void WindowsSpellChecker::RetrieveSpellcheckLanguages(
    spellcheck_platform::RetrieveSpellcheckLanguagesCompleteCallback callback) {
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &windows_spell_checker::BackgroundHelper::RetrieveSpellcheckLanguages,
          base::Unretained(background_helper_.get())),
      std::move(callback));
}

void WindowsSpellChecker::AddSpellcheckLanguagesForTesting(
    const std::vector<std::string>& languages) {
  background_helper_->AddSpellcheckLanguagesForTesting(languages);
}
