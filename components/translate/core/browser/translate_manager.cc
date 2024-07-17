// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include <map>
#include <memory>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/common/language_experiments.h"
#include "components/language/core/common/language_util.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_error_details.h"
#include "components/translate/core/browser/translate_init_details.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/browser/translate_metrics_logger_impl.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ranker.h"
#include "components/translate/core/browser/translate_script.h"
#include "components/translate/core/browser/translate_trigger_decision.h"
#include "components/translate/core/browser/translate_url_util.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/translate/core/common/translate_util.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/google_api_keys.h"
#include "net/base/mime_util.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "third_party/metrics_proto/translate_event.pb.h"

namespace translate {

namespace {

// Callbacks for translate errors.
TranslateManager::TranslateErrorCallbackList* g_error_callback_list_ = nullptr;

// Callbacks for translate initializations.
TranslateManager::TranslateInitCallbackList* g_init_callback_list_ = nullptr;

// Callbacks for language detection.
TranslateManager::LanguageDetectedCallbackList* g_detection_callback_list_ =
    nullptr;

}  // namespace

TranslateManager::~TranslateManager() = default;

// static
base::CallbackListSubscription TranslateManager::RegisterTranslateErrorCallback(
    const TranslateManager::TranslateErrorCallback& callback) {
  if (!g_error_callback_list_)
    g_error_callback_list_ = new TranslateErrorCallbackList;
  return g_error_callback_list_->Add(callback);
}

// static
base::CallbackListSubscription TranslateManager::RegisterTranslateInitCallback(
    const TranslateManager::TranslateInitCallback& callback) {
  if (!g_init_callback_list_)
    g_init_callback_list_ = new TranslateInitCallbackList;
  return g_init_callback_list_->Add(callback);
}

// static
base::CallbackListSubscription
TranslateManager::RegisterLanguageDetectedCallback(
    const TranslateManager::LanguageDetectedCallback& callback) {
  if (!g_detection_callback_list_)
    g_detection_callback_list_ = new LanguageDetectedCallbackList;
  return g_detection_callback_list_->Add(callback);
}

TranslateManager::TranslateManager(TranslateClient* translate_client,
                                   TranslateRanker* translate_ranker,
                                   language::LanguageModel* language_model)
    : page_seq_no_(0),
      translate_client_(translate_client),
      translate_driver_(translate_client_->GetTranslateDriver()),
      translate_ranker_(translate_ranker),
      language_model_(language_model),
      null_translate_metrics_logger_(
          std::make_unique<NullTranslateMetricsLogger>()),
      language_state_(translate_driver_),
      translate_event_(std::make_unique<metrics::TranslateEventProto>()) {}

base::WeakPtr<TranslateManager> TranslateManager::GetWeakPtr() {
  return weak_method_factory_.GetWeakPtr();
}

void TranslateManager::InitiateTranslation(const std::string& page_lang) {
  std::unique_ptr<TranslatePrefs> translate_prefs(
      translate_client_->GetTranslatePrefs());
  std::string page_language_code =
      TranslateDownloadManager::GetLanguageCode(page_lang);
  TranslateBrowserMetrics::TargetLanguageOrigin target_language_origin =
      TranslateBrowserMetrics::TargetLanguageOrigin::kUninitialized;
  std::string target_lang =
      GetTargetLanguage(translate_prefs.get(), language_model_,
                        page_language_code, target_language_origin);

  // TODO(crbug.com/40610937): The ranker event shouldn't be a global on this
  // object. It should instead be passed around to code that uses it.
  InitTranslateEvent(page_language_code, target_lang, *translate_prefs);

  // Logs the initial source and target languages, as well as whether the
  // initial source language is blocked (i.e. on the never translate list).
  GetActiveTranslateMetricsLogger()->LogInitialSourceLanguage(
      page_language_code,
      translate_prefs->IsBlockedLanguage(page_language_code));
  GetActiveTranslateMetricsLogger()->LogTargetLanguage(target_lang,
                                                       target_language_origin);

  const TranslateTriggerDecision& decision = ComputePossibleOutcomes(
      translate_prefs.get(), page_language_code, target_lang);

  bool ui_shown = MaterializeDecision(decision, translate_prefs.get(),
                                      page_language_code, target_lang);
  MaybeShowOmniboxIcon(decision);

  NotifyTranslateInit(page_language_code, target_lang, decision, ui_shown);

  RecordDecisionMetrics(decision, page_language_code, ui_shown);
  RecordDecisionRankerEvent(decision, translate_prefs.get(), page_language_code,
                            target_lang);

  // Mark the current state as the initial state now that we are done
  // initializing Translate.
  GetActiveTranslateMetricsLogger()->LogInitialState();
}

bool TranslateManager::CanManuallyTranslate(bool menuLogging) {
  bool can_translate = true;

  if (!base::FeatureList::IsEnabled(translate::kTranslate)) {
    if (!menuLogging)
      return false;
    TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
        TranslateBrowserMetrics::MenuTranslationUnavailableReason::
            kTranslateDisabled);
    can_translate = false;
  }

