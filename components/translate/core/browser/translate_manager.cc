// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/common/language_experiments.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_error_details.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ranker.h"
#include "components/translate/core/browser/translate_script.h"
#include "components/translate/core/browser/translate_url_util.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/translate/core/common/translate_util.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/google_api_keys.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "third_party/metrics_proto/translate_event.pb.h"

namespace translate {

namespace {

// Callbacks for translate errors.
TranslateManager::TranslateErrorCallbackList* g_callback_list_ = nullptr;

const char kReportLanguageDetectionErrorURL[] =
    "https://translate.google.com/translate_error?client=cr&action=langidc";

// Used in kReportLanguageDetectionErrorURL to specify the original page
// language.
const char kSourceLanguageQueryName[] = "sl";

// Used in kReportLanguageDetectionErrorURL to specify the page URL.
const char kUrlQueryName[] = "u";

std::set<std::string> GetSkippedLanguagesForExperiments(
    std::string source_lang,
    translate::TranslatePrefs* translate_prefs) {
  // Under this experiment, skip english as the target language if possible so
  // that Translate triggers on English pages.
  std::set<std::string> skipped_languages;
  if (language::ShouldForceTriggerTranslateOnEnglishPages(
          translate_prefs->GetForceTriggerOnEnglishPagesCount()) &&
      source_lang == "en") {
    skipped_languages.insert("en");
  }
  return skipped_languages;
}

// Moves any element in |languages| for which |lang_code| is found in
// |skipped_languages| to the end of |languages|. Otherwise preserves relative
// ordering of elements. Modifies |languages| in place.
void MoveSkippedLanguagesToEndIfNecessary(
    std::vector<std::string>* languages,
    const std::set<std::string>& skipped_languages) {
  if (!skipped_languages.empty()) {
    std::stable_partition(
        languages->begin(), languages->end(), [&](const auto& lang) {
          return skipped_languages.find(lang) == skipped_languages.end();
        });
  }
}

}  // namespace

TranslateManager::~TranslateManager() {}

// static
std::unique_ptr<TranslateManager::TranslateErrorCallbackList::Subscription>
TranslateManager::RegisterTranslateErrorCallback(
    const TranslateManager::TranslateErrorCallback& callback) {
  if (!g_callback_list_)
    g_callback_list_ = new TranslateErrorCallbackList;
  return g_callback_list_->Add(callback);
}

TranslateManager::TranslateManager(TranslateClient* translate_client,
                                   TranslateRanker* translate_ranker,
                                   language::LanguageModel* language_model)
    : page_seq_no_(0),
      translate_client_(translate_client),
      translate_driver_(translate_client_->GetTranslateDriver()),
      translate_ranker_(translate_ranker),
      language_model_(language_model),
      language_state_(translate_driver_),
      translate_event_(std::make_unique<metrics::TranslateEventProto>()),
      weak_method_factory_(this) {}

base::WeakPtr<TranslateManager> TranslateManager::GetWeakPtr() {
  return weak_method_factory_.GetWeakPtr();
}

void TranslateManager::InitiateTranslation(const std::string& page_lang) {
  // Short-circuit out if not in a state where initiating translation makes
  // sense (this method may be called muhtiple times for a given page).
  if (!language_state_.page_needs_translation() ||
      language_state_.translation_pending() ||
      language_state_.translation_declined() ||
      language_state_.IsPageTranslated() ||
      !base::FeatureList::IsEnabled(translate::kTranslateUI)) {
    return;
  }

  // Also, skip if the connection is currently offline - initiation doesn't make
  // sense there, either.
  if (net::NetworkChangeNotifier::IsOffline())
    return;

  if (!ignore_missing_key_for_testing_ &&
      !::google_apis::HasAPIKeyConfigured()) {
    // Without an API key, translate won't work, so don't offer to translate in
    // the first place. Leave prefs::kOfferTranslateEnabled on, though, because
    // that settings syncs and we don't want to turn off translate everywhere
    // else.
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_KEY);
    return;
  }

  std::unique_ptr<TranslatePrefs> translate_prefs(
      translate_client_->GetTranslatePrefs());

