// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/spellcheck_language.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"
#include "components/spellcheck/renderer/spellcheck_renderer_metrics.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_visitor.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_text_checking_completion.h"
#include "third_party/blink/public/web/web_text_checking_result.h"
#include "third_party/blink/public/web/web_text_decoration_type.h"

using blink::WebVector;
using blink::WebString;
using blink::WebTextCheckingResult;
using blink::WebTextDecorationType;

namespace {

class UpdateSpellcheckEnabled : public content::RenderFrameVisitor {
 public:
  explicit UpdateSpellcheckEnabled(bool enabled) : enabled_(enabled) {}

  UpdateSpellcheckEnabled(const UpdateSpellcheckEnabled&) = delete;
  UpdateSpellcheckEnabled& operator=(const UpdateSpellcheckEnabled&) = delete;

  bool Visit(content::RenderFrame* render_frame) override;

 private:
  bool enabled_;  // New spellcheck-enabled state.
};

bool UpdateSpellcheckEnabled::Visit(content::RenderFrame* render_frame) {
  if (!enabled_) {
    if (render_frame && render_frame->GetWebFrame())
      render_frame->GetWebFrame()->RemoveSpellingMarkers();
  }
  return true;
}

WebVector<WebString> ConvertToWebStringFromUtf8(
    const std::set<std::string>& words) {
  WebVector<WebString> result(words.size());
  base::ranges::transform(words, result.begin(), [](const auto& word) {
    return WebString::FromUTF8(word);
  });
  return result;
}

bool IsApostrophe(char16_t c) {
  const char16_t kApostrophe = 0x27;
  const char16_t kRightSingleQuotationMark = 0x2019;
  return c == kApostrophe || c == kRightSingleQuotationMark;
}

// Makes sure that the apostrophes in the |spelling_suggestion| are the same
// type as in the |misspelled_word| and in the same order. Ignore differences in
// the number of apostrophes.
void PreserveOriginalApostropheTypes(const std::u16string& misspelled_word,
                                     std::u16string* spelling_suggestion) {
  auto it = spelling_suggestion->begin();
  for (const char16_t& c : misspelled_word) {
    if (IsApostrophe(c)) {
      it = std::find_if(it, spelling_suggestion->end(), IsApostrophe);
      if (it == spelling_suggestion->end())
        return;

      *it++ = c;
    }
  }
}

std::vector<WebString> FilterReplacementSuggestions(
    const std::u16string& misspelled_word,
    const std::vector<std::u16string>& replacements) {
  std::vector<WebString> replacements_filtered;
  for (std::u16string replacement : replacements) {
    // Use the same types of apostrophes as in the mispelled word.
    PreserveOriginalApostropheTypes(misspelled_word, &replacement);

    // Ignore suggestions that are just changing the apostrophe type
    // (straight vs. typographical)
    if (replacement == misspelled_word)
      continue;

    replacements_filtered.push_back(WebString::FromUTF16(replacement));
  }

  return replacements_filtered;
}

}  // namespace

class SpellCheck::SpellcheckRequest {
 public:
  SpellcheckRequest(
      const std::u16string& text,
      std::unique_ptr<blink::WebTextCheckingCompletion> completion,
      base::WeakPtr<SpellCheckProvider> provider)
      : text_(text),
        completion_(std::move(completion)),
        start_ticks_(base::TimeTicks::Now()),
        provider_(provider) {
    DCHECK(completion_);
  }

  SpellcheckRequest(const SpellcheckRequest&) = delete;
  SpellcheckRequest& operator=(const SpellcheckRequest&) = delete;

  ~SpellcheckRequest() {}

  std::u16string text() { return text_; }
  blink::WebTextCheckingCompletion* completion() { return completion_.get(); }
  base::TimeTicks start_ticks() { return start_ticks_; }

  SpellCheckProvider* provider() { return provider_.get(); }

 private:
  std::u16string text_;  // Text to be checked in this task.

  // The interface to send the misspelled ranges to WebKit.
  std::unique_ptr<blink::WebTextCheckingCompletion> completion_;

  // The time ticks at which this request was created
  base::TimeTicks start_ticks_;

  base::WeakPtr<SpellCheckProvider> provider_;
};