  if (net::NetworkChangeNotifier::IsOffline()) {
    if (!menuLogging)
      return false;
    TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
        TranslateBrowserMetrics::MenuTranslationUnavailableReason::
            kNetworkOffline);
    can_translate = false;
  }

  if (!ignore_missing_key_for_testing_ &&
      !::google_apis::HasAPIKeyConfigured()) {
    if (!menuLogging)
      return false;
    TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
        TranslateBrowserMetrics::MenuTranslationUnavailableReason::
            kApiKeysMissing);
    can_translate = false;
  }

  // not supported MIME type pages currently cannot be translated.
  // See bug: 217945, 1208340.
  if (!IsMimeTypeSupported(translate_driver_->GetContentsMimeType())) {
    if (!menuLogging)
      return false;
    TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
        TranslateBrowserMetrics::MenuTranslationUnavailableReason::
            kMIMETypeUnsupported);
    can_translate = false;
  }

  if (!translate_client_->IsTranslatableURL(
          translate_driver_->GetVisibleURL())) {
    if (!menuLogging)
      return false;
    TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
        TranslateBrowserMetrics::MenuTranslationUnavailableReason::
            kURLNotTranslatable);
    can_translate = false;
  }

  const std::string source_language = language_state_.source_language();
  // The source language is empty when language detection has not finished
  // running. In this case, Android queues the translation and waits until the
  // source language has been determined. Android is the only platform that
  // supports manual translation in this case.
#if !BUILDFLAG(IS_ANDROID)
  if (source_language.empty()) {
    if (!menuLogging)
      return false;
    TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
        TranslateBrowserMetrics::MenuTranslationUnavailableReason::
            kSourceLangUnknown);
    can_translate = false;
  }
#endif

  std::unique_ptr<TranslatePrefs> translate_prefs(
      translate_client_->GetTranslatePrefs());
  if (!translate_prefs->IsTranslateAllowedByPolicy()) {
    if (!menuLogging)
      return false;
    TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
        TranslateBrowserMetrics::MenuTranslationUnavailableReason::
            kNotAllowedByPolicy);
    can_translate = false;
  }

  const std::string target_lang = GetTargetLanguage(
      translate_prefs.get(), language_model_,
      TranslateDownloadManager::GetLanguageCode(source_language));
  if (target_lang.empty()) {
    if (!menuLogging)
      return false;
    TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
        TranslateBrowserMetrics::MenuTranslationUnavailableReason::
            kTargetLangUnknown);
    can_translate = false;
  }

  if (menuLogging)
    UMA_HISTOGRAM_BOOLEAN("Translate.MenuTranslation.IsAvailable",
                          can_translate);

  return can_translate;
}

bool TranslateManager::CanPartiallyTranslateTargetLanguage() {
  std::unique_ptr<TranslatePrefs> translate_prefs(
      translate_client_->GetTranslatePrefs());
  const std::string& source_language = language_state_.source_language();
  const std::string target_lang = GetTargetLanguage(
      translate_prefs.get(), language_model_,
      TranslateDownloadManager::GetLanguageCode(source_language));

  if (target_lang.empty()) {
    return false;
  }
  return TranslateLanguageList::IsSupportedPartialTranslateLanguage(
      target_lang);
}

bool TranslateManager::IsMimeTypeSupported(const std::string& mime_type) {
  if (net::MatchesMimeType("image/*", mime_type))
    return false;
  if (mime_type == "multipart/related")
    return false;

  return true;
}

void TranslateManager::ShowTranslateUI(const std::string& target_lang,
                                       bool auto_translate,
                                       bool triggered_from_menu) {
  // If a translation is in progress, do nothing.
  if (language_state_.translation_pending()) {
    return;
  }
  std::unique_ptr<TranslatePrefs> translate_prefs(
      translate_client_->GetTranslatePrefs());
  const std::string source_code = TranslateDownloadManager::GetLanguageCode(
      language_state_.source_language());
  bool is_translated = language_state_.IsPageTranslated() &&
                       target_lang == language_state_.current_language();

  language_state_.SetTranslateEnabled(true);
  const TranslateStep step = is_translated ? TRANSLATE_STEP_AFTER_TRANSLATE
                                           : TRANSLATE_STEP_BEFORE_TRANSLATE;
  // Translate the page if it has not been translated and manual translate
  // should trigger translation automatically. Otherwise, only show the infobar.
  if (auto_translate && !is_translated) {
    TranslatePage(
        source_code, target_lang, triggered_from_menu,
        GetActiveTranslateMetricsLogger()->GetNextManualTranslationType(
            triggered_from_menu));
    return;
  }
  translate_client_->ShowTranslateUI(step, source_code, target_lang,
                                     TranslateErrors::NONE,
                                     triggered_from_menu);
}

void TranslateManager::ShowTranslateUI(bool auto_translate,
                                       bool triggered_from_menu) {
  std::unique_ptr<TranslatePrefs> translate_prefs(
      translate_client_->GetTranslatePrefs());
  const std::string target_lang =
      GetTargetLanguageForDisplay(translate_prefs.get(), language_model_);

  ShowTranslateUI(target_lang, auto_translate, triggered_from_menu);
}

