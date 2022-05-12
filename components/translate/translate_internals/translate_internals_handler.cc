// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/translate_internals/translate_internals_handler.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
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
    NOTREACHED();
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
base::Value TranslateInternalsHandler::GetLanguages() {
  base::Value dict(base::Value::Type::DICTIONARY);

  const std::string app_locale =
      translate::TranslateDownloadManager::GetInstance()->application_locale();
  std::vector<std::string> language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &language_codes);

  for (auto& lang_code : language_codes) {
    std::u16string lang_name =
        l10n_util::GetDisplayNameForLocale(lang_code, app_locale, false);
    dict.SetStringKey(lang_code, lang_name);
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
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetDoubleKey("time", details.time.ToJsTime());
  dict.SetStringKey("url", details.url.spec());
  dict.SetStringKey("content_language", details.content_language);
  dict.SetStringKey("model_detected_language", details.model_detected_language);
  dict.SetBoolKey("is_model_reliable", details.is_model_reliable);
  dict.SetDoubleKey("model_reliability_score", details.model_reliability_score);
  dict.SetBoolKey("has_notranslate", details.has_notranslate);
  dict.SetStringKey("html_root_language", details.html_root_language);
  dict.SetStringKey("adopted_language", details.adopted_language);
  dict.SetStringKey("content", details.contents);
  dict.SetStringKey("detection_model_version", details.detection_model_version);
  SendMessageToJs("languageDetectionInfoAdded", dict);
}

void TranslateInternalsHandler::OnTranslateError(
    const translate::TranslateErrorDetails& details) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetDoubleKey("time", details.time.ToJsTime());
  dict.SetStringKey("url", details.url.spec());
  dict.SetIntKey("error", details.error);
  SendMessageToJs("translateErrorDetailsAdded", dict);
}

void TranslateInternalsHandler::OnTranslateInit(
    const translate::TranslateInitDetails& details) {
  if (!GetTranslateClient()->IsTranslatableURL(details.url))
    return;
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetDoubleKey("time", details.time.ToJsTime());
  dict.SetStringKey("url", details.url.spec());

  dict.SetStringKey("page_language_code", details.page_language_code);
  dict.SetStringKey("target_lang", details.target_lang);

  dict.SetBoolKey("can_auto_translate", details.decision.can_auto_translate());
  dict.SetBoolKey("can_show_ui", details.decision.can_show_ui());
  dict.SetBoolKey("can_auto_href_translate",
                  details.decision.can_auto_href_translate());
  dict.SetBoolKey("can_show_href_translate_ui",
                  details.decision.can_show_href_translate_ui());
  dict.SetBoolKey("can_show_predefined_language_translate_ui",
                  details.decision.can_show_predefined_language_translate_ui());
  dict.SetBoolKey("should_suppress_from_ranker",
                  details.decision.should_suppress_from_ranker());
  dict.SetBoolKey("is_triggering_possible",
                  details.decision.IsTriggeringPossible());
  dict.SetBoolKey("should_auto_translate",
                  details.decision.ShouldAutoTranslate());
  dict.SetBoolKey("should_show_ui", details.decision.ShouldShowUI());

  dict.SetStringKey("auto_translate_target",
                    details.decision.auto_translate_target);
  dict.SetStringKey("href_translate_target",
                    details.decision.href_translate_target);
  dict.SetStringKey("predefined_translate_target",
                    details.decision.predefined_translate_target);

  dict.SetBoolKey("ui_shown", details.ui_shown);
  SendMessageToJs("translateInitDetailsAdded", dict);
}

void TranslateInternalsHandler::OnTranslateEvent(
    const translate::TranslateEventDetails& details) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetDoubleKey("time", details.time.ToJsTime());
  dict.SetStringKey("filename", details.filename);
  dict.SetIntKey("line", details.line);
  dict.SetStringKey("message", details.message);
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
    const std::string& to = args[2].GetString();
    translate_prefs->RemoveLanguagePairFromAlwaysTranslateList(from, to);
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

void TranslateInternalsHandler::SendMessageToJs(const std::string& message,
                                                const base::Value& value) {
  const char func[] = "cr.webUIListenerCallback";
  base::Value message_data(message);
  std::vector<const base::Value*> args{&message_data, &value};
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
      translate::TranslatePrefs::kPrefNeverPromptSitesWithTime,
      translate::TranslatePrefs::kPrefTranslateDeniedCount,
      translate::TranslatePrefs::kPrefTranslateIgnoredCount,
      translate::TranslatePrefs::kPrefTranslateAcceptedCount,
  };

  base::Value dict(base::Value::Type::DICTIONARY);
  for (const char* key : keys) {
    const PrefService::Preference* pref = prefs->FindPreference(key);
    if (pref)
      dict.SetKey(translate::TranslatePrefs::MapPreferenceName(key),
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

  base::Value languages_list(base::Value::Type::LIST);
  for (std::string& lang : languages)
    languages_list.Append(std::move(lang));

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("languages", std::move(languages_list));
  dict.SetDoubleKey("last_updated", last_updated.ToJsTime());
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

  base::Value dict(base::Value::Type::DICTIONARY);
  if (!country.empty()) {
    dict.SetStringKey("country", country);
    dict.SetBoolKey("update", was_updated);
    dict.SetBoolKey("overridden", !overridden_country.empty());
  }
  SendMessageToJs("countryUpdated", dict);
}

}  // namespace translate
