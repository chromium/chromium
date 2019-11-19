// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MANAGER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/translate_errors.h"

namespace language {
class LanguageModel;
}  // namespace language

namespace metrics {
class TranslateEventProto;
}  // namespace metrics

namespace translate {

class TranslateClient;
class TranslateDriver;
class TranslatePrefs;
class TranslateRanker;
struct TranslateTriggerDecision;

namespace testing {
class TranslateManagerTest;
}  // namespace testing

struct TranslateErrorDetails;
struct TranslateInitDetails;

// The TranslateManager class is responsible for showing an info-bar when a page
// in a language different than the user language is loaded.  It triggers the
// page translation the user requests.

class TranslateManager {
 public:
  // |translate_client| is expected to outlive the TranslateManager.
  TranslateManager(TranslateClient* translate_client,
                   TranslateRanker* translate_ranker,
                   language::LanguageModel* language_model);
  virtual ~TranslateManager();

  // Returns a weak pointer to this instance.
  base::WeakPtr<TranslateManager> GetWeakPtr();

  // Cannot return NULL.
  TranslateClient* translate_client() { return translate_client_; }

  // Sets the sequence number of the current page, for use while sending
  // messages to the renderer.
  void set_current_seq_no(int page_seq_no) { page_seq_no_ = page_seq_no; }

  metrics::TranslateEventProto* mutable_translate_event() {
    return translate_event_.get();
  }

  // Returns the language to translate to.
  //
  // If provided a non-null |language_model|, returns the first language from
  // the model that is supported by the translation service.
  //
  // Otherwise, returns the first language found in the following list that is
  // supported by the translation service:
  //     High confidence and high probability reading language in ULP
  //     the UI language
  //     the accept-language list
  //
  // If no language is found then an empty string is returned.
  static std::string GetTargetLanguage(const TranslatePrefs* prefs,
                                       language::LanguageModel* language_model);

  // Returns the language to translate to using the same logic as
  // GetTargetLanguage but doesn't returned languages contained in
  // |skipped_languages| if |language_model| is not null and there is at least
  // one other suitable language.
  static std::string GetTargetLanguage(
      const TranslatePrefs* prefs,
      language::LanguageModel* language_model,
      const std::set<std::string>& skipped_languages);

  // Returns the language to automatically translate to. |original_language| is
  // the webpage's original language.
  static std::string GetAutoTargetLanguage(const std::string& original_language,
                                           TranslatePrefs* translate_prefs);

  // Returns the target language for a manually triggered translation: the
  // output of GetTargetLanguage if the page hasn't been translated yet,
  // otherwise the page's current language.
  static std::string GetManualTargetLanguage(
      const std::string& source_code,
      const LanguageState& language_state,
      translate::TranslatePrefs* prefs,
      language::LanguageModel* language_model);

  // Translates the page contents from |source_lang| to |target_lang|.
  // The actual translation might be performed asynchronously if the translate
  // script is not yet available.
  void TranslatePage(const std::string& source_lang,
                     const std::string& target_lang,
                     bool triggered_from_menu);

  // Starts the translation process for the page in the |page_lang| language.
  void InitiateTranslation(const std::string& page_lang);

  // Initiate a manually triggered translation process for the current page.
  // Collect source and target languages, and show translation UI. If
  // |auto_translate| is true the page gets translated to the target language.
  void InitiateManualTranslation(bool auto_translate = false);

  // Returns true iff the current page could be manually translated.
  bool CanManuallyTranslate();

  // Shows the after translate or error infobar depending on the details.
  void PageTranslated(const std::string& source_lang,
                      const std::string& target_lang,
                      TranslateErrors::Type error_type);

  // Reverts the contents of the page to its original language.
  void RevertTranslation();

  // Reports to the Google translate server that a page language was incorrectly
  // detected.  This call is initiated by the user selecting the "report" menu
  // under options in the translate infobar.
  void ReportLanguageDetectionError();

  // Callback types for translate errors.
  typedef base::RepeatingCallback<void(const TranslateErrorDetails&)>
      TranslateErrorCallback;
  typedef base::CallbackList<void(const TranslateErrorDetails&)>
      TranslateErrorCallbackList;

  // Callback types for translate initialization.
  typedef base::RepeatingCallback<void(const TranslateInitDetails&)>
      TranslateInitCallback;
  typedef base::CallbackList<void(const TranslateInitDetails&)>
      TranslateInitCallbackList;

  // Registers a callback for translate errors.
  static std::unique_ptr<TranslateErrorCallbackList::Subscription>
  RegisterTranslateErrorCallback(const TranslateErrorCallback& callback);

  // Registers a callback for translate initialization.
  static std::unique_ptr<TranslateInitCallbackList::Subscription>
  RegisterTranslateInitCallback(const TranslateInitCallback& callback);

  // Gets the LanguageState associated with the TranslateManager
  LanguageState& GetLanguageState();

  // Record an event of the given |event_type| using the currently saved
  // |translate_event_| as context. |event_type| must be one of the values
  // defined by metrics::TranslateEventProto::EventType.
  void RecordTranslateEvent(int event_type);

  // By default, don't offer to translate in builds lacking an API key. For
  // testing, set to true to offer anyway.
  static void SetIgnoreMissingKeyForTesting(bool ignore);

  // Returns true if the TranslateManager is available and enabled by user
  // preferences. It is not available for builds without API keys.
  // blink's hrefTranslate attribute existence relies on the result.
  // See https://github.com/dtapuska/html-translate
  static bool IsAvailable(const TranslatePrefs* prefs);