// Initializes SpellCheck object.
// spellcheck_enabled_ currently MUST be set to true, due to peculiarities of
// the initialization sequence.
// Since it defaults to true, newly created SpellCheckProviders will enable
// spellchecking. After the first word is typed, the provider requests a check,
// which in turn triggers the delayed initialization sequence in SpellCheck.
// This does send a message to the browser side, which triggers the creation
// of the SpellcheckService. That does create the observer for the preference
// responsible for enabling/disabling checking, which allows subsequent changes
// to that preference to be sent to all SpellCheckProviders.
// Setting |spellcheck_enabled_| to false by default prevents that mechanism,
// and as such the SpellCheckProviders will never be notified of different
// values.
// TODO(groby): Simplify this.
SpellCheck::SpellCheck(
    service_manager::LocalInterfaceProvider* embedder_provider)
    : embedder_provider_(embedder_provider), spellcheck_enabled_(true) {
  DCHECK(embedder_provider);
}

SpellCheck::~SpellCheck() = default;

void SpellCheck::BindReceiver(
    mojo::PendingReceiver<spellcheck::mojom::SpellChecker> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SpellCheck::Initialize(
    std::vector<spellcheck::mojom::SpellCheckBDictLanguagePtr> dictionaries,
    const std::vector<std::string>& custom_words,
    bool enable) {
  languages_.clear();

  for (const auto& dictionary : dictionaries)
    AddSpellcheckLanguage(std::move(dictionary->file), dictionary->language);

  custom_dictionary_.Init(
      std::set<std::string>(custom_words.begin(), custom_words.end()));
#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  if (!spellcheck::UseBrowserSpellChecker()) {
    PostDelayedSpellCheckTask(pending_request_param_.release());
  }
#endif

  spellcheck_enabled_ = enable;
  UpdateSpellcheckEnabled updater(enable);
  content::RenderFrame::ForEach(&updater);
}

void SpellCheck::CustomDictionaryChanged(
    const std::vector<std::string>& words_added,
    const std::vector<std::string>& words_removed) {
  const std::set<std::string> added(words_added.begin(), words_added.end());
  NotifyDictionaryObservers(ConvertToWebStringFromUtf8(added));
  custom_dictionary_.OnCustomDictionaryChanged(
      added, std::set<std::string>(words_removed.begin(), words_removed.end()));
}

// TODO(groby): Make sure we always have a spelling engine, even before
// AddSpellcheckLanguage() is called.
void SpellCheck::AddSpellcheckLanguage(base::File file,
                                       const std::string& language) {
  languages_.push_back(
      std::make_unique<SpellcheckLanguage>(embedder_provider_));
  languages_.back()->Init(std::move(file), language);
}

bool SpellCheck::SpellCheckWord(std::u16string_view text,
                                spellcheck::mojom::SpellCheckHost& host,
                                size_t* misspelling_start,
                                size_t* misspelling_len,
                                std::nullptr_t null_suggestions_ptr) {
  return SpellCheckWord(
      text, host, misspelling_start, misspelling_len,
      static_cast<spellcheck::PerLanguageSuggestions*>(nullptr));
}

bool SpellCheck::SpellCheckWord(
    std::u16string_view text,
    spellcheck::mojom::SpellCheckHost& host,
    size_t* misspelling_start,
    size_t* misspelling_len,
    std::vector<std::u16string>* optional_suggestions) {
  if (!optional_suggestions) {
    return SpellCheckWord(text, host, misspelling_start, misspelling_len,
                          nullptr);
  }

  bool result;
  spellcheck::PerLanguageSuggestions per_language_suggestions;
  result = SpellCheckWord(text, host, misspelling_start, misspelling_len,
                          &per_language_suggestions);
  spellcheck::FillSuggestions(per_language_suggestions, optional_suggestions);

  return result;
}

