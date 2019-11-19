// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_platform.h"

#include <objidl.h>
#include <spellcheck.h>
#include <wrl/client.h>

#include <codecvt>
#include <locale>
#include <string>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/com_init_util.h"
#include "base/win/windows_types.h"
#include "base/win/windows_version.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"

namespace spellcheck_platform {

namespace {
// WindowsSpellChecker class is used to store all the COM objects and
// control their lifetime. The class also provides wrappers for
// ISpellCheckerFactory and ISpellChecker APIs. All COM calls are on the
// background thread.
class WindowsSpellChecker {
 public:
  WindowsSpellChecker(
      const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);

  void CreateSpellChecker(const std::string& lang_tag,
                          base::OnceCallback<void(bool)> callback);

  void DisableSpellChecker(const std::string& lang_tag);

  void RequestTextCheckForAllLanguages(int document_tag,
                                       const base::string16& text,
                                       TextCheckCompleteCallback callback);

  void AddWordForAllLanguages(const base::string16& word);

  void RemoveWordForAllLanguages(const base::string16& word);

  void IgnoreWordForAllLanguages(const base::string16& word);

  void RecordMissingLanguagePacksCount(
      const std::vector<std::string> spellcheck_locales,
      SpellCheckHostMetrics* metrics);

 private:
  void CreateSpellCheckerFactoryInBackgroundThread();

  // Creates ISpellchecker for given language |lang_tag| and run callback with
  // the creation result. This function must run on the background thread.
  void CreateSpellCheckerWithCallbackInBackgroundThread(
      const std::string& lang_tag,
      base::OnceCallback<void(bool)> callback);

  // Removes the Windows Spellchecker for the given language |lang_tag|. This
  // function must run on the background thread.
  void DisableSpellCheckerInBackgroundThread(const std::string& lang_tag);

  // Request spell checking of string |text| for all available spellchecking
  // languages and run callback with spellchecking results. This function must
  // run on the background thread.
  void RequestTextCheckForAllLanguagesInBackgroundThread(
      int document_tag,
      const base::string16& text,
      TextCheckCompleteCallback callback);

  // Fills the given vector |optional_suggestions| with a number (up to
  // kMaxSuggestions) of suggestions for the string |wrong_word| of language
  // |lang_tag|. This function must run on the background thread.
  void FillSuggestionListInBackgroundThread(
      const std::string& lang_tag,
      const base::string16& wrong_word,
      std::vector<base::string16>* optional_suggestions);

  void AddWordForAllLanguagesInBackgroundThread(const base::string16& word);

  void RemoveWordForAllLanguagesInBackgroundThread(const base::string16& word);

  void IgnoreWordForAllLanguagesInBackgroundThread(const base::string16& word);

  // Returns true if spellchecker is available for the given language
  // |current_language|. This function must run on the background thread.
  bool IsLanguageSupportedInBackgroundThread(
      const std::string& current_language);

  // Returns true if ISpellCheckerFactory has been initialized.
  bool IsSpellCheckerFactoryInitialized();

  // Returns true if the ISpellChecker has been initialized for given laugnage
  // |lang_tag|.
  bool SpellCheckerReady(const std::string& lang_tag);

  // Returns the ISpellChecker pointer for given language |lang_tag|.
  Microsoft::WRL::ComPtr<ISpellChecker> GetSpellChecker(
      const std::string& lang_tag);

  // Records how many user spellcheck languages are currently not supported by
  // the Windows OS spellchecker due to missing language packs. Must run on the
  // background thread.
  void RecordMissingLanguagePacksCountInBackgroundThread(
      const std::vector<std::string> spellcheck_locales,
      SpellCheckHostMetrics* metrics);

  // Spellchecker objects are owned by WindowsSpellChecker class.
  Microsoft::WRL::ComPtr<ISpellCheckerFactory> spell_checker_factory_;
  std::map<std::string, Microsoft::WRL::ComPtr<ISpellChecker>>
      spell_checker_map_;

