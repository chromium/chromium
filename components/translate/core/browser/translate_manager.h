// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MANAGER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/common/translate_constants.h"
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

class NullTranslateMetricsLogger;

namespace testing {
class TranslateManagerTest;
}  // namespace testing

struct LanguageDetectionDetails;
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

  TranslateManager(const TranslateManager&) = delete;
  TranslateManager& operator=(const TranslateManager&) = delete;

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

  // Returns the target language to show in the UI representing the current
  // page state.
  //
  // On a translated page it will be the language that a page is translated to.
  // On a non translated page it will be the result of |GetTargetlanguage|.
  std::string GetTargetLanguageForDisplay(
      TranslatePrefs* prefs,
      language::LanguageModel* language_model);

  // Returns the language to translate to.
  //
  // If provided a non-undefined |source_language|, returns the language from
  // the auto translate list (if not empty and it supports translate).
  //
  // If the recent target language is not empty and supports translate returns
  // that.
  //
  // If provided a non-null |language_model|, returns the first language from
  // the model that is supported by the translation service and that is not to
  // be skipped due to any ongoing experiment (see
  // |GetSkippedLanguagesForExperiments|).
  //
  // Otherwise, return the first language on the accept-language list that is
  // supported by the translation service
  //
  // If no language is found then "en" is returned.
  static std::string GetTargetLanguage(
      TranslatePrefs* prefs,
      language::LanguageModel* language_model,
      const std::string source_lang_code = translate::kUnknownLanguageCode);

  // Returns the language to automatically translate to. |source_language| is
  // the webpage's source language.
  static std::string GetAutoTargetLanguage(const std::string& source_language,
                                           TranslatePrefs* translate_prefs);

  // Translates the page contents from |source_lang| to |target_lang|.
  // The actual translation might be performed asynchronously if the translate
  // script is not yet available.
  void TranslatePage(
      const std::string& source_lang,
      const std::string& target_lang,
      bool triggered_from_menu,
      TranslationType translate_type = TranslationType::kUninitialized);

  // Starts the translation process for the page in the |page_lang| language.
  void InitiateTranslation(const std::string& page_lang);

  // Show the translation UI with the target language enforced to |target_lang|.
  // If |auto_translate| is true the page gets translated to the target
  // language.
  void ShowTranslateUI(const std::string& target_lang,
                       bool auto_translate = false,
                       bool triggered_from_menu = false);

  // Show the translation UI. If |auto_translate| is true the page gets
  // translated to the target language.
  void ShowTranslateUI(bool auto_translate = false,
                       bool triggered_from_menu = false);

  // Returns true iff the current page could be manually translated.
  // Logging should only be performed when this method is called to show the
  // Full Page Translate menu item.
  bool CanManuallyTranslate(bool menuLogging = false);

  // Whether or not partial translation is supported for the current target
  // language. Partial translate supports a subset of translation languages,
  // but shares a target language with full page translation.
  bool CanPartiallyTranslateTargetLanguage();

  bool IsMimeTypeSupported(const std::string& mime_type);

  // Shows the after translate or error infobar depending on the details.
  void PageTranslated(const std::string& source_lang,
                      const std::string& target_lang,
                      TranslateErrors error_type);

  // Reverts the contents of the page to its original language.
  void RevertTranslation();

  // Global Callbacks

  // The three callbacks below (translate error, translate initialization, and
  // language detected) are global for all WebContentses and should only be used
  // by translate-internals. All other clients should (probably) care about
  // which WebContents is being translated and therefore should instead use
  // LanguageDetectionObserver.

  // Callback types for translate errors.
  using TranslateErrorCallbackList =
      base::RepeatingCallbackList<void(const TranslateErrorDetails&)>;
  using TranslateErrorCallback = TranslateErrorCallbackList::CallbackType;

  // Callback types for translate initialization.
  using TranslateInitCallbackList =
      base::RepeatingCallbackList<void(const TranslateInitDetails&)>;
  using TranslateInitCallback = TranslateInitCallbackList::CallbackType;

  // Callback types for language detection.
  using LanguageDetectedCallbackList =
      base::RepeatingCallbackList<void(const LanguageDetectionDetails&)>;
  using LanguageDetectedCallback = LanguageDetectedCallbackList::CallbackType;

  // Registers a callback for translate errors.
  static base::CallbackListSubscription RegisterTranslateErrorCallback(
      const TranslateErrorCallback& callback);

  // Registers a callback for translate initialization.
  static base::CallbackListSubscription RegisterTranslateInitCallback(
      const TranslateInitCallback& callback);

  // Registers a callback for language detection.
  static base::CallbackListSubscription RegisterLanguageDetectedCallback(
      const LanguageDetectedCallback& callback);

  // Gets the LanguageState associated with the TranslateManager
  LanguageState* GetLanguageState();

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

  // Returns true if the MATCHES_PREVIOUS_LANGUAGE decision should be overridden
  // and logs the event appropriately.
  bool ShouldOverrideMatchesPreviousLanguageDecision();

  // Returns true if the BubbleUI should be suppressed, where |target_language|
  // is the target language that would be shown in the UI.
  bool ShouldSuppressBubbleUI(const std::string& target_language);

  // Sets target language. Note that showing of the translate UI might still not
  // happen in certain situations, e.g. if the translation is prevented by user
  // prefs (i.e., blocklists), if |language_code| isn't a valid target language,
  // if the translate service isn't reachable, etc. Setting
  // |should_auto_translate| to true specifies both (1) that translation should
  // be initiated automatically and (2) that translation should occur even when
  // it would otherwise be prevented by user prefs.
  void SetPredefinedTargetLanguage(const std::string& language_code,
                                   bool should_auto_translate = false);

  // Returns a reference to |active_translate_metrics_logger_|. In the event
  // that this value is null, a |NullTranslateMetricsLogger| (a null
  // implementation) will be returned. This guarantees that the returned value
  // is always non-null.
  TranslateMetricsLogger* GetActiveTranslateMetricsLogger();

  // Sets |active_translate_metrics_logger_| to the given
  // |translate_metrics_logger|.
  void RegisterTranslateMetricsLogger(
      base::WeakPtr<TranslateMetricsLogger> translate_metrics_logger);

  // Called when the language of a page has been detected.
  void NotifyLanguageDetected(const LanguageDetectionDetails& details);

 private:
  friend class translate::testing::TranslateManagerTest;

  // Sends a translation request to the TranslateDriver.
  void DoTranslatePage(const std::string& translate_script,
                       const std::string& source_lang,
                       const std::string& target_lang);

  // Notifies all registered callbacks of translate errors.
  void NotifyTranslateError(TranslateErrors error_type);

  // Notifies all registered callbacks of translate initialization.
  void NotifyTranslateInit(std::string page_language_code,
                           std::string target_lang,
                           TranslateTriggerDecision decision,
                           bool ui_shown);

  // Called when the Translate script has been fetched.
  // Initiates the translation.
  void OnTranslateScriptFetchComplete(const std::string& source_lang,
                                      const std::string& target_lang,
                                      bool success);

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

  // See description of the public |GetTargetLanguage|. This is the actual
  // implementation that also takes a reference to |target_language_origin| for
  // logging.
  static std::string GetTargetLanguage(
      TranslatePrefs* prefs,
      language::LanguageModel* language_model,
      const std::string source_lang_code,
      TranslateBrowserMetrics::TargetLanguageOrigin& target_language_origin);

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

  raw_ptr<TranslateClient> translate_client_;        // Weak.
  raw_ptr<TranslateDriver> translate_driver_;        // Weak.
  raw_ptr<TranslateRanker> translate_ranker_;        // Weak.
  raw_ptr<language::LanguageModel> language_model_;  // Weak.

  base::WeakPtr<TranslateMetricsLogger> active_translate_metrics_logger_;
  std::unique_ptr<NullTranslateMetricsLogger> null_translate_metrics_logger_;

  LanguageState language_state_;

  std::unique_ptr<metrics::TranslateEventProto> translate_event_;

  base::WeakPtrFactory<TranslateManager> weak_method_factory_{this};

  // By default, don't offer to translate in builds lacking an API key. For
  // testing, set to true to offer anyway.
  static bool ignore_missing_key_for_testing_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MANAGER_H_