void TranslateManager::TranslatePage(const std::string& original_source_lang,
                                     const std::string& target_lang,
                                     bool triggered_from_menu,
                                     TranslationType translation_type) {
  const GURL& page_url = translate_driver_->GetVisibleURL();
  language_state_.SetTranslationType(translation_type);
  // TODO(crbug.com/40898004): Very rarely, users can reach a state where this
  // Translate code is called on a page ineligible for translation. It is
  // unclear how this state is reached, but the crash rate is very low
  // (<0.1CPM) and this state is not breaking for the user. This crash rate
  // should be monitored and investigated if it increases.
  if (!translate_client_->IsTranslatableURL(page_url)) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  translate_driver_->PrepareToTranslatePage(page_seq_no_, original_source_lang,
                                            target_lang, triggered_from_menu);

  // If the source language matches the UI language, it means the translation
  // prompt is being forced by an experiment. Report this so the count of how
  // often it happens can be decremented (meaning the user didn't decline or
  // ignore the prompt).
  if (original_source_lang ==
      TranslateDownloadManager::GetLanguageCode(
          TranslateDownloadManager::GetInstance()->application_locale())) {
    translate_client_->GetTranslatePrefs()
        ->ReportAcceptedAfterForceTriggerOnEnglishPages();
  }

  // If the target language isn't in the chrome://settings/languages list, add
  // it there. This way, it's obvious to the user that Chrome is remembering
  // their choice, they can remove it from the list, and they'll send that
  // language in the Accept-Language header, giving servers a chance to serve
  // them pages in that language.
  AddTargetLanguageToAcceptLanguages(target_lang);

  // Translation can be kicked by context menu against unsupported languages.
  // Unsupported language strings should be replaced with
  // kUnknownLanguageCode in order to send a translation request with enabling
  // server side auto language detection.
  std::string source_lang(original_source_lang);
  if (!TranslateDownloadManager::IsSupportedLanguage(source_lang))
    source_lang = std::string(translate::kUnknownLanguageCode);

  // Capture the translate event if we were triggered from the menu.
  if (triggered_from_menu) {
    RecordTranslateEvent(
        metrics::TranslateEventProto::USER_CONTEXT_MENU_TRANSLATE);
  }

  if (source_lang == target_lang) {
    // If the languages are the same, try the translation using the unknown
    // language code. The source and target languages should only be equal if
    // the translation was manually triggered by the user. Rather than show
    // them the error, we should attempt to send the page for translation. For
    // page with multiple languages we often detect same language, but the
    // Translation service is able to translate the various languages using it's
    // own language detection.
    source_lang = translate::kUnknownLanguageCode;
  }

  // Trigger the "translating now" UI.
  translate_client_->ShowTranslateUI(
      translate::TRANSLATE_STEP_TRANSLATING, source_lang, target_lang,
      TranslateErrors::NONE, triggered_from_menu);

  GetActiveTranslateMetricsLogger()->LogTranslationStarted(translation_type);

  TranslateScript* script = TranslateDownloadManager::GetInstance()->script();
  DCHECK(script != nullptr);

  const std::string& script_data = script->data();
  if (!script_data.empty()) {
    DoTranslatePage(script_data, source_lang, target_lang);
    return;
  }

  // The script is not available yet.  Queue that request and query for the
  // script.  Once it is downloaded we'll do the translate.
  TranslateScript::RequestCallback callback =
      base::BindOnce(&TranslateManager::OnTranslateScriptFetchComplete,
                     GetWeakPtr(), source_lang, target_lang);

  script->Request(std::move(callback), translate_driver_->IsIncognito());
}

void TranslateManager::RevertTranslation() {
  // Do nothing if the page is not translated.
  if (!GetLanguageState()->IsPageTranslated()) {
    return;
  }
  // Capture the revert event in the translate metrics
  RecordTranslateEvent(metrics::TranslateEventProto::USER_REVERT);

  // Revert the translation.
  translate_driver_->RevertTranslation(page_seq_no_);
  language_state_.SetCurrentLanguage(language_state_.source_language());

  GetActiveTranslateMetricsLogger()->LogReversion();
}

void TranslateManager::DoTranslatePage(const std::string& translate_script,
                                       const std::string& source_lang,
                                       const std::string& target_lang) {
  language_state_.set_translation_pending(true);
  translate_driver_->TranslatePage(page_seq_no_, translate_script, source_lang,
                                   target_lang);
}

// Notifies |g_error_callback_list_| of translate errors.
void TranslateManager::NotifyTranslateError(TranslateErrors error_type) {
  if (!g_error_callback_list_ || error_type == TranslateErrors::NONE ||
      translate_driver_->IsIncognito()) {
    return;
  }

  TranslateErrorDetails error_details;
  error_details.time = base::Time::Now();
  error_details.url = translate_driver_->GetLastCommittedURL();
  error_details.error = error_type;
  g_error_callback_list_->Notify(error_details);
}

void TranslateManager::NotifyTranslateInit(std::string page_language_code,
                                           std::string target_lang,
                                           TranslateTriggerDecision decision,
                                           bool ui_shown) {
  if (!g_init_callback_list_ || translate_driver_->IsIncognito())
    return;

  TranslateInitDetails details;
  details.time = base::Time::Now();
  details.url = translate_driver_->GetLastCommittedURL();
  details.page_language_code = page_language_code;
  details.target_lang = target_lang;
  details.decision = decision;
  details.ui_shown = ui_shown;

  g_init_callback_list_->Notify(details);
}

void TranslateManager::NotifyLanguageDetected(
    const translate::LanguageDetectionDetails& details) {
  if (!g_detection_callback_list_)
    return;

  g_detection_callback_list_->Notify(details);
}

void TranslateManager::PageTranslated(const std::string& source_lang,
                                      const std::string& target_lang,
                                      TranslateErrors error_type) {
  if (error_type == TranslateErrors::NONE) {
    // The user could have updated the source language before translating, so
    // update the language state with both source and current.
    language_state_.SetSourceLanguage(source_lang);
    language_state_.SetCurrentLanguage(target_lang);
  }

  language_state_.set_translation_pending(false);
  language_state_.set_translation_error(error_type != TranslateErrors::NONE);

  if ((error_type == TranslateErrors::NONE) &&
      source_lang != translate::kUnknownLanguageCode &&
      !TranslateDownloadManager::IsSupportedLanguage(source_lang)) {
    error_type = TranslateErrors::UNSUPPORTED_LANGUAGE;
  }

  // Currently we only want to log any error happens during the translation
  // script initialization phase such as translation script failed because of
  // CSP issues (crbug.com/738277).
  // Note: NotifyTranslateError and ShowTranslateUI will not log the errors.
  if (error_type == TranslateErrors::INITIALIZATION_ERROR)
    RecordTranslateEvent(metrics::TranslateEventProto::INITIALIZATION_ERROR);

  translate_client_->ShowTranslateUI(translate::TRANSLATE_STEP_AFTER_TRANSLATE,
                                     source_lang, target_lang, error_type,
                                     false);

  language_state_.SetTranslationType(TranslationType::kUninitialized);
  NotifyTranslateError(error_type);
  GetActiveTranslateMetricsLogger()->LogTranslationFinished(
      error_type == TranslateErrors::NONE, error_type);
}