bool SpellCheck::SpellCheckWord(
    std::u16string_view text,
    spellcheck::mojom::SpellCheckHost& host,
    size_t* misspelling_start,
    size_t* misspelling_len,
    spellcheck::PerLanguageSuggestions* optional_per_language_suggestions) {
  DCHECK(misspelling_start && misspelling_len) << "Out vars must be given.";

  // Do nothing if we need to delay initialization. (Rather than blocking,
  // report the word as correctly spelled.)
  if (InitializeIfNeeded())
    return true;

  // To prevent an infinite loop below, ensure that at least one language is
  // enabled before starting the check. If no language is enabled, we should
  // never report a spelling mistake, so return true here.
  if (EnabledLanguageCount() == 0) {
    return true;
  }

  // These are for holding misspelling or skippable word positions and lengths
  // between calls to SpellcheckLanguage::SpellCheckWord.
  size_t possible_misspelling_start;
  size_t possible_misspelling_len;
  // The longest sequence of text that all languages agree is skippable.
  size_t agreed_skippable_len;
  // A vector of vectors containing spelling suggestions from different
  // languages.
  std::vector<std::vector<std::u16string>> suggestions_list;
  // A vector to hold a language's misspelling suggestions between spellcheck
  // calls.
  std::vector<std::u16string> language_suggestions;

  const size_t text_length = text.size();
  size_t position_in_text = 0;

  // This loop only advances if all languages agree that a sequence of text is
  // skippable.
  for (; position_in_text <= text_length;
       position_in_text += agreed_skippable_len) {
    // Reseting |agreed_skippable_len| to the worst-case length each time
    // prevents some unnecessary iterations.
    agreed_skippable_len = text_length;
    *misspelling_start = 0;
    *misspelling_len = 0;
    suggestions_list.clear();

    for (auto language = languages_.begin(); language != languages_.end();) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
      if (!(*language)->IsEnabled()) {
        // In the case of hybrid spell checking on Windows, languages that are
        // handled on the browser side are marked as disabled on the renderer
        // side. We do not want to return IS_CORRECT for those languages, so we
        // simply skip them.
        language++;
        continue;
      }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

      size_t possible_misspelling_start_relative_to_substring = 0;
      language_suggestions.clear();
      SpellcheckLanguage::SpellcheckWordResult result =
          (*language)->SpellCheckWord(
              text.substr(position_in_text), host,
              &possible_misspelling_start_relative_to_substring,
              &possible_misspelling_len,
              optional_per_language_suggestions ? &language_suggestions
                                                : nullptr);
      // SpellCheckWord informs us of a misspelling index relative to the
      // substring of text which was passed in, so add `position_in_text` to
      // this value to get the offset relative to `text`.
      possible_misspelling_start =
          possible_misspelling_len > 0
              ? position_in_text +
                    possible_misspelling_start_relative_to_substring
              : 0;

      switch (result) {
        case SpellcheckLanguage::SpellcheckWordResult::IS_CORRECT:
          *misspelling_start = 0;
          *misspelling_len = 0;
          return true;
        case SpellcheckLanguage::SpellcheckWordResult::IS_SKIPPABLE:
          agreed_skippable_len =
              std::min(agreed_skippable_len, possible_misspelling_len);
          // If true, this means the spellchecker moved past a word that was
          // previously determined to be misspelled or skippable, which means
          // another spellcheck language marked it as correct.
          if (position_in_text != possible_misspelling_start) {
            *misspelling_len = 0;
            position_in_text = possible_misspelling_start;
            suggestions_list.clear();
            language = languages_.begin();
          } else {
            language++;
          }
          break;
        case SpellcheckLanguage::SpellcheckWordResult::IS_MISSPELLED:
          *misspelling_start = possible_misspelling_start;
          *misspelling_len = possible_misspelling_len;
          // If true, this means the spellchecker moved past a word that was
          // previously determined to be misspelled or skippable, which means
          // another spellcheck language marked it as correct.
          if (position_in_text != *misspelling_start) {
            suggestions_list.clear();
            language = languages_.begin();
            position_in_text = *misspelling_start;
          } else {
            suggestions_list.push_back(language_suggestions);
            language++;
          }
          break;
      }
    }

    // If |*misspelling_len| is non-zero, that means at least one language
    // marked a word misspelled and no other language considered it correct.
    if (*misspelling_len != 0) {
      if (optional_per_language_suggestions) {
        optional_per_language_suggestions->swap(suggestions_list);
      }
      return false;
    }

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    // If we're performing a hybrid spell check, we're only interested in
    // knowing whether some Hunspell languages considered this text range as
    // correctly spelled. If no misspellings were found, but the entire text was
    // skipped, it means that no Hunspell language considered this text
    // correct, so we should return false here.
    if (spellcheck::UseBrowserSpellChecker() &&
        EnabledLanguageCount() != LanguageCount() &&
        agreed_skippable_len == text_length) {
      return false;
    }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  }

  NOTREACHED_IN_MIGRATION();
  return true;
}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
bool SpellCheck::SpellCheckParagraph(
    const std::u16string& text,
    spellcheck::mojom::SpellCheckHost& host,
    WebVector<WebTextCheckingResult>* results) {
  DCHECK(results);
  std::vector<WebTextCheckingResult> textcheck_results;
  const size_t text_length = text.length();
  size_t position_in_text = 0;

  // Spellcheck::SpellCheckWord() automatically breaks text into words and
  // checks the spellings of the extracted words. This function sets the
  // position and length of the first misspelled word and returns false when
  // the text includes misspelled words. Therefore, we just repeat calling the
  // function until it returns true to check the whole text.
  size_t misspelling_start = 0;
  size_t misspelling_length = 0;
  while (position_in_text <= text_length) {
    size_t misspelling_start_relative_to_substring = 0;
    bool spelled_correctly = SpellCheckWord(
        text.substr(position_in_text), host,
        &misspelling_start_relative_to_substring, &misspelling_length, nullptr);
    // SpellCheckWord informs us of a misspelling index relative to the
    // substring of text which was passed in, so add `position_in_text` to this
    // value to get the offset relative to `text`.
    misspelling_start =
        misspelling_length > 0
            ? position_in_text + misspelling_start_relative_to_substring
            : 0;
    if (spelled_correctly) {
      results->Assign(textcheck_results);
      return true;
    }

    if (!custom_dictionary_.SpellCheckWord(text, misspelling_start,
                                           misspelling_length)) {
      textcheck_results.push_back(
          WebTextCheckingResult(blink::kWebTextDecorationTypeSpelling,
                                base::checked_cast<int>(misspelling_start),
                                base::checked_cast<int>(misspelling_length)));
    }
    position_in_text = misspelling_start + misspelling_length;
  }
  results->Assign(textcheck_results);
  return false;
}

