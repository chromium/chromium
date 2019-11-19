// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/spellcheck_language.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"
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
const int kNoOffset = 0;
const int kNoTag = 0;

class UpdateSpellcheckEnabled : public content::RenderFrameVisitor {
 public:
  explicit UpdateSpellcheckEnabled(bool enabled) : enabled_(enabled) {}
  bool Visit(content::RenderFrame* render_frame) override;

 private:
  bool enabled_;  // New spellcheck-enabled state.
  DISALLOW_COPY_AND_ASSIGN(UpdateSpellcheckEnabled);
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
  std::transform(words.begin(), words.end(), result.begin(),
                 [](const std::string& w) { return WebString::FromUTF8(w); });
  return result;
}

bool IsApostrophe(base::char16 c) {
  const base::char16 kApostrophe = 0x27;
  const base::char16 kRightSingleQuotationMark = 0x2019;
  return c == kApostrophe || c == kRightSingleQuotationMark;
}

// Makes sure that the apostrophes in the |spelling_suggestion| are the same
// type as in the |misspelled_word| and in the same order. Ignore differences in
// the number of apostrophes.
void PreserveOriginalApostropheTypes(const base::string16& misspelled_word,
                                     base::string16* spelling_suggestion) {
  auto it = spelling_suggestion->begin();
  for (const base::char16& c : misspelled_word) {
    if (IsApostrophe(c)) {
      it = std::find_if(it, spelling_suggestion->end(), IsApostrophe);
      if (it == spelling_suggestion->end())
        return;

      *it++ = c;
    }
  }
}

std::vector<WebString> FilterReplacementSuggestions(
    const base::string16& misspelled_word,
    const std::vector<base::string16>& replacements) {
  std::vector<WebString> replacements_filtered;
  for (base::string16 replacement : replacements) {
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
      const base::string16& text,
      std::unique_ptr<blink::WebTextCheckingCompletion> completion)
      : text_(text), completion_(std::move(completion)) {
    DCHECK(completion_);
  }
  ~SpellcheckRequest() {}

  base::string16 text() { return text_; }
  blink::WebTextCheckingCompletion* completion() { return completion_.get(); }

 private:
  base::string16 text_;  // Text to be checked in this task.

  // The interface to send the misspelled ranges to WebKit.
  std::unique_ptr<blink::WebTextCheckingCompletion> completion_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckRequest);
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

void SpellCheck::FillSuggestions(
    const std::vector<std::vector<base::string16>>& suggestions_list,
    std::vector<base::string16>* optional_suggestions) {
  DCHECK(optional_suggestions);
  size_t num_languages = suggestions_list.size();

  // Compute maximum number of suggestions in a single language.
  size_t max_suggestions = 0;
  for (const auto& suggestions : suggestions_list)
    max_suggestions = std::max(max_suggestions, suggestions.size());

  for (size_t count = 0; count < (max_suggestions * num_languages); ++count) {
    size_t language = count % num_languages;
    size_t index = count / num_languages;

    if (suggestions_list[language].size() <= index)
      continue;

    const base::string16& suggestion = suggestions_list[language][index];
    // Only add the suggestion if it's unique.
    if (!base::Contains(*optional_suggestions, suggestion)) {
      optional_suggestions->push_back(suggestion);
    }
    if (optional_suggestions->size() >= spellcheck::kMaxSuggestions) {
      break;
    }
  }
}

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

bool SpellCheck::SpellCheckWord(
    const base::char16* text_begin,
    size_t position_in_text,
    size_t text_length,
    int tag,
    size_t* misspelling_start,
    size_t* misspelling_len,
    std::vector<base::string16>* optional_suggestions) {
  DCHECK(text_length >= position_in_text);
  DCHECK(misspelling_start && misspelling_len) << "Out vars must be given.";

  // Do nothing if we need to delay initialization. (Rather than blocking,
  // report the word as correctly spelled.)
  if (InitializeIfNeeded())
    return true;

  // These are for holding misspelling or skippable word positions and lengths
  // between calls to SpellcheckLanguage::SpellCheckWord.
  size_t possible_misspelling_start;
  size_t possible_misspelling_len;
  // The longest sequence of text that all languages agree is skippable.
  size_t agreed_skippable_len;
  // A vector of vectors containing spelling suggestions from different
  // languages.
  std::vector<std::vector<base::string16>> suggestions_list;
  // A vector to hold a language's misspelling suggestions between spellcheck
  // calls.
  std::vector<base::string16> language_suggestions;

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
      language_suggestions.clear();
      SpellcheckLanguage::SpellcheckWordResult result =
          (*language)->SpellCheckWord(
              text_begin, position_in_text, text_length, tag,
              &possible_misspelling_start, &possible_misspelling_len,
              optional_suggestions ? &language_suggestions : nullptr);

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
      if (optional_suggestions)
        FillSuggestions(suggestions_list, optional_suggestions);
      return false;
    }
  }

  NOTREACHED();
  return true;
}