void TranslateManager::OnTranslateScriptFetchComplete(
    const std::string& source_lang,
    const std::string& target_lang,
    bool success) {
  if (!translate_driver_->HasCurrentPage())
    return;

  if (success) {
    // Translate the page.
    TranslateScript* translate_script =
        TranslateDownloadManager::GetInstance()->script();
    DCHECK(translate_script);
    DoTranslatePage(translate_script->data(), source_lang, target_lang);
  } else {
    translate_client_->ShowTranslateUI(
        translate::TRANSLATE_STEP_TRANSLATE_ERROR, source_lang, target_lang,
        TranslateErrors::NETWORK, false);
    NotifyTranslateError(TranslateErrors::NETWORK);
    GetActiveTranslateMetricsLogger()->LogTranslationFinished(
        false, TranslateErrors::NETWORK);
  }
}

std::string TranslateManager::GetTargetLanguageForDisplay(
    TranslatePrefs* prefs,
    language::LanguageModel* language_model) {
  // If the page is translated, always return the current target language. This
  // ensures that reshowing the UI on a translated page maintains the correct
  // target language that the page is currently translated into.
  if (language_state_.IsPageTranslated())
    return language_state_.current_language();

  return GetTargetLanguage(prefs, language_model,
                           TranslateDownloadManager::GetLanguageCode(
                               language_state_.source_language()));
}

// static
std::string TranslateManager::GetTargetLanguage(
    TranslatePrefs* prefs,
    language::LanguageModel* language_model,
    const std::string source_lang_code,
    TranslateBrowserMetrics::TargetLanguageOrigin& target_language_origin) {
  DCHECK(prefs);

  // For callers with source language we want to check their auto translate
  // preference.
  // We don't enable auto translate feature in incognito profiles so this will
  // always be empty for incognito.
  if (source_lang_code != translate::kUnknownLanguageCode) {
    std::string auto_translate_language =
        translate::TranslateManager::GetAutoTargetLanguage(source_lang_code,
                                                           prefs);
    if (!auto_translate_language.empty() &&
        TranslateDownloadManager::IsSupportedLanguage(
            auto_translate_language)) {
      target_language_origin =
          TranslateBrowserMetrics::TargetLanguageOrigin::kAutoTranslate;
      return auto_translate_language;
    }
  }

  const std::string& recent_target = prefs->GetRecentTargetLanguage();

  // If we've recorded the most recent target language, use that.
  if (base::FeatureList::IsEnabled(kTranslateRecentTarget) &&
      !recent_target.empty() &&
      TranslateDownloadManager::IsSupportedLanguage(recent_target)) {
    target_language_origin =
        TranslateBrowserMetrics::TargetLanguageOrigin::kRecentTarget;
    return recent_target;
  }

  if (language_model) {
    std::vector<std::string> language_codes;
    for (const auto& lang : language_model->GetLanguages()) {
      std::string lang_code =
          TranslateDownloadManager::GetLanguageCode(lang.lang_code);
      language::ToTranslateLanguageSynonym(&lang_code);
      if (TranslateDownloadManager::IsSupportedLanguage(lang_code))
        language_codes.push_back(lang_code);
    }

    // If forcing triggering on English, skip English as the target language if
    // possible by moving it to the end of |language_codes|.
    if (prefs->ShouldForceTriggerTranslateOnEnglishPages() &&
        source_lang_code == "en") {
      std::stable_partition(
          language_codes.begin(), language_codes.end(),
          [&](const auto& lang) { return lang.compare("en") != 0; });
    }

    // Use the first language from the model that translate supports.
    if (!language_codes.empty()) {
      target_language_origin =
          TranslateBrowserMetrics::TargetLanguageOrigin::kLanguageModel;
      return language_codes[0];
    }
  }

  // Get the browser's user interface language.
  std::string ui_language = TranslateDownloadManager::GetLanguageCode(
      TranslateDownloadManager::GetInstance()->application_locale());
  // Map 'he', 'nb', 'fil' back to 'iw', 'no', 'tl'
  language::ToTranslateLanguageSynonym(&ui_language);
  if (TranslateDownloadManager::IsSupportedLanguage(ui_language)) {
    target_language_origin =
        TranslateBrowserMetrics::TargetLanguageOrigin::kApplicationUI;
    return ui_language;
  }

  // Get the first supported language on the Accept Languages list.
  std::vector<std::string> accept_languages_list;
  prefs->GetLanguageList(&accept_languages_list);
  for (const auto& lang : accept_languages_list) {
    std::string lang_code = TranslateDownloadManager::GetLanguageCode(lang);
    if (TranslateDownloadManager::IsSupportedLanguage(lang_code)) {
      target_language_origin =
          TranslateBrowserMetrics::TargetLanguageOrigin::kAcceptLanguages;
      return lang_code;
    }
  }

  // If there isn't a target language determined by the above logic, default to
  // English. Otherwise the user can get stuck not being able to translate. See
  // https://crbug.com/1041387.
  target_language_origin =
      TranslateBrowserMetrics::TargetLanguageOrigin::kDefaultEnglish;
  return std::string("en");
}