  if (!translate_prefs->IsOfferTranslateEnabled()) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_PREFS);
    std::string target_lang =
        GetTargetLanguage(translate_prefs.get(), language_model_);
    std::string language_code =
        TranslateDownloadManager::GetLanguageCode(page_lang);
    InitTranslateEvent(language_code, target_lang, *translate_prefs);
    RecordTranslateEvent(metrics::TranslateEventProto::DISABLED_BY_PREF);
    const std::string& locale =
        TranslateDownloadManager::GetInstance()->application_locale();
    TranslateBrowserMetrics::ReportLocalesOnDisabledByPrefs(locale);
    return;
  }

  // MHTML pages currently cannot be translated.
  // See bug: 217945.
  if (translate_driver_->GetContentsMimeType() == "multipart/related") {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_MIME_TYPE_IS_NOT_SUPPORTED);
    return;
  }

  // Don't translate any Chrome specific page, e.g., New Tab Page, Download,
  // History, and so on.
  const GURL& page_url = translate_driver_->GetVisibleURL();
  if (!translate_client_->IsTranslatableURL(page_url)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_URL_IS_NOT_SUPPORTED);
    return;
  }

  std::string language_code =
      TranslateDownloadManager::GetLanguageCode(page_lang);
  const std::set<std::string>& skipped_languages =
      GetSkippedLanguagesForExperiments(language_code, translate_prefs.get());
  std::string target_lang = GetTargetLanguage(
      translate_prefs.get(), language_model_, skipped_languages);

  // Don't translate similar languages (ex: en-US to en).
  if (language_code == target_lang) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_SIMILAR_LANGUAGES);
    return;
  }

  InitTranslateEvent(language_code, target_lang, *translate_prefs);

  // Querying the ranker now, but not exiting immediately so that we may log
  // other potential suppression reasons.
  // Ignore Ranker's decision under triggering experiments since it wasn't
  // trained appropriately under those scenarios.
  bool should_offer_translation =
      language::ShouldPreventRankerEnforcementInIndia(
          translate_prefs->GetForceTriggerOnEnglishPagesCount()) ||
      translate_ranker_->ShouldOfferTranslation(translate_event_.get());

  // Nothing to do if either the language Chrome is in or the language of
  // the page is not supported by the translation server.
  if (target_lang.empty() ||
      !TranslateDownloadManager::IsSupportedLanguage(language_code)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_LANGUAGE_IS_NOT_SUPPORTED);
    TranslateBrowserMetrics::ReportUnsupportedLanguageAtInitiation(
        language_code);
    RecordTranslateEvent(metrics::TranslateEventProto::UNSUPPORTED_LANGUAGE);
    return;
  }

  TranslateAcceptLanguages* accept_languages =
      translate_client_->GetTranslateAcceptLanguages();
  // Don't translate any user black-listed languages.
  if (!translate_prefs->CanTranslateLanguage(accept_languages, language_code)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_CONFIG);
    RecordTranslateEvent(
        metrics::TranslateEventProto::LANGUAGE_DISABLED_BY_USER_CONFIG);
    return;
  }

  // Don't translate any user black-listed URLs.
  if (translate_prefs->IsSiteBlacklisted(page_url.HostNoBrackets())) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_CONFIG);
    RecordTranslateEvent(
        metrics::TranslateEventProto::URL_DISABLED_BY_USER_CONFIG);
    return;
  }

  // If the user has previously selected "always translate" for this language we
  // automatically translate.  Note that in incognito mode we disable that
  // feature; the user will get an infobar, so they can control whether the
  // page's text is sent to the translate server.
  if (!translate_driver_->IsIncognito()) {
    std::string auto_target_lang =
        GetAutoTargetLanguage(language_code, translate_prefs.get());
    if (!auto_target_lang.empty()) {
      TranslateBrowserMetrics::ReportInitiationStatus(
          TranslateBrowserMetrics::INITIATION_STATUS_AUTO_BY_CONFIG);
      translate_event_->set_modified_target_language(auto_target_lang);
      RecordTranslateEvent(
          metrics::TranslateEventProto::AUTO_TRANSLATION_BY_PREF);
      TranslatePage(language_code, auto_target_lang, false);
      return;
    }
  }

  std::string auto_translate_to = language_state_.AutoTranslateTo();
  if (!auto_translate_to.empty()) {
    // This page was navigated through a click from a translated page.
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_AUTO_BY_LINK);
    translate_event_->set_modified_target_language(auto_translate_to);
    RecordTranslateEvent(
        metrics::TranslateEventProto::AUTO_TRANSLATION_BY_LINK);
    TranslatePage(language_code, auto_translate_to, false);
    return;
  }

  // Show the omnibar icon if we've gotten this far.
  language_state_.SetTranslateEnabled(true);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_SHOW_ICON);

  // Will be true if we've decided to show the infobar/bubble UI to the user.
  bool did_show_ui = false;

  if (should_offer_translation) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_SHOW_INFOBAR);

    // If the source language matches the UI language, it means the translation
    // prompt is being forced by an experiment. Report this so the count of how
    // often it happens can be tracked to suppress the experiment as necessary.
    if (language_code ==
        TranslateDownloadManager::GetLanguageCode(
            TranslateDownloadManager::GetInstance()->application_locale())) {
      translate_prefs->ReportForceTriggerOnEnglishPages();
    }

    // Prompts the user if they want the page translated.
    did_show_ui = translate_client_->ShowTranslateUI(
        translate::TRANSLATE_STEP_BEFORE_TRANSLATE, language_code, target_lang,
        TranslateErrors::NONE, false);

  } else {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_ABORTED_BY_RANKER);
    RecordTranslateEvent(metrics::TranslateEventProto::DISABLED_BY_RANKER);
  }

  if (!did_show_ui) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_SUPPRESS_INFOBAR);
  }
}