bool SpellCheck::SpellCheckParagraph(
    const base::string16& text,
    WebVector<WebTextCheckingResult>* results) {
#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  if (!spellcheck::UseBrowserSpellChecker()) {
    DCHECK(results);
    std::vector<WebTextCheckingResult> textcheck_results;
    size_t length = text.length();
    size_t position_in_text = 0;

    // Spellcheck::SpellCheckWord() automatically breaks text into words and
    // checks the spellings of the extracted words. This function sets the
    // position and length of the first misspelled word and returns false when
    // the text includes misspelled words. Therefore, we just repeat calling the
    // function until it returns true to check the whole text.
    size_t misspelling_start = 0;
    size_t misspelling_length = 0;
    while (position_in_text <= length) {
      if (SpellCheckWord(text.c_str(), position_in_text, length, kNoTag,
                         &misspelling_start, &misspelling_length, nullptr)) {
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
#endif

  // This function is only invoked if renderer(hunspell) spellchecker is used.
  DCHECK(spellcheck::UseBrowserSpellChecker());
  NOTREACHED();
  return true;
}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void SpellCheck::RequestTextChecking(
    const base::string16& text,
    std::unique_ptr<blink::WebTextCheckingCompletion> completion) {
  // Clean up the previous request before starting a new request.
  if (pending_request_param_)
    pending_request_param_->completion()->DidCancelCheckingText();

  pending_request_param_.reset(
      new SpellcheckRequest(text, std::move(completion)));
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

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SpellCheck::PerformSpellCheck, AsWeakPtr(),
                                base::Owned(request)));
}
#endif

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void SpellCheck::PerformSpellCheck(SpellcheckRequest* param) {
  DCHECK(param);

  if (languages_.empty() ||
      std::find_if(languages_.begin(), languages_.end(),
                   [](std::unique_ptr<SpellcheckLanguage>& language) {
                     return !language->IsEnabled();
                   }) != languages_.end()) {
    param->completion()->DidCancelCheckingText();
  } else {
    WebVector<blink::WebTextCheckingResult> results;
    SpellCheckParagraph(param->text(), &results);
    param->completion()->DidFinishCheckingText(results);
  }
}
#endif

void SpellCheck::CreateTextCheckingResults(
    ResultFilter filter,
    int line_offset,
    const base::string16& line_text,
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

    const base::string16& misspelled_word =
        line_text.substr(spellcheck_result.location, spellcheck_result.length);
    const std::vector<base::string16>& replacements =
        spellcheck_result.replacements;
    SpellCheckResult::Decoration decoration = spellcheck_result.decoration;

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

    if (filter == USE_NATIVE_CHECKER) {
      // Double-check misspelled words with out spellchecker and attach grammar
      // markers to them if our spellchecker tells us they are correct words,
      // i.e. they are probably contextually-misspelled words.
      size_t unused_misspelling_start = 0;
      size_t unused_misspelling_length = 0;
      if (decoration == SpellCheckResult::SPELLING &&
          SpellCheckWord(misspelled_word.c_str(), kNoOffset,
                         misspelled_word.length(), kNoTag,
                         &unused_misspelling_start, &unused_misspelling_length,
                         nullptr)) {
        decoration = SpellCheckResult::GRAMMAR;
      }
    }

    results.push_back(
        WebTextCheckingResult(static_cast<WebTextDecorationType>(decoration),
                              line_offset + spellcheck_result.location,
                              spellcheck_result.length, replacements_filtered));
  }

  textcheck_results->Assign(results);
}

bool SpellCheck::IsSpellcheckEnabled() {
#if defined(OS_ANDROID)
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

void SpellCheck::NotifyDictionaryObservers(
    const WebVector<WebString>& words_added) {
  for (auto& observer : dictionary_update_observers_) {
    observer.OnDictionaryUpdated(words_added);
  }
}