// static
std::string TranslateManager::GetTargetLanguage(
    TranslatePrefs* prefs,
    language::LanguageModel* language_model,
    const std::string source_lang_code) {
  TranslateBrowserMetrics::TargetLanguageOrigin target_language_origin =
      TranslateBrowserMetrics::TargetLanguageOrigin::kUninitialized;
  return GetTargetLanguage(prefs, language_model, source_lang_code,
                           target_language_origin);
}

// static
std::string TranslateManager::GetAutoTargetLanguage(
    const std::string& source_language,
    TranslatePrefs* translate_prefs) {
  std::string auto_target_lang;
  if (translate_prefs->ShouldAutoTranslate(source_language,
                                           &auto_target_lang)) {
    // We need to confirm that the saved target language is still supported.
    // Also, GetLanguageCode will take care of removing country code if any.
    auto_target_lang =
        TranslateDownloadManager::GetLanguageCode(auto_target_lang);
    if (TranslateDownloadManager::IsSupportedLanguage(auto_target_lang))
      return auto_target_lang;
  }
  return std::string();
}

LanguageState* TranslateManager::GetLanguageState() {
  return &language_state_;
}

bool TranslateManager::ignore_missing_key_for_testing_ = false;

// static
void TranslateManager::SetIgnoreMissingKeyForTesting(bool ignore) {
  ignore_missing_key_for_testing_ = ignore;
}

// static
bool TranslateManager::IsAvailable(const TranslatePrefs* prefs) {
  // These conditions mirror the conditions in InitiateTranslation.
  return base::FeatureList::IsEnabled(translate::kTranslate) &&
         (ignore_missing_key_for_testing_ ||
          ::google_apis::HasAPIKeyConfigured()) &&
         prefs->IsOfferTranslateEnabled();
}

void TranslateManager::InitTranslateEvent(const std::string& src_lang,
                                          const std::string& dst_lang,
                                          const TranslatePrefs& prefs) {
  translate_event_->Clear();
  translate_event_->set_source_language(src_lang);
  translate_event_->set_target_language(dst_lang);
  translate_event_->set_country(prefs.GetCountry());
  translate_event_->set_accept_count(
      prefs.GetTranslationAcceptedCount(src_lang));
  translate_event_->set_decline_count(
      prefs.GetTranslationDeniedCount(src_lang));
  translate_event_->set_ignore_count(
      prefs.GetTranslationIgnoredCount(src_lang));
  translate_event_->set_ranker_response(
      metrics::TranslateEventProto::NOT_QUERIED);
  translate_event_->set_event_type(metrics::TranslateEventProto::UNKNOWN);
  // TODO(rogerm): Populate the language list.
}

void TranslateManager::RecordTranslateEvent(int event_type) {
  translate_ranker_->RecordTranslateEvent(
      event_type, translate_driver_->GetUkmSourceId(), translate_event_.get());
}

bool TranslateManager::ShouldOverrideMatchesPreviousLanguageDecision() {
  return translate_ranker_->ShouldOverrideMatchesPreviousLanguageDecision(
      translate_driver_->GetUkmSourceId(), translate_event_.get());
}

bool TranslateManager::ShouldSuppressBubbleUI(
    const std::string& target_language) {
  // Suppress the UI if the user navigates to a page with the same language as
  // the previous page, unless the page was loaded from a link click with
  // hrefTranslate attached that matches the target language, since in that case
  // the site might have a good reason to show the translate UI regardless of
  // the source language of the previous page. In the new UI, continue offering
  // translation after the user navigates to another page.
  DCHECK(!target_language.empty());
  if (language_state_.href_translate() == target_language ||
      language_state_.HasLanguageChanged() ||
      ShouldOverrideMatchesPreviousLanguageDecision()) {
    return false;
  }

  GetActiveTranslateMetricsLogger()->LogTriggerDecision(
      TriggerDecision::kDisabledMatchesPreviousLanguage);
  return true;
}

void TranslateManager::AddTargetLanguageToAcceptLanguages(
    const std::string& target_language_code) {
  auto prefs = translate_client_->GetTranslatePrefs();
  std::vector<std::string> languages;
  prefs->GetLanguageList(&languages);

  std::string_view target_language, tail;
  // |target_language_code| should satisfy BCP47 and consist of a language code
  // and an optional region code joined by an hyphen.
  std::tie(target_language, tail) =
      language::SplitIntoMainAndTail(target_language_code);

  // Don't add the target language if it's redundant with another already in the
  // list.
  if (tail.empty()) {
    for (const auto& language : languages) {
      if (language::ExtractBaseLanguage(language) == target_language)
        return;
    }
  } else {
    for (const auto& language : languages) {
      if (language == target_language_code)
        return;
    }
  }

  // Only add the target language if it's not an automatic target (such as when
  // translation happens because of an hrefTranslate navigation).
  if (language_state_.AutoTranslateTo() != target_language_code &&
      language_state_.href_translate() != target_language_code) {
    prefs->AddToLanguageList(target_language_code, /*force_blocked=*/false);
  }
}