void SpellCheck::RequestTextChecking(
    const std::u16string& text,
    std::unique_ptr<blink::WebTextCheckingCompletion> completion,
    base::WeakPtr<SpellCheckProvider> provider) {
  // Clean up the previous request before starting a new request.
  if (pending_request_param_)
    pending_request_param_->completion()->DidCancelCheckingText();

  pending_request_param_ = std::make_unique<SpellcheckRequest>(
      text, std::move(completion), std::move(provider));
  // We will check this text after we finish loading the hunspell dictionary.
  if (InitializeIfNeeded())
    return;

  PostDelayedSpellCheckTask(pending_request_param_.release());
}
#endif

bool SpellCheck::InitializeIfNeeded() {
  if (languages_.empty())
    return true;

  bool initialize_if_needed = false;
  for (auto& language : languages_)
    initialize_if_needed |= language->InitializeIfNeeded();

  return initialize_if_needed;
}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void SpellCheck::PostDelayedSpellCheckTask(SpellcheckRequest* request) {
  if (!request)
    return;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SpellCheck::PerformSpellCheck, weak_factory_.GetWeakPtr(),
                     base::Owned(request)));
}
#endif

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void SpellCheck::PerformSpellCheck(SpellcheckRequest* param) {
  DCHECK(param);

  spellcheck::mojom::SpellCheckHost* host = nullptr;
  if (SpellCheckProvider* provider = param->provider()) {
    // It is safe to provide this as a pointer here because
    // it will only be used synchronously in the SpellCheckParagraph
    // method.
    host = &provider->GetSpellCheckHost();
  }

  if (!host || languages_.empty() ||
      !base::ranges::all_of(languages_, &SpellcheckLanguage::IsEnabled)) {
    param->completion()->DidCancelCheckingText();
  } else {
    WebVector<blink::WebTextCheckingResult> results;
    SpellCheckParagraph(param->text(), *host, &results);
    param->completion()->DidFinishCheckingText(results);
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    spellcheck_renderer_metrics::RecordSpellcheckDuration(
        base::TimeTicks::Now() - param->start_ticks(),
        /*used_hunspell=*/true, /*used_native=*/false);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  }
}
#endif

