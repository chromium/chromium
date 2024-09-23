// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/translate_internals/translate_internals_handler.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_error_details.h"
#include "components/translate/core/browser/translate_event_details.h"
#include "components/translate/core/browser/translate_init_details.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/variations/service/variations_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace translate {

TranslateInternalsHandler::TranslateInternalsHandler() {
  translate::TranslateLanguageList* language_list =
      translate::TranslateDownloadManager::GetInstance()->language_list();
  if (!language_list) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  error_subscription_ =
      translate::TranslateManager::RegisterTranslateErrorCallback(
          base::BindRepeating(&TranslateInternalsHandler::OnTranslateError,
                              base::Unretained(this)));

  init_subscription_ =
      translate::TranslateManager::RegisterTranslateInitCallback(
          base::BindRepeating(&TranslateInternalsHandler::OnTranslateInit,
                              base::Unretained(this)));

  event_subscription_ = language_list->RegisterEventCallback(
      base::BindRepeating(&TranslateInternalsHandler::OnTranslateEvent,
                          base::Unretained(this)));
}

TranslateInternalsHandler::~TranslateInternalsHandler() = default;

// static.
base::Value::Dict TranslateInternalsHandler::GetLanguages() {
  base::Value::Dict dict;

  const std::string app_locale =
      translate::TranslateDownloadManager::GetInstance()->application_locale();
  std::vector<std::string> language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &language_codes);

  for (auto& lang_code : language_codes) {
    std::u16string lang_name =
        l10n_util::GetDisplayNameForLocale(lang_code, app_locale, false);
    dict.Set(lang_code, lang_name);
  }
  return dict;
}

void TranslateInternalsHandler::RegisterMessageCallbacks() {
  RegisterMessageCallback(
      "removePrefItem",
      base::BindRepeating(&TranslateInternalsHandler::OnRemovePrefItem,
                          base::Unretained(this)));
  RegisterMessageCallback(
      "setRecentTargetLanguage",
      base::BindRepeating(&TranslateInternalsHandler::OnSetRecentTargetLanguage,
                          base::Unretained(this)));
  RegisterMessageCallback(
      "requestInfo",
      base::BindRepeating(&TranslateInternalsHandler::OnRequestInfo,
                          base::Unretained(this)));
  RegisterMessageCallback(
      "overrideCountry",
      base::BindRepeating(&TranslateInternalsHandler::OnOverrideCountry,
                          base::Unretained(this)));
}

void TranslateInternalsHandler::AddLanguageDetectionDetails(
    const translate::LanguageDetectionDetails& details) {
  base::Value::Dict dict;
  dict.Set("has_run_lang_detection", details.has_run_lang_detection);
  dict.Set("time", details.time.InMillisecondsFSinceUnixEpoch());
  dict.Set("url", details.url.spec());
  dict.Set("content_language", details.content_language);
  dict.Set("model_detected_language", details.model_detected_language);
  dict.Set("is_model_reliable", details.is_model_reliable);
  dict.Set("model_reliability_score", details.model_reliability_score);
  dict.Set("has_notranslate", details.has_notranslate);
  dict.Set("html_root_language", details.html_root_language);
  dict.Set("adopted_language", details.adopted_language);
  dict.Set("content", details.contents);
  dict.Set("detection_model_version", details.detection_model_version);
  SendMessageToJs("languageDetectionInfoAdded", dict);
}

void TranslateInternalsHandler::OnTranslateError(
    const translate::TranslateErrorDetails& details) {
  base::Value::Dict dict;
  dict.Set("time", details.time.InMillisecondsFSinceUnixEpoch());
  dict.Set("url", details.url.spec());
  dict.Set("error", base::to_underlying(details.error));
  SendMessageToJs("translateErrorDetailsAdded", dict);
}

void TranslateInternalsHandler::OnTranslateInit(
    const translate::TranslateInitDetails& details) {
  if (!GetTranslateClient()->IsTranslatableURL(details.url))
    return;
  base::Value::Dict dict;
  dict.Set("time", details.time.InMillisecondsFSinceUnixEpoch());
  dict.Set("url", details.url.spec());

  dict.Set("page_language_code", details.page_language_code);
  dict.Set("target_lang", details.target_lang);

  dict.Set("can_auto_translate", details.decision.can_auto_translate());
  dict.Set("can_show_ui", details.decision.can_show_ui());
  dict.Set("can_auto_href_translate",
           details.decision.can_auto_href_translate());
  dict.Set("can_show_href_translate_ui",
           details.decision.can_show_href_translate_ui());
  dict.Set("can_show_predefined_language_translate_ui",
           details.decision.can_show_predefined_language_translate_ui());
  dict.Set("should_suppress_from_ranker",
           details.decision.should_suppress_from_ranker());
  dict.Set("is_triggering_possible", details.decision.IsTriggeringPossible());
  dict.Set("should_auto_translate", details.decision.ShouldAutoTranslate());
  dict.Set("should_show_ui", details.decision.ShouldShowUI());

  dict.Set("auto_translate_target", details.decision.auto_translate_target);
  dict.Set("href_translate_target", details.decision.href_translate_target);
  dict.Set("predefined_translate_target",
           details.decision.predefined_translate_target);

  dict.Set("ui_shown", details.ui_shown);
  SendMessageToJs("translateInitDetailsAdded", dict);
}