const TranslateTriggerDecision TranslateManager::ComputePossibleOutcomes(
    TranslatePrefs* translate_prefs,
    const std::string& page_language_code,
    const std::string& target_lang) {
  // This function looks at a bunch of signals and determines which of three
  // outcomes should be selected:
  // 1. Auto-translate the page
  // 2. Show translate UI
  // 3. Do nothing
  // This is achieved by passing the |decision| object to the different Filter*
  // functions, which will mark certain outcomes as undesirable. This |decision|
  // object is then used to trigger the correct behavior, and finally record
  // corresponding metrics in InitiateTranslation.
  TranslateTriggerDecision decision;

  FilterIsTranslatePossible(&decision, translate_prefs, page_language_code,
                            target_lang);

  // Querying the ranker now, but not exiting immediately so that we may log
  // other potential suppression reasons.
  if (!translate_ranker_->ShouldOfferTranslation(
          translate_event_.get(), GetActiveTranslateMetricsLogger())) {
    decision.SuppressFromRanker();
  }

  FilterForUserPrefs(&decision, translate_prefs, page_language_code);

  if (decision.should_suppress_from_ranker()) {
    // Delay logging this until after FilterForUserPrefs because TriggerDecision
    // values from FilterForUserPrefs have higher priority.
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kDisabledByRanker);
  }

  FilterAutoTranslate(&decision, translate_prefs, page_language_code);
  FilterForHrefTranslate(&decision, translate_prefs, page_language_code);
  FilterForPredefinedTarget(&decision, translate_prefs, page_language_code);

  return decision;
}

void TranslateManager::FilterIsTranslatePossible(
    TranslateTriggerDecision* decision,
    TranslatePrefs* translate_prefs,
    const std::string& page_language_code,
    const std::string& target_lang) {
  // Short-circuit out if not in a state where initiating translation makes
  // sense (this method may be called multiple times for a given page).
  if (!language_state_.page_level_translation_criteria_met() ||
      language_state_.translation_pending() ||
      language_state_.translation_declined() ||
      language_state_.IsPageTranslated()) {
    decision->PreventAllTriggering();
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kDisabledDoesntNeedTranslation);
  }

  if (!base::FeatureList::IsEnabled(translate::kTranslate)) {
    decision->PreventAllTriggering();
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kDisabledTranslationFeatureDisabled);
  }

  // Also, skip if the connection is currently offline - initiation doesn't make
  // sense there, either.
  if (net::NetworkChangeNotifier::IsOffline()) {
    decision->PreventAllTriggering();
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kDisabledOffline);
  }

  if (!ignore_missing_key_for_testing_ &&
      !::google_apis::HasAPIKeyConfigured()) {
    // Without an API key, translate won't work, so don't offer to translate in
    // the first place. Leave kOfferTranslateEnabled on, though, because that
    // settings syncs and we don't want to turn off translate everywhere else.
    decision->PreventAllTriggering();
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kDisabledMissingAPIKey);
  }

  // not supported MIME type pages currently cannot be translated.
  // See bug: 217945, 1208340.
  if (!IsMimeTypeSupported(translate_driver_->GetContentsMimeType())) {
    decision->PreventAllTriggering();
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kDisabledMIMETypeNotSupported);
  }

  // Don't translate any Chrome specific page, e.g., New Tab Page, Download,
  // History, and so on.
  const GURL& page_url = translate_driver_->GetVisibleURL();
  if (!translate_client_->IsTranslatableURL(page_url)) {
    decision->PreventAllTriggering();
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kDisabledURLNotSupported);
  }

  if (!translate_prefs->IsOfferTranslateEnabled()) {
    decision->PreventAllTriggering();
    decision->ranker_events.push_back(
        metrics::TranslateEventProto::DISABLED_BY_PREF);
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kDisabledNeverOfferTranslations);
  }

  // Don't translate similar languages (ex: en-US to en).
  if (page_language_code == target_lang) {
    // This doesn't prevent *all* possible translate outcomes because some could
    // use a different target language, making this condition only relevant to
    // regular auto-translate/show UI.
    decision->PreventAutoTranslate();
    decision->PreventShowingUI();
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kDisabledSimilarLanguages);
  }

  // Nothing to do if either the language Chrome is in or the language of
  // the page is not supported by the translation server.
  if (target_lang.empty() ||
      !TranslateDownloadManager::IsSupportedLanguage(page_language_code)) {
    // This doesn't prevent *all* possible translate outcomes because some could
    // use a different target language, making this condition only relevant to
    // regular auto-translate/show UI.
    decision->PreventAutoTranslate();
    decision->PreventShowingUI();
    decision->ranker_events.push_back(
        metrics::TranslateEventProto::UNSUPPORTED_LANGUAGE);
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kDisabledUnsupportedLanguage);
  }
}

void TranslateManager::FilterAutoTranslate(
    TranslateTriggerDecision* decision,
    TranslatePrefs* translate_prefs,
    const std::string& page_language_code) {
  // Determine whether auto-translate is required, and if so for which target
  // language.
  std::string always_translate_target =
      GetAutoTargetLanguage(page_language_code, translate_prefs);
  std::string link_auto_translate_target = language_state_.AutoTranslateTo();
  if (!translate_driver_->IsIncognito() && !always_translate_target.empty()) {
    // If the user has previously selected "always translate" for this language
    // we automatically translate.  Note that in incognito mode we disable that
    // feature; the user will get an infobar, so they can control whether the
    // page's text is sent to the translate server.
    decision->auto_translate_target = always_translate_target;
    decision->ranker_events.push_back(
        metrics::TranslateEventProto::AUTO_TRANSLATION_BY_PREF);
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kAutomaticTranslationByPref);
  } else if (!link_auto_translate_target.empty()) {
    // This page was navigated through a click from a translated page.
    decision->auto_translate_target = link_auto_translate_target;
    decision->ranker_events.push_back(
        metrics::TranslateEventProto::AUTO_TRANSLATION_BY_LINK);
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kAutomaticTranslationByLink);
  }

  if (decision->auto_translate_target.empty()) {
    decision->PreventAutoTranslate();
  }
}