void SpellCheck::CreateTextCheckingResults(
    ResultFilter filter,
    spellcheck::mojom::SpellCheckHost& host,
    int line_offset,
    const std::u16string& line_text,
    const std::vector<SpellCheckResult>& spellcheck_results,
    WebVector<WebTextCheckingResult>* textcheck_results) {
  DCHECK(!line_text.empty());

  std::vector<WebTextCheckingResult> results;
  for (const SpellCheckResult& spellcheck_result : spellcheck_results) {
    DCHECK_LE(static_cast<size_t>(spellcheck_result.location),
              line_text.length());
    DCHECK_LE(static_cast<size_t>(spellcheck_result.location +
                                  spellcheck_result.length),
              line_text.length());

    const std::u16string& misspelled_word =
        line_text.substr(spellcheck_result.location, spellcheck_result.length);
    const std::vector<std::u16string>& replacements =
        spellcheck_result.replacements;
    SpellCheckResult::Decoration decoration = spellcheck_result.decoration;

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    // Ignore words that are in a script not supported by any of the enabled
    // spellcheck languages.
    if (spellcheck::UseBrowserSpellChecker() &&
        !IsWordInSupportedScript(misspelled_word)) {
      continue;
    }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

    // Ignore words in custom dictionary.
    if (custom_dictionary_.SpellCheckWord(misspelled_word, 0,
                                          misspelled_word.length())) {
      continue;
    }

    std::vector<WebString> replacements_filtered =
        FilterReplacementSuggestions(misspelled_word, replacements);

    // If the spellchecker suggested replacements, but they were all just
    // changing apostrophe styles, ignore this misspelling. If there were never
    // any suggested replacements, keep the misspelling.
    if (replacements_filtered.empty() && !replacements.empty())
      continue;

    if (filter == USE_HUNSPELL_FOR_GRAMMAR) {
      // Double-check misspelled words with Hunspell and attach grammar markers
      // to them if Hunspell tells us they are correct words, i.e. they are
      // probably contextually-misspelled words.
      size_t unused_misspelling_start = 0;
      size_t unused_misspelling_length = 0;
      if (decoration == SpellCheckResult::SPELLING &&
          SpellCheckWord(misspelled_word, host, &unused_misspelling_start,
                         &unused_misspelling_length, nullptr)) {
        decoration = SpellCheckResult::GRAMMAR;
      }
    }
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    else if (filter == USE_HUNSPELL_FOR_HYBRID_CHECK &&
             spellcheck::UseBrowserSpellChecker() &&
             EnabledLanguageCount() > 0) {
      // Remove the suggestions that were generated by the native spell checker,
      // otherwise Blink will cache them without asking for the suggestions
      // from Hunspell.
      replacements_filtered.clear();

      // The native spell checker was not able to check all locales. Double-
      // check misspelled words with Hunspell for the unchecked locales
      // and remove the results if Hunspell tells us the words are correctly
      // spelled in those locales.
      size_t unused_misspelling_start = 0;
      size_t unused_misspelling_length = 0;

      if (SpellCheckWord(misspelled_word, host, &unused_misspelling_start,
                         &unused_misspelling_length, nullptr)) {
        // Correctly spelled in a Hunspell locale. If enhanced spell check was
        // used, turn the spelling mistake into a grammar mistake (local and
        // remote checks disagree, so the word is probably only contextually
        // misspelled). If enhanced spell check wasn't used, remove this
        // misspelling.
        if (spellcheck_result.spelling_service_used) {
          decoration = SpellCheckResult::GRAMMAR;
        } else {
          continue;
        }
      }
    }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

    results.push_back(
        WebTextCheckingResult(static_cast<WebTextDecorationType>(decoration),
                              line_offset + spellcheck_result.location,
                              spellcheck_result.length, replacements_filtered));
  }

  textcheck_results->Assign(results);
}

bool SpellCheck::IsSpellcheckEnabled() {
#if BUILDFLAG(IS_ANDROID)
  if (!spellcheck::IsAndroidSpellCheckFeatureEnabled()) return false;
#endif
  return spellcheck_enabled_;
}

void SpellCheck::AddDictionaryUpdateObserver(
    DictionaryUpdateObserver* observer) {
  return dictionary_update_observers_.AddObserver(observer);
}

void SpellCheck::RemoveDictionaryUpdateObserver(
    DictionaryUpdateObserver* observer) {
  return dictionary_update_observers_.RemoveObserver(observer);
}

size_t SpellCheck::LanguageCount() {
  return languages_.size();
}

size_t SpellCheck::EnabledLanguageCount() {
  return base::ranges::count_if(languages_, &SpellcheckLanguage::IsEnabled);
}

void SpellCheck::NotifyDictionaryObservers(
    const WebVector<WebString>& words_added) {
  for (auto& observer : dictionary_update_observers_) {
    observer.OnDictionaryUpdated(words_added);
  }
}

bool SpellCheck::IsWordInSupportedScript(const std::u16string& word) const {
  return base::ranges::any_of(languages_, [word](const auto& language) {
    return language->IsTextInSameScript(word);
  });
}