void TranslateInternalsHandler::OnTranslateEvent(
    const translate::TranslateEventDetails& details) {
  base::Value::Dict dict;
  dict.Set("time", details.time.InMillisecondsFSinceUnixEpoch());
  dict.Set("filename", details.filename);
  dict.Set("line", details.line);
  dict.Set("message", details.message);
  SendMessageToJs("translateEventDetailsAdded", dict);
}

void TranslateInternalsHandler::OnRemovePrefItem(
    const base::Value::List& args) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      GetTranslateClient()->GetTranslatePrefs();

  if (!args[0].is_string())
    return;

  const std::string& pref_name = args[0].GetString();
  if (pref_name == "blocked_languages") {
    if (!args[1].is_string())
      return;
    const std::string& language = args[1].GetString();
    translate_prefs->UnblockLanguage(language);
  } else if (pref_name == "site_blocklist") {
    if (!args[1].is_string())
      return;
    const std::string& site = args[1].GetString();
    translate_prefs->RemoveSiteFromNeverPromptList(site);
  } else if (pref_name == "allowlists") {
    if (!args[1].is_string())
      return;
    if (!args[2].is_string())
      return;
    const std::string& from = args[1].GetString();
    translate_prefs->RemoveLanguagePairFromAlwaysTranslateList(from);
  } else {
    return;
  }

  SendPrefsToJs();
}

void TranslateInternalsHandler::OnSetRecentTargetLanguage(
    const base::Value::List& args) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      GetTranslateClient()->GetTranslatePrefs();

  if (!args[0].is_string())
    return;

  const std::string& new_value = args[0].GetString();
  translate_prefs->SetRecentTargetLanguage(new_value);

  SendPrefsToJs();
}

void TranslateInternalsHandler::OnOverrideCountry(
    const base::Value::List& args) {
  if (args[0].is_string()) {
    const std::string& country = args[0].GetString();
    variations::VariationsService* variations_service = GetVariationsService();
    SendCountryToJs(
        variations_service->OverrideStoredPermanentCountry(country));
  }
}

void TranslateInternalsHandler::OnRequestInfo(
    const base::Value::List& /*args*/) {
  SendPrefsToJs();
  SendSupportedLanguagesToJs();
  SendCountryToJs(false);
}

void TranslateInternalsHandler::SendMessageToJs(
    std::string_view message,
    const base::Value::Dict& value) {
  const char func[] = "cr.webUIListenerCallback";
  base::Value message_data(message);
  base::ValueView args[] = {message_data, value};
  CallJavascriptFunction(func, args);
}

void TranslateInternalsHandler::SendPrefsToJs() {
  PrefService* prefs = GetTranslateClient()->GetPrefs();

  static const char* const keys[] = {
      language::prefs::kAcceptLanguages,
      prefs::kBlockedLanguages,
      prefs::kOfferTranslateEnabled,
      prefs::kPrefAlwaysTranslateList,
      prefs::kPrefTranslateRecentTarget,
      translate::TranslatePrefs::kPrefNeverPromptSitesDeprecated,
      prefs::kPrefNeverPromptSitesWithTime,
      translate::TranslatePrefs::kPrefTranslateDeniedCount,
      translate::TranslatePrefs::kPrefTranslateIgnoredCount,
      translate::TranslatePrefs::kPrefTranslateAcceptedCount,
  };

  base::Value::Dict dict;
  for (const char* key : keys) {
    const PrefService::Preference* pref = prefs->FindPreference(key);
    if (pref)
      dict.Set(translate::TranslatePrefs::MapPreferenceName(key),
               pref->GetValue()->Clone());
  }

  SendMessageToJs("prefsUpdated", dict);
}

void TranslateInternalsHandler::SendSupportedLanguagesToJs() {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      GetTranslateClient()->GetTranslatePrefs();

  // Fetch supported language information.
  std::vector<std::string> languages;
  translate::TranslateDownloadManager::GetSupportedLanguages(
      translate_prefs->IsTranslateAllowedByPolicy(), &languages);
  base::Time last_updated =
      translate::TranslateDownloadManager::GetSupportedLanguagesLastUpdated();

  base::Value::List languages_list;
  for (std::string& lang : languages)
    languages_list.Append(std::move(lang));

  base::Value::Dict dict;
  dict.Set("languages", std::move(languages_list));
  dict.Set("last_updated", last_updated.InMillisecondsFSinceUnixEpoch());
  SendMessageToJs("supportedLanguagesUpdated", dict);
}

void TranslateInternalsHandler::SendCountryToJs(bool was_updated) {
  std::string country, overridden_country;
  variations::VariationsService* variations_service = GetVariationsService();
  // The |country| will get the overridden country when it exists. The
  // |overridden_country| is used to check if the overridden country exists or
  // not and disable/enable the clear button.
  country = variations_service->GetStoredPermanentCountry();
  overridden_country = variations_service->GetOverriddenPermanentCountry();

  base::Value::Dict dict;
  if (!country.empty()) {
    dict.Set("country", country);
    dict.Set("update", was_updated);
    dict.Set("overridden", !overridden_country.empty());
  }
  SendMessageToJs("countryUpdated", dict);
}

}  // namespace translate