void TranslateManager::FilterForUserPrefs(
    TranslateTriggerDecision* decision,
    TranslatePrefs* translate_prefs,
    const std::string& page_language_code) {
  // Don't translate any user blocklisted languages.
  if (!translate_prefs->CanTranslateLanguage(page_language_code)) {
    decision->SetIsInLanguageBlocklist();

    decision->PreventAutoTranslate();
    decision->PreventShowingUI();

    // Disable showing the translate UI for a predefined target language,
    // although note that auto translation to a predefined target language is
    // still allowed to happen, since that auto-translation overrides the user's
    // language blocklist.
    decision->PreventShowingPredefinedLanguageTranslateUI();

    decision->ranker_events.push_back(
        metrics::TranslateEventProto::LANGUAGE_DISABLED_BY_USER_CONFIG);
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kDisabledNeverTranslateLanguage);
  }

  // Don't translate any user blocklisted URLs.
  const GURL& page_url = translate_driver_->GetVisibleURL();
  if (translate_prefs->IsSiteOnNeverPromptList(page_url.HostNoBrackets())) {
    decision->SetIsInSiteBlocklist();

    decision->PreventAutoTranslate();
    decision->PreventShowingUI();

    // The site blocklist isn't overridden for hrefTranslate.
    decision->PreventShowingHrefTranslateUI();
    decision->PreventAutoHrefTranslate();

    // The site blocklist isn't overridden for predefined target languages.
    decision->PreventShowingPredefinedLanguageTranslateUI();
    decision->PreventPredefinedLanguageAutoTranslate();

    decision->ranker_events.push_back(
        metrics::TranslateEventProto::URL_DISABLED_BY_USER_CONFIG);
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kDisabledNeverTranslateSite);
  }
}

void TranslateManager::FilterForHrefTranslate(
    TranslateTriggerDecision* decision,
    TranslatePrefs* translate_prefs,
    const std::string& page_language_code) {
  if (!language_state_.navigation_from_google()) {
    decision->PreventAutoHrefTranslate();
  }

  decision->href_translate_source = page_language_code;
  decision->href_translate_target = language_state_.href_translate();

  if (language_state_.navigation_from_google()) {
    GetActiveTranslateMetricsLogger()->SetHasHrefTranslateTarget(
        !decision->href_translate_target.empty());
  }

  // Can't honor hrefTranslate if there's no specified target or the target
  // language isn't supported.
  if (decision->href_translate_target.empty() ||
      !TranslateDownloadManager::IsSupportedLanguage(
          decision->href_translate_target)) {
    decision->PreventAutoHrefTranslate();
    decision->PreventShowingHrefTranslateUI();
  }

  if (!TranslateDownloadManager::IsSupportedLanguage(page_language_code) ||
      page_language_code == decision->href_translate_target) {
    if (language_state_.navigation_from_google()) {
      // If hrefTranslate is present but the page language is unsupported,
      // unknown, or seems to match the hrefTranslate target language, then as a
      // last ditch effort assume that language detection was incorrect and send
      // "und" as the source language to make the translate service attempt to
      // detect the language as it processes the page content.
      decision->href_translate_source = translate::kUnknownLanguageCode;
    } else {
      decision->PreventAutoHrefTranslate();
      decision->PreventShowingHrefTranslateUI();
    }
  }
}

void TranslateManager::FilterForPredefinedTarget(
    TranslateTriggerDecision* decision,
    TranslatePrefs* translate_prefs,
    const std::string& page_language_code) {
  decision->predefined_translate_source = page_language_code;
  decision->predefined_translate_target =
      language_state_.GetPredefinedTargetLanguage();

  if (!language_state_.should_auto_translate_to_predefined_target_language()) {
    decision->PreventPredefinedLanguageAutoTranslate();
  }

  if (decision->predefined_translate_target.empty() ||
      !TranslateDownloadManager::IsSupportedLanguage(
          decision->predefined_translate_target)) {
    decision->PreventShowingPredefinedLanguageTranslateUI();
    decision->PreventPredefinedLanguageAutoTranslate();
    return;
  }

  if (!TranslateDownloadManager::IsSupportedLanguage(
          decision->predefined_translate_source) ||
      decision->predefined_translate_source ==
          decision->href_translate_target) {
    // If a predefined auto-translate target language is present but the page
    // language is unsupported, unknown, or seems to match this target language,
    // then as a last ditch effort assume that language detection was incorrect
    // and send "und" as the source language to make the translate service
    // attempt to detect the language as it processes the page content.
    decision->predefined_translate_source = translate::kUnknownLanguageCode;
  }
}

void TranslateManager::MaybeShowOmniboxIcon(
    const TranslateTriggerDecision& decision) {
  if (decision.IsTriggeringPossible()) {
    // Show the omnibox icon if any translate trigger is possible.
    language_state_.SetTranslateEnabled(true);
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kShowIcon);
  }
}