// static
std::string TranslateManager::GetManualTargetLanguage(
    const std::string& source_code,
    const LanguageState& language_state,
    translate::TranslatePrefs* prefs,
    language::LanguageModel* language_model) {
  if (language_state.IsPageTranslated()) {
    return language_state.current_language();
  } else {
    const std::set<std::string>& skipped_languages =
        GetSkippedLanguagesForExperiments(source_code, prefs);
    return GetTargetLanguage(prefs, language_model, skipped_languages);
  }
}

bool TranslateManager::CanManuallyTranslate() {
  std::unique_ptr<TranslatePrefs> translate_prefs(
      translate_client_->GetTranslatePrefs());
  const std::string source_code = TranslateDownloadManager::GetLanguageCode(
      language_state_.original_language());
  const std::string target_lang = GetManualTargetLanguage(
      source_code, language_state_, translate_prefs.get(), language_model_);

  return language_state_.page_needs_translation() &&
         base::FeatureList::IsEnabled(translate::kTranslateUI) &&
         !net::NetworkChangeNotifier::IsOffline() &&
         (ignore_missing_key_for_testing_ ||
          ::google_apis::HasAPIKeyConfigured()) &&
         // MHTML pages currently cannot be translated (crbug.com/217945).
         translate_driver_->GetContentsMimeType() != "multipart/related" &&
         translate_client_->IsTranslatableURL(
             translate_driver_->GetVisibleURL()) &&
         !target_lang.empty();
}

void TranslateManager::InitiateManualTranslation() {
  std::unique_ptr<TranslatePrefs> translate_prefs(
      translate_client_->GetTranslatePrefs());
  const std::string source_code = TranslateDownloadManager::GetLanguageCode(
      language_state_.original_language());
  const std::string target_lang = GetManualTargetLanguage(
      source_code, language_state_, translate_prefs.get(), language_model_);

  language_state_.SetTranslateEnabled(true);
  const translate::TranslateStep step =
      language_state_.IsPageTranslated()
          ? translate::TRANSLATE_STEP_AFTER_TRANSLATE
          : translate::TRANSLATE_STEP_BEFORE_TRANSLATE;
  translate_client_->ShowTranslateUI(step, source_code, target_lang,
                                     TranslateErrors::NONE, false);
}