  // |main_task_runner_| is running on the main thread, which is used to post
  // task to the main thread from the background thread.
  // |background_task_runner_| is running on the background thread, which is
  // used to post task to the background thread from main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;
  base::WeakPtrFactory<WindowsSpellChecker> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WindowsSpellChecker);
};

WindowsSpellChecker::WindowsSpellChecker(
    const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner> background_task_runner)
    : main_task_runner_(main_task_runner),
      background_task_runner_(background_task_runner) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::CreateSpellCheckerFactoryInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr()));
}

void WindowsSpellChecker::CreateSpellChecker(
    const std::string& lang_tag,
    base::OnceCallback<void(bool)> callback) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowsSpellChecker::
                         CreateSpellCheckerWithCallbackInBackgroundThread,
                     weak_ptr_factory_.GetWeakPtr(), lang_tag,
                     std::move(callback)));
}

void WindowsSpellChecker::DisableSpellChecker(const std::string& lang_tag) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::DisableSpellCheckerInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr(), lang_tag));
}

void WindowsSpellChecker::RequestTextCheckForAllLanguages(
    int document_tag,
    const base::string16& text,
    TextCheckCompleteCallback callback) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowsSpellChecker::
                         RequestTextCheckForAllLanguagesInBackgroundThread,
                     weak_ptr_factory_.GetWeakPtr(), document_tag, text,
                     std::move(callback)));
}

void WindowsSpellChecker::AddWordForAllLanguages(const base::string16& word) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::AddWordForAllLanguagesInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr(), word));
}

void WindowsSpellChecker::RemoveWordForAllLanguages(
    const base::string16& word) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::RemoveWordForAllLanguagesInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr(), word));
}

void WindowsSpellChecker::IgnoreWordForAllLanguages(
    const base::string16& word) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsSpellChecker::IgnoreWordForAllLanguagesInBackgroundThread,
          weak_ptr_factory_.GetWeakPtr(), word));
}

void WindowsSpellChecker::RecordMissingLanguagePacksCount(
    const std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowsSpellChecker::
                         RecordMissingLanguagePacksCountInBackgroundThread,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(spellcheck_locales), metrics));
}

void WindowsSpellChecker::CreateSpellCheckerFactoryInBackgroundThread() {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
  base::win::AssertComApartmentType(base::win::ComApartmentType::STA);

  if (!spellcheck::WindowsVersionSupportsSpellchecker() ||
      FAILED(::CoCreateInstance(__uuidof(::SpellCheckerFactory), nullptr,
                                (CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER),
                                IID_PPV_ARGS(&spell_checker_factory_)))) {
    spell_checker_factory_ = nullptr;
  }
}

void WindowsSpellChecker::CreateSpellCheckerWithCallbackInBackgroundThread(
    const std::string& lang_tag,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());

  bool result = false;
  if (IsSpellCheckerFactoryInitialized()) {
    if (SpellCheckerReady(lang_tag)) {
      result = true;
    } else if (IsLanguageSupportedInBackgroundThread(lang_tag)) {
      Microsoft::WRL::ComPtr<ISpellChecker> spell_checker;
      std::wstring bcp47_language_tag = base::UTF8ToWide(lang_tag);
      HRESULT hr = spell_checker_factory_->CreateSpellChecker(
          bcp47_language_tag.c_str(), &spell_checker);
      if (SUCCEEDED(hr)) {
        spell_checker_map_.insert({lang_tag, spell_checker});
        result = true;
      }
    }
  }

  // Run the callback with result on the main thread.
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), result));
}

void WindowsSpellChecker::DisableSpellCheckerInBackgroundThread(
    const std::string& lang_tag) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());

  if (!IsSpellCheckerFactoryInitialized())
    return;

  auto it = spell_checker_map_.find(lang_tag);
  if (it != spell_checker_map_.end()) {
    spell_checker_map_.erase(it);
  }
}

