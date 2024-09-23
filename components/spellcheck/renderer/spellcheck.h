// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_H_

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/renderer/custom_dictionary_engine.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class SpellcheckLanguage;
class SpellCheckProvider;
struct SpellCheckResult;

namespace blink {
class WebTextCheckingCompletion;
struct WebTextCheckingResult;
template <typename T> class WebVector;
class WebString;
}

namespace service_manager {
class LocalInterfaceProvider;
}

class DictionaryUpdateObserver {
 public:
  virtual ~DictionaryUpdateObserver() = default;
  // |words_added| is newly added words to dictionary as correct words.
  // OnDictionaryUpdated should be called even if |words_added| empty.
  virtual void OnDictionaryUpdated(
      const blink::WebVector<blink::WebString>& words_added) = 0;
};

// TODO(morrita): Needs reorg with SpellCheckProvider.
// See http://crbug.com/73699.
// Shared spellchecking logic/data for a RenderProcess. All RenderViews use
// this object to perform spellchecking tasks.
class SpellCheck : public spellcheck::mojom::SpellChecker {
 public:
  // TODO(groby): I wonder if this can be private, non-mac only.
  class SpellcheckRequest;
  enum ResultFilter {
    // Do not modify results.
    DO_NOT_MODIFY = 1,
    // Use Hunspell to double-check the results from the spelling service
    // (enhanced spell check). If Hunspell doesn't find a mistake, it most
    // likely means it was a grammar mistake, not a spelling mistake.
    USE_HUNSPELL_FOR_GRAMMAR,
    // Use Hunspell to double-check the results from the native spell checker
    // for locales that it doesn't support. If Hunspell doesn't find a mistake,
    // it means the misspelling was a false positive, since the word is
    // correctly spelled in at least one locale.
    USE_HUNSPELL_FOR_HYBRID_CHECK,
  };

  explicit SpellCheck(
      service_manager::LocalInterfaceProvider* embedder_provider);

  SpellCheck(const SpellCheck&) = delete;
  SpellCheck& operator=(const SpellCheck&) = delete;

  ~SpellCheck() override;

  void AddSpellcheckLanguage(base::File file, const std::string& language);

  // If there are no dictionary files, then this requests them from the browser
  // and does not block. In this case it returns true.
  // If there are dictionary files, but their Hunspell has not been loaded, then
  // this loads their Hunspell.
  // If each dictionary file's Hunspell is already loaded, this does nothing. In
  // both the latter cases it returns false, meaning that it is OK to continue
  // spellchecking.
  bool InitializeIfNeeded();

  // SpellCheck a word.
  // Returns true if spelled correctly for any language in |languages_|, false
  // otherwise.
  // If any spellcheck languages failed to initialize, always returns true.
  // In addition, finds the suggested words for a given word
  // and puts them into |*optional_suggestions|.
  // If the word is spelled correctly, the vector is empty.
  // If optional_suggestions is NULL, suggested words will not be looked up.
  // Note that doing suggest lookups can be slow.
  bool SpellCheckWord(std::u16string_view text,
                      spellcheck::mojom::SpellCheckHost& host,
                      size_t* misspelling_start,
                      size_t* misspelling_len,
                      std::vector<std::u16string>* optional_suggestions);

  // Overload of SpellCheckWord where the replacement suggestions are kept
  // separately per language, instead of combined into a single list. This is
  // useful if the suggestions must be merged with another list of suggestions,
  // for example in the case of the Windows hybrid spellchecker.
  bool SpellCheckWord(
      std::u16string_view text,
      spellcheck::mojom::SpellCheckHost& host,
      size_t* misspelling_start,
      size_t* misspelling_len,
      spellcheck::PerLanguageSuggestions* optional_per_language_suggestions);

  // Overload of SpellCheckWord for skipping optional suggestions with a
  // nullptr, used to disambiguate between the other two overloads.
  bool SpellCheckWord(std::u16string_view text,
                      spellcheck::mojom::SpellCheckHost& host,
                      size_t* misspelling_start,
                      size_t* misspelling_len,
                      std::nullptr_t null_suggestions_ptr);

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  // SpellCheck a paragraph.
  // Returns true if |text| is correctly spelled, false otherwise.
  // If the spellchecker failed to initialize, always returns true.
  bool SpellCheckParagraph(
      const std::u16string& text,
      spellcheck::mojom::SpellCheckHost& host,
      blink::WebVector<blink::WebTextCheckingResult>* results);