void TranslateManager::TranslatePage(const std::string& original_source_lang,
                                     const std::string& target_lang,
                                     bool triggered_from_menu) {
  if (!translate_driver_->HasCurrentPage()) {
    NOTREACHED();
    return;
  }

  // Log the source and target languages of the translate request.
  TranslateBrowserMetrics::ReportTranslateSourceLanguage(original_source_lang);
  TranslateBrowserMetrics::ReportTranslateTargetLanguage(target_lang);

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

  // Trigger the "translating now" UI.
  translate_client_->ShowTranslateUI(
      translate::TRANSLATE_STEP_TRANSLATING, source_lang, target_lang,
      TranslateErrors::NONE, triggered_from_menu);

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
      base::Bind(&TranslateManager::OnTranslateScriptFetchComplete,
                 GetWeakPtr(), source_lang, target_lang);

  script->Request(callback, translate_driver_->IsIncognito());
}

void TranslateManager::RevertTranslation() {
  // Capture the revert event in the translate metrics
  RecordTranslateEvent(metrics::TranslateEventProto::USER_REVERT);

  // Revert the translation.
  translate_driver_->RevertTranslation(page_seq_no_);
  language_state_.SetCurrentLanguage(language_state_.original_language());
}

void TranslateManager::ReportLanguageDetectionError() {
  TranslateBrowserMetrics::ReportLanguageDetectionError();

  GURL report_error_url = GURL(kReportLanguageDetectionErrorURL);

  report_error_url = net::AppendQueryParameter(
      report_error_url, kUrlQueryName,
      translate_driver_->GetLastCommittedURL().spec());

  report_error_url =
      net::AppendQueryParameter(report_error_url, kSourceLanguageQueryName,
                                language_state_.original_language());

  report_error_url = translate::AddHostLocaleToUrl(report_error_url);
  report_error_url = translate::AddApiKeyToUrl(report_error_url);

  translate_client_->ShowReportLanguageDetectionErrorUI(report_error_url);
}

void TranslateManager::DoTranslatePage(const std::string& translate_script,
                                       const std::string& source_lang,
                                       const std::string& target_lang) {
  language_state_.set_translation_pending(true);
  translate_driver_->TranslatePage(page_seq_no_, translate_script, source_lang,
                                   target_lang);
}

// Notifies |g_callback_list_| of translate errors.
void TranslateManager::NotifyTranslateError(TranslateErrors::Type error_type) {
  if (!g_callback_list_ || error_type == TranslateErrors::NONE ||
      translate_driver_->IsIncognito()) {
    return;
  }

  TranslateErrorDetails error_details;
  error_details.time = base::Time::Now();
  error_details.url = translate_driver_->GetLastCommittedURL();
  error_details.error = error_type;
  g_callback_list_->Notify(error_details);
}

void TranslateManager::PageTranslated(const std::string& source_lang,
                                      const std::string& target_lang,
                                      TranslateErrors::Type error_type) {
  if (error_type == TranslateErrors::NONE)
    language_state_.SetCurrentLanguage(target_lang);

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
  NotifyTranslateError(error_type);
}

void TranslateManager::OnTranslateScriptFetchComplete(
    const std::string& source_lang,
    const std::string& target_lang,
    bool success,
    const std::string& data) {
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
  }
}

// static
std::string TranslateManager::GetTargetLanguage(
    const TranslatePrefs* prefs,
    language::LanguageModel* language_model,
    const std::set<std::string>& skipped_languages) {
  DCHECK(prefs);
  const std::string& recent_target = prefs->GetRecentTargetLanguage();

  // If we've recorded the most recent target language, use that.
  if (base::FeatureList::IsEnabled(kTranslateRecentTarget) &&
      !recent_target.empty()) {
    return recent_target;
  }

  if (language_model) {
    std::vector<std::string> language_codes;
    for (const auto& lang : language_model->GetLanguages()) {
      std::string lang_code =
          TranslateDownloadManager::GetLanguageCode(lang.lang_code);
      translate::ToTranslateLanguageSynonym(&lang_code);
      if (TranslateDownloadManager::IsSupportedLanguage(lang_code))
        language_codes.push_back(lang_code);
    }
    // If some languages need to be skipped, move them to the end of the
    // language vector so that any other eligible language takes priority.
    MoveSkippedLanguagesToEndIfNecessary(&language_codes, skipped_languages);

    // Use the first language from the model that translate supports.
    if (!language_codes.empty())
      return language_codes[0];
  } else {
    // Get the browser's user interface language.
    std::string language = TranslateDownloadManager::GetLanguageCode(
        TranslateDownloadManager::GetInstance()->application_locale());
    // Map 'he', 'nb', 'fil' back to 'iw', 'no', 'tl'
    translate::ToTranslateLanguageSynonym(&language);
    if (TranslateDownloadManager::IsSupportedLanguage(language))
      return language;

    // Will translate to the first supported language on the Accepted Language
    // list or not at all if no such candidate exists.
    std::vector<std::string> accept_languages_list;
    prefs->GetLanguageList(&accept_languages_list);
    for (const auto& lang : accept_languages_list) {
      std::string lang_code = TranslateDownloadManager::GetLanguageCode(lang);
      if (TranslateDownloadManager::IsSupportedLanguage(lang_code))
        return lang_code;
    }
  }

  return std::string();
}