void WindowsSpellChecker::RequestTextCheckForAllLanguagesInBackgroundThread(
    int document_tag,
    const base::string16& text,
    TextCheckCompleteCallback callback) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());

  // Construct a map to store spellchecking results. The key of the map is a
  // tuple which contains the start index and the word length of the misspelled
  // word. The value of the map is a vector which contains suggestion lists for
  // each available language.
  std::map<std::tuple<ULONG, ULONG>, std::vector<std::vector<base::string16>>>
      result_map;

  std::vector<SpellCheckResult> results;
  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    std::wstring word_to_check_wide(base::UTF16ToWide(text));
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
          std::vector<base::string16> suggestions;
          FillSuggestionListInBackgroundThread(
              it->first, text.substr(start_index, error_length), &suggestions);
          result_map[std::tuple<ULONG, ULONG>(start_index, error_length)]
              .push_back(suggestions);
        }
      } while (hr == S_OK);
    }
  }

  // Generates results vector from map. Remove entries if the word is not
  // misspelled for all available laugages.
  for (auto it = result_map.begin(); it != result_map.end();) {
    if (it->second.size() < spell_checker_map_.size()) {
      it = result_map.erase(it);
    } else {
      // Prepare results vector.
      std::vector<base::string16> suggestions_result;
      for (auto suggestions_list : it->second) {
        for (auto suggestions : suggestions_list) {
          suggestions_result.push_back(suggestions);
        }
      }

      results.push_back(SpellCheckResult(
          SpellCheckResult::Decoration::SPELLING, std::get<0>(it->first),
          std::get<1>(it->first), suggestions_result));

      ++it;
    }
  }

  // Runs the callback on the main thread after spellcheck completed.
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), results));
}

void WindowsSpellChecker::FillSuggestionListInBackgroundThread(
    const std::string& lang_tag,
    const base::string16& wrong_word,
    std::vector<base::string16>* optional_suggestions) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());

  std::wstring word_wide(base::UTF16ToWide(wrong_word));

  Microsoft::WRL::ComPtr<IEnumString> suggestions;
  HRESULT hr =
      GetSpellChecker(lang_tag)->Suggest(word_wide.c_str(), &suggestions);

  // Populate the vector of WideStrings.
  while (hr == S_OK) {
    wchar_t* suggestion = nullptr;
    hr = suggestions->Next(1, &suggestion, nullptr);
    if (hr == S_OK) {
      base::string16 utf16_suggestion;
      if (base::WideToUTF16(suggestion, wcslen(suggestion),
                            &utf16_suggestion)) {
        optional_suggestions->push_back(utf16_suggestion);
      }
      CoTaskMemFree(suggestion);
    }
  }
}

void WindowsSpellChecker::AddWordForAllLanguagesInBackgroundThread(
    const base::string16& word) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    std::wstring word_to_add_wide(base::UTF16ToWide(word));
    it->second->Add(word_to_add_wide.c_str());
  }
}

void WindowsSpellChecker::RemoveWordForAllLanguagesInBackgroundThread(
    const base::string16& word) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
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

void WindowsSpellChecker::IgnoreWordForAllLanguagesInBackgroundThread(
    const base::string16& word) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
  for (auto it = spell_checker_map_.begin(); it != spell_checker_map_.end();
       ++it) {
    std::wstring word_to_ignore_wide(base::UTF16ToWide(word));
    it->second->Ignore(word_to_ignore_wide.c_str());
  }
}

bool WindowsSpellChecker::IsLanguageSupportedInBackgroundThread(
    const std::string& current_language) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());

  if (!IsSpellCheckerFactoryInitialized()) {
    // The native spellchecker creation failed; no language is supported.
    return false;
  }

  BOOL is_language_supported = (BOOL) false;
  std::wstring bcp47_language_tag = base::UTF8ToWide(current_language);

  HRESULT hr = spell_checker_factory_->IsSupported(bcp47_language_tag.c_str(),
                                                   &is_language_supported);
  return SUCCEEDED(hr) && is_language_supported;
}

bool WindowsSpellChecker::IsSpellCheckerFactoryInitialized() {
  return spell_checker_factory_ != nullptr;
}

bool WindowsSpellChecker::SpellCheckerReady(const std::string& lang_tag) {
  return spell_checker_map_.find(lang_tag) != spell_checker_map_.end();
}