bool TranslateManager::MaterializeDecision(
    const TranslateTriggerDecision& decision,
    TranslatePrefs* translate_prefs,
    const std::string& page_language_code,
    const std::string target_lang) {
  // Auto-translating always happens if it's still possible here.
  if (decision.can_auto_translate()) {
    TranslatePage(page_language_code, decision.auto_translate_target, false,
                  GetLanguageState()->InTranslateNavigation()
                      ? TranslationType::kAutomaticTranslationByLink
                      : TranslationType::kAutomaticTranslationByPref);
    return true;
  }

  if (decision.can_auto_href_translate()) {
    TranslatePage(decision.href_translate_source,
                  decision.href_translate_target, false,
                  TranslationType::kAutomaticTranslationByHref);
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kAutomaticTranslationByHref);
    return true;
  }

  if (decision.can_auto_translate_for_predefined_language()) {
    TranslatePage(decision.predefined_translate_source,
                  decision.predefined_translate_target, false,
                  TranslationType::kAutomaticTranslationToPredefinedTarget);
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kAutomaticTranslationToPredefinedTarget);
    return true;
  }

  // Auto-translate didn't happen, so check if the UI should be shown. It must
  // not be suppressed by preference, system state, or the Ranker.

  // Will be true if we've decided to show the infobar/bubble UI to the user.
  bool did_show_ui = false;

  // Check whether the target language is predefined. If it is predefined
  // trigger Translate UI even if it would not otherwise be triggered
  // or would be triggered with another language.
  if (decision.can_show_predefined_language_translate_ui()) {
    did_show_ui = translate_client_->ShowTranslateUI(
        translate::TRANSLATE_STEP_BEFORE_TRANSLATE, page_language_code,
        decision.predefined_translate_target, TranslateErrors::NONE, false);
  }

  if (!did_show_ui && decision.ShouldShowUI()) {
    // If the source language matches the UI language, it means the translation
    // prompt is being forced by an experiment. Report this so the count of how
    // often it happens can be tracked to suppress the experiment as necessary.
    if (page_language_code ==
        TranslateDownloadManager::GetLanguageCode(
            TranslateDownloadManager::GetInstance()->application_locale())) {
      translate_prefs->ReportForceTriggerOnEnglishPages();
    }

    // Prompts the user if they want the page translated.
    did_show_ui = translate_client_->ShowTranslateUI(
        translate::TRANSLATE_STEP_BEFORE_TRANSLATE, page_language_code,
        target_lang, TranslateErrors::NONE, false);
  }

  // Auto-translate didn't happen, and the UI wasn't shown so consider the
  // hrefTranslate attribute if it was present on the originating link.
  if (!did_show_ui && decision.can_show_href_translate_ui()) {
    did_show_ui = translate_client_->ShowTranslateUI(
        translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
        decision.href_translate_source, decision.href_translate_target,
        TranslateErrors::NONE, false);
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kShowUIFromHref);
  }

  if (did_show_ui) {
    GetActiveTranslateMetricsLogger()->LogTriggerDecision(
        TriggerDecision::kShowUI);
  }

  return did_show_ui;
}

void TranslateManager::RecordDecisionMetrics(
    const TranslateTriggerDecision& decision,
    const std::string& page_language_code,
    bool ui_shown) {
  // For Google navigations, the hrefTranslate hint may trigger a translation
  // automatically. Record metrics if there is navigation from Google and a
  // |decision.href_translate_target|.
  if (language_state_.navigation_from_google() &&
      !decision.href_translate_target.empty()) {
    if (decision.can_auto_translate() || decision.can_auto_href_translate()) {
      if (decision.can_auto_translate() &&
          decision.auto_translate_target != decision.href_translate_target) {
        TranslateBrowserMetrics::ReportTranslateHrefHintStatus(
            TranslateBrowserMetrics::HrefTranslateStatus::
                kAutoTranslatedDifferentTargetLanguage);
      } else {
        TranslateBrowserMetrics::ReportTranslateHrefHintStatus(
            TranslateBrowserMetrics::HrefTranslateStatus::kAutoTranslated);
      }
    } else if (decision.can_show_href_translate_ui()) {
      TranslateBrowserMetrics::ReportTranslateHrefHintStatus(
          TranslateBrowserMetrics::HrefTranslateStatus::
              kUiShownNotAutoTranslated);
    } else {
      TranslateBrowserMetrics::ReportTranslateHrefHintStatus(
          TranslateBrowserMetrics::HrefTranslateStatus::
              kNoUiShownNotAutoTranslated);
    }
  }

  if (!decision.can_auto_translate() && !decision.can_auto_href_translate() &&
      (decision.can_auto_translate_for_predefined_language() ||
       decision.can_show_predefined_language_translate_ui())) {
    return;
  }
}

void TranslateManager::RecordDecisionRankerEvent(
    const TranslateTriggerDecision& decision,
    TranslatePrefs* translate_prefs,
    const std::string& page_language_code,
    const std::string& target_lang) {
  if (!decision.auto_translate_target.empty()) {
    translate_event_->set_modified_target_language(
        decision.auto_translate_target);
  }

  if (!decision.ranker_events.empty()) {
    auto event = decision.ranker_events[0];
    RecordTranslateEvent(event);
  }

  // Finally, if the decision was to show UI and ranker suppressed it, log that.
  if (!decision.can_auto_translate() && decision.can_show_ui() &&
      decision.should_suppress_from_ranker()) {
    RecordTranslateEvent(metrics::TranslateEventProto::DISABLED_BY_RANKER);
  }
}

void TranslateManager::SetPredefinedTargetLanguage(
    const std::string& language_code,
    bool should_auto_translate) {
  language_state_.SetPredefinedTargetLanguage(language_code,
                                              should_auto_translate);
}

TranslateMetricsLogger* TranslateManager::GetActiveTranslateMetricsLogger() {
  // If |active_translate_metrics_logger_| is not null, return that. Otherwise
  // return |null_translate_metrics_logger_|. This way the callee doesn't have
  // to check if the returned value is null.
  return active_translate_metrics_logger_
             ? active_translate_metrics_logger_.get()
             : null_translate_metrics_logger_.get();
}

void TranslateManager::RegisterTranslateMetricsLogger(
    base::WeakPtr<TranslateMetricsLogger> translate_metrics_logger) {
  active_translate_metrics_logger_ = translate_metrics_logger;
}

}  // namespace translate