  // Returns true if the decision should be overridden and logs the event
  // appropriately. |event_type| must be one of the
  // values defined by metrics::TranslateEventProto::EventType.
  bool ShouldOverrideDecision(int event_type);

  // Returns true if the BubbleUI should be suppressed.
  bool ShouldSuppressBubbleUI(bool triggered_from_menu,
                              const std::string& source_language);

  // Sets target language.
  void SetPredefinedTargetLanguage(const std::string& language_code);

 private:
  friend class translate::testing::TranslateManagerTest;

  // Sends a translation request to the TranslateDriver.
  void DoTranslatePage(const std::string& translate_script,
                       const std::string& source_lang,
                       const std::string& target_lang);

  // Notifies all registered callbacks of translate errors.
  void NotifyTranslateError(TranslateErrors::Type error_type);

  // Notifies all registered callbacks of translate initialization.
  void NotifyTranslateInit(std::string page_language_code,
                           std::string target_lang,
                           TranslateTriggerDecision decision,
                           bool ui_shown);

  // Called when the Translate script has been fetched.
  // Initiates the translation.
  void OnTranslateScriptFetchComplete(const std::string& source_lang,
                                      const std::string& target_lang,
                                      bool success,
                                      const std::string& data);

  // Helper function to initialize a translate event metric proto.
  void InitTranslateEvent(const std::string& src_lang,
                          const std::string& dst_lang,
                          const TranslatePrefs& translate_prefs);

  void AddTargetLanguageToAcceptLanguages(
      const std::string& target_language_code);

  // Creates a TranslateTriggerDecision and filters out possible outcomes based
  // on the current state. Returns a decision objects ready to be used to
  // trigger behavior and record metrics.
  const TranslateTriggerDecision ComputePossibleOutcomes(
      TranslatePrefs* translate_prefs,
      const std::string& page_language_code,
      const std::string& target_lang);

  // Determines whether translation is even possible (connected to the internet,
  // source and target languages don't match, etc) and mutates |decision| based
  // on the result.
  void FilterIsTranslatePossible(TranslateTriggerDecision* decision,
                                 TranslatePrefs* translate_prefs,
                                 const std::string& page_language_code,
                                 const std::string& target_lang);

  // Determines whether auto-translate is a possible outcome, and mutates
  // |decision| accordingly.
  void FilterAutoTranslate(TranslateTriggerDecision* decision,
                           TranslatePrefs* translate_prefs,
                           const std::string& page_language_code);

  // Determines whether user prefs prohibit translations for this specific
  // navigation. For example, a user can select "never translate this language".
  // Mutates |decision| accordingly.
  void FilterForUserPrefs(TranslateTriggerDecision* decision,
                          TranslatePrefs* translate_prefs,
                          const std::string& page_language_code);

  // Determines if either auto-translation or showing the UI is supported for
  // the current navigation's hrefTranslate attribute. Writes the results to
  // |decision|.
  void FilterForHrefTranslate(TranslateTriggerDecision* decision,
                              TranslatePrefs* translate_prefs,
                              const std::string& page_language_code);

  // Determines if showing the UI is supported for the predefined target
  // language which was set via SetPredefinedTargetLanguage call.
  // Writes the results to |decision|.
  void FilterForPredefinedTarget(TranslateTriggerDecision* decision,
                                 TranslatePrefs* translate_prefs,
                                 const std::string& page_language_code);

  // Check whether there is specified target, the source and
  // the target are both supported, and the source and target don't match.
  bool IsTranslatableLanguagePair(const std::string& page_language_code,
                                  const std::string& target_language_code);

  // Enables or disables the translate omnibox icon depending on |decision|. The
  // icon is always shown if translate UI is shown, auto-translation happens, or
  // the UI is suppressed by ranker.
  void MaybeShowOmniboxIcon(const TranslateTriggerDecision& decision);

  // Shows the UI or auto-translates based on the state of |decision|. Returns
  // true if UI was shown, false otherwise.
  bool MaterializeDecision(const TranslateTriggerDecision& decision,
                           TranslatePrefs* translate_prefs,
                           const std::string& page_language_code,
                           const std::string target_lang);

  // Records all UMA metrics related to the current |decision|.
  void RecordDecisionMetrics(const TranslateTriggerDecision& decision,
                             const std::string& page_language_code,
                             bool ui_shown);

  // Records the RankerEvent associated with the current |decision|.
  void RecordDecisionRankerEvent(const TranslateTriggerDecision& decision,
                                 TranslatePrefs* translate_prefs,
                                 const std::string& page_language_code,
                                 const std::string& target_lang);

  // Sequence number of the current page.
  int page_seq_no_;

  // Preference name for the Accept-Languages HTTP header.
  std::string accept_languages_pref_name_;

  TranslateClient* translate_client_;        // Weak.
  TranslateDriver* translate_driver_;        // Weak.
  TranslateRanker* translate_ranker_;        // Weak.
  language::LanguageModel* language_model_;  // Weak.

  LanguageState language_state_;

  std::unique_ptr<metrics::TranslateEventProto> translate_event_;

  base::WeakPtrFactory<TranslateManager> weak_method_factory_{this};

  // By default, don't offer to translate in builds lacking an API key. For
  // testing, set to true to offer anyway.
  static bool ignore_missing_key_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManager);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MANAGER_H_