  // Requests to spellcheck the specified text in the background. This function
  // posts a background task and calls SpellCheckParagraph() in the task.
  void RequestTextChecking(
      const std::u16string& text,
      std::unique_ptr<blink::WebTextCheckingCompletion> completion,
      base::WeakPtr<SpellCheckProvider> provider);
#endif

  // Creates a list of WebTextCheckingResult objects (used by WebKit) from a
  // list of SpellCheckResult objects (used by Chrome). This function also
  // checks misspelled words returned by the Spelling service and changes the
  // underline colors of contextually-misspelled words.
  void CreateTextCheckingResults(
      ResultFilter filter,
      spellcheck::mojom::SpellCheckHost& host,
      int line_offset,
      const std::u16string& line_text,
      const std::vector<SpellCheckResult>& spellcheck_results,
      blink::WebVector<blink::WebTextCheckingResult>* textcheck_results);

  bool IsSpellcheckEnabled();

  // Add observer on dictionary update event.
  void AddDictionaryUpdateObserver(DictionaryUpdateObserver* observer);
  // Remove observer on dictionary update event.
  void RemoveDictionaryUpdateObserver(DictionaryUpdateObserver* observer);

  // Binds receivers for the SpellChecker interface.
  void BindReceiver(
      mojo::PendingReceiver<spellcheck::mojom::SpellChecker> receiver);

  // Returns the current number of spell check languages.
  // Overridden by tests in spellcheck_provider_test.cc (FakeSpellCheck class).
  virtual size_t LanguageCount();

  // Returns the current number of spell check languages with enabled engines.
  // Overridden by tests in spellcheck_provider_test.cc (FakeSpellCheck class).
  virtual size_t EnabledLanguageCount();

  // spellcheck::mojom::SpellChecker:
  // Initialize the SpellCheck object with data provided by the browser process.
  // Method is public since called directly in
  // SpellCheckProvider::OnRespondInitializeDictionaries.
  void Initialize(
      std::vector<spellcheck::mojom::SpellCheckBDictLanguagePtr> dictionaries,
      const std::vector<std::string>& custom_words,
      bool enable) override;

 private:
   friend class SpellCheckTest;
   friend class FakeSpellCheck;
   FRIEND_TEST_ALL_PREFIXES(SpellCheckTest, GetAutoCorrectionWord_EN_US);
   FRIEND_TEST_ALL_PREFIXES(SpellCheckTest,
       RequestSpellCheckMultipleTimesWithoutInitialization);

   // spellcheck::mojom::SpellChecker:
   void CustomDictionaryChanged(
       const std::vector<std::string>& words_added,
       const std::vector<std::string>& words_removed) override;

   // Performs dictionary update notification.
   void NotifyDictionaryObservers(
       const blink::WebVector<blink::WebString>& words_added);

   // Returns whether a word is in the script of one of the enabled spellcheck
   // languages.
   bool IsWordInSupportedScript(const std::u16string& word) const;

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
   // Posts delayed spellcheck task and clear it if any.
   // Takes ownership of |request|.
   void PostDelayedSpellCheckTask(SpellcheckRequest* request);

   // Performs spell checking from the request queue.
   void PerformSpellCheck(SpellcheckRequest* request);

   // The parameters of a pending background-spellchecking request. When WebKit
   // sends a background-spellchecking request before initializing hunspell,
   // we save its parameters and start spellchecking after we finish
   // initializing hunspell. (When WebKit sends two or more requests, we cancel
   // the previous requests so we do not have to use vectors.)
   std::unique_ptr<SpellcheckRequest> pending_request_param_;
#endif

  // Receivers for SpellChecker clients.
  mojo::ReceiverSet<spellcheck::mojom::SpellChecker> receivers_;

  // A vector of objects used to actually check spelling, one for each enabled
  // language.
  std::vector<std::unique_ptr<SpellcheckLanguage>> languages_;

  // Custom dictionary spelling engine.
  CustomDictionaryEngine custom_dictionary_;

  raw_ptr<service_manager::LocalInterfaceProvider> embedder_provider_;

  // Remember state for spellchecking.
  bool spellcheck_enabled_;

  // Observers of update dictionary events.
  base::ObserverList<DictionaryUpdateObserver>::Unchecked
      dictionary_update_observers_;

  base::WeakPtrFactory<SpellCheck> weak_factory_{this};
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_H_