// static
std::string TranslateManager::GetTargetLanguage(
    const TranslatePrefs* prefs,
    language::LanguageModel* language_model) {
  return GetTargetLanguage(prefs, language_model, {});
}

// static
std::string TranslateManager::GetAutoTargetLanguage(
    const std::string& original_language,
    TranslatePrefs* translate_prefs) {
  std::string auto_target_lang;
  if (translate_prefs->ShouldAutoTranslate(original_language,
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

LanguageState& TranslateManager::GetLanguageState() {
  return language_state_;
}

bool TranslateManager::ignore_missing_key_for_testing_ = false;

// static
void TranslateManager::SetIgnoreMissingKeyForTesting(bool ignore) {
  ignore_missing_key_for_testing_ = ignore;
}

// static
bool TranslateManager::IsAvailable(const TranslatePrefs* prefs) {
  // These conditions mirror the conditions in InitiateTranslation.
  return base::FeatureList::IsEnabled(translate::kTranslateUI) &&
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
      event_type, translate_driver_->GetVisibleURL(), translate_event_.get());
  translate_client_->RecordTranslateEvent(*translate_event_);
}

bool TranslateManager::ShouldOverrideDecision(int event_type) {
  return translate_ranker_->ShouldOverrideDecision(
      event_type, translate_driver_->GetVisibleURL(), translate_event_.get());
}

bool TranslateManager::ShouldSuppressBubbleUI(
    bool triggered_from_menu,
    const std::string& source_language) {
  // Suppress the UI if the user navigates to a page with
  // the same language as the previous page. In the new UI,
  // continue offering translation after the user navigates
  // to another page.
  if (!language_state_.HasLanguageChanged() &&
      !ShouldOverrideDecision(
          metrics::TranslateEventProto::MATCHES_PREVIOUS_LANGUAGE)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::
            INITIATION_STATUS_ABORTED_BY_MATCHES_PREVIOUS_LANGUAGE);
    return true;
  }

  // Suppress the UI if the user denied translation for this language
  // too often.
  if (!triggered_from_menu &&
      translate_client_->GetTranslatePrefs()->IsTooOftenDenied(
          source_language) &&
      !ShouldOverrideDecision(
          metrics::TranslateEventProto::LANGUAGE_DISABLED_BY_AUTO_BLACKLIST)) {
    TranslateBrowserMetrics::ReportInitiationStatus(
        TranslateBrowserMetrics::INITIATION_STATUS_ABORTED_BY_TOO_OFTEN_DENIED);
    return true;
  }

  return false;
}

void TranslateManager::AddTargetLanguageToAcceptLanguages(
    const std::string& target_language_code) {
  std::string target_language, tail;
  // |target_language_code| should satisfy BCP47 and consist of a language code
  // and an optional region code joined by an hyphen.
  language::SplitIntoMainAndTail(target_language_code, &target_language, &tail);

  std::function<bool(const std::string&)> is_redundant;
  if (tail.empty()) {
    is_redundant = [&target_language](const std::string& language) {
      return language::ExtractBaseLanguage(language) == target_language;
    };
  } else {
    is_redundant = [&target_language_code](const std::string& language) {
      return language == target_language_code;
    };
  }

  auto prefs = translate_client_->GetTranslatePrefs();
  std::vector<std::string> languages;
  prefs->GetLanguageList(&languages);

  if (std::none_of(languages.begin(), languages.end(), is_redundant)) {
    prefs->AddToLanguageList(target_language_code, /*force_blocked=*/false);
  }
}

}  // namespace translate