Microsoft::WRL::ComPtr<ISpellChecker> WindowsSpellChecker::GetSpellChecker(
    const std::string& lang_tag) {
  DCHECK(SpellCheckerReady(lang_tag));
  return spell_checker_map_.find(lang_tag)->second;
}

void WindowsSpellChecker::RecordMissingLanguagePacksCountInBackgroundThread(
    const std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics) {
  DCHECK(!main_task_runner_->BelongsToCurrentThread());
  DCHECK(metrics);

  if (!IsSpellCheckerFactoryInitialized()) {
    // The native spellchecker creation failed. Do not record any metrics.
    return;
  }

  metrics->RecordMissingLanguagePacksCount(
      std::count_if(spellcheck_locales.begin(), spellcheck_locales.end(),
                    [this](const std::string& s) {
                      return !this->IsLanguageSupportedInBackgroundThread(s);
                    }));
}

// Create WindowsSpellChecker class with static storage duration that is only
// constructed on first access and never invokes the destructor.
std::unique_ptr<WindowsSpellChecker>& GetWindowsSpellChecker() {
  static base::NoDestructor<std::unique_ptr<WindowsSpellChecker>>
      win_spell_checker(std::make_unique<WindowsSpellChecker>(
          base::ThreadTaskRunnerHandle::Get(),
          base::CreateCOMSTATaskRunner(
              {base::ThreadPool(), base::MayBlock()})));
  return *win_spell_checker;
}
}  // anonymous namespace

bool SpellCheckerAvailable() {
  return true;
}

bool PlatformSupportsLanguage(const std::string& current_language) {
  return true;
}

void SetLanguage(const std::string& lang_to_set,
                 base::OnceCallback<void(bool)> callback) {
  GetWindowsSpellChecker()->CreateSpellChecker(lang_to_set,
                                               std::move(callback));
}

void DisableLanguage(const std::string& lang_to_disable) {
  GetWindowsSpellChecker()->DisableSpellChecker(lang_to_disable);
}

bool CheckSpelling(const base::string16& word_to_check, int tag) {
  return true;  // Not used in Windows
}

void FillSuggestionList(const base::string16& wrong_word,
                        std::vector<base::string16>* optional_suggestions) {
  // Not used in Windows.
}

void RequestTextCheck(int document_tag,
                      const base::string16& text,
                      TextCheckCompleteCallback callback) {
  GetWindowsSpellChecker()->RequestTextCheckForAllLanguages(
      document_tag, text, std::move(callback));
}

void AddWord(const base::string16& word) {
  GetWindowsSpellChecker()->AddWordForAllLanguages(word);
}

void RemoveWord(const base::string16& word) {
  GetWindowsSpellChecker()->RemoveWordForAllLanguages(word);
}

void IgnoreWord(const base::string16& word) {
  GetWindowsSpellChecker()->IgnoreWordForAllLanguages(word);
}

void GetAvailableLanguages(std::vector<std::string>* spellcheck_languages) {
  // Not used in Windows
}

int GetDocumentTag() {
  return 1;  // Not used in Windows
}

void CloseDocumentWithTag(int tag) {
  // Not implemented since Windows spellchecker doesn't have this concept
}

bool SpellCheckerProvidesPanel() {
  return false;  // Windows doesn't have a spelling panel
}

bool SpellingPanelVisible() {
  return false;  // Windows doesn't have a spelling panel
}

void ShowSpellingPanel(bool show) {
  // Not implemented since Windows doesn't have spelling panel like Mac
}

void UpdateSpellingPanelWithMisspelledWord(const base::string16& word) {
  // Not implemented since Windows doesn't have spelling panel like Mac
}

void RecordMissingLanguagePacksCount(
    const std::vector<std::string> spellcheck_locales,
    SpellCheckHostMetrics* metrics) {
  if (spellcheck::WindowsVersionSupportsSpellchecker()) {
    GetWindowsSpellChecker()->RecordMissingLanguagePacksCount(
        std::move(spellcheck_locales), metrics);
  }
}
}  // namespace spellcheck_platform
