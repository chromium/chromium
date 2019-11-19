// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/translate_internals/translate_internals_handler.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
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

TranslateInternalsHandler::~TranslateInternalsHandler() {
  // |event_subscription_|, |error_subscription_| and |init_subscription_| are
  // deleted automatically and un-register the callbacks automatically.
}

// static.
void TranslateInternalsHandler::GetLanguages(base::DictionaryValue* dict) {
  DCHECK(dict);

  const std::string app_locale =
      translate::TranslateDownloadManager::GetInstance()->application_locale();
  std::vector<std::string> language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &language_codes);

  for (auto it = language_codes.begin(); it != language_codes.end(); ++it) {
    const std::string& lang_code = *it;
    base::string16 lang_name =
        l10n_util::GetDisplayNameForLocale(lang_code, app_locale, false);
    dict->SetString(lang_code, lang_name);
  }
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
  base::DictionaryValue dict;
  dict.SetDouble("time", details.time.ToJsTime());
  dict.SetString("url", details.url.spec());
  dict.SetString("content_language", details.content_language);
  dict.SetString("cld_language", details.cld_language);
  dict.SetBoolean("is_cld_reliable", details.is_cld_reliable);
  dict.SetBoolean("has_notranslate", details.has_notranslate);
  dict.SetString("html_root_language", details.html_root_language);
  dict.SetString("adopted_language", details.adopted_language);
  dict.SetString("content", details.contents);
  SendMessageToJs("languageDetectionInfoAdded", dict);
}

void TranslateInternalsHandler::OnTranslateError(
    const translate::TranslateErrorDetails& details) {
  base::DictionaryValue dict;
  dict.SetDouble("time", details.time.ToJsTime());
  dict.SetString("url", details.url.spec());
  dict.SetInteger("error", details.error);
  SendMessageToJs("translateErrorDetailsAdded", dict);
}

void TranslateInternalsHandler::OnTranslateInit(
    const translate::TranslateInitDetails& details) {
  if (!GetTranslateClient()->IsTranslatableURL(details.url))
    return;
  base::DictionaryValue dict;
  dict.SetKey("time", base::Value(details.time.ToJsTime()));
  dict.SetKey("url", base::Value(details.url.spec()));

  dict.SetKey("page_language_code", base::Value(details.page_language_code));
  dict.SetKey("target_lang", base::Value(details.target_lang));

  dict.SetKey("can_auto_translate",
              base::Value(details.decision.can_auto_translate()));
  dict.SetKey("can_show_ui", base::Value(details.decision.can_show_ui()));
  dict.SetKey("can_auto_href_translate",
              base::Value(details.decision.can_auto_href_translate()));
  dict.SetKey("can_show_href_translate_ui",
              base::Value(details.decision.can_show_href_translate_ui()));
  dict.SetKey(
      "can_show_predefined_language_translate_ui",
      base::Value(
          details.decision.can_show_predefined_language_translate_ui()));
  dict.SetKey("should_suppress_from_ranker",
              base::Value(details.decision.should_suppress_from_ranker()));
  dict.SetKey("is_triggering_possible",
              base::Value(details.decision.IsTriggeringPossible()));
  dict.SetKey("should_auto_translate",
              base::Value(details.decision.ShouldAutoTranslate()));
  dict.SetKey("should_show_ui", base::Value(details.decision.ShouldShowUI()));

  dict.SetKey("auto_translate_target",
              base::Value(details.decision.auto_translate_target));
  dict.SetKey("href_translate_target",
              base::Value(details.decision.href_translate_target));
  dict.SetKey("predefined_translate_target",
              base::Value(details.decision.predefined_translate_target));

  dict.SetKey("ui_shown", base::Value(details.ui_shown));
  SendMessageToJs("translateInitDetailsAdded", dict);
}

void TranslateInternalsHandler::OnTranslateEvent(
    const translate::TranslateEventDetails& details) {
  base::DictionaryValue dict;
  dict.SetDouble("time", details.time.ToJsTime());
  dict.SetString("filename", details.filename);
  dict.SetInteger("line", details.line);
  dict.SetString("message", details.message);
  SendMessageToJs("translateEventDetailsAdded", dict);
}

void TranslateInternalsHandler::OnRemovePrefItem(const base::ListValue* args) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      GetTranslateClient()->GetTranslatePrefs();

  std::string pref_name;
  if (!args->GetString(0, &pref_name))
    return;

  if (pref_name == "blocked_languages") {
    std::string language;
    if (!args->GetString(1, &language))
      return;
    translate_prefs->UnblockLanguage(language);
  } else if (pref_name == "site_blacklist") {
    std::string site;
    if (!args->GetString(1, &site))
      return;
    translate_prefs->RemoveSiteFromBlacklist(site);
  } else if (pref_name == "whitelists") {
    std::string from, to;
    if (!args->GetString(1, &from))
      return;
    if (!args->GetString(2, &to))
      return;
    translate_prefs->RemoveLanguagePairFromWhitelist(from, to);
  } else if (pref_name == "too_often_denied") {
    translate_prefs->ResetDenialState();
  } else {
    return;
  }

  SendPrefsToJs();
}

void TranslateInternalsHandler::OnSetRecentTargetLanguage(
    const base::ListValue* args) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      GetTranslateClient()->GetTranslatePrefs();

  std::string new_value;
  if (!args->GetString(0, &new_value))
    return;

  translate_prefs->SetRecentTargetLanguage(new_value);

  SendPrefsToJs();
}

void TranslateInternalsHandler::OnOverrideCountry(const base::ListValue* args) {
  std::string country;
  if (args->GetString(0, &country)) {
    variations::VariationsService* variations_service = GetVariationsService();
    SendCountryToJs(
        variations_service->OverrideStoredPermanentCountry(country));
  }
}

void TranslateInternalsHandler::OnRequestInfo(const base::ListValue* /*args*/) {
  SendPrefsToJs();
  SendSupportedLanguagesToJs();
  SendCountryToJs(false);
}

void TranslateInternalsHandler::SendMessageToJs(const std::string& message,
                                                const base::Value& value) {
  const char func[] = "cr.translateInternals.messageHandler";
  base::Value message_data(message);
  std::vector<const base::Value*> args{&message_data, &value};
  CallJavascriptFunction(func, args);
}

void TranslateInternalsHandler::SendPrefsToJs() {
  PrefService* prefs = GetTranslateClient()->GetPrefs();

  base::DictionaryValue dict;

  static const char* const keys[] = {
      language::prefs::kFluentLanguages,
      prefs::kOfferTranslateEnabled,
      translate::TranslatePrefs::kPrefTranslateRecentTarget,
      translate::TranslatePrefs::kPrefTranslateSiteBlacklistDeprecated,
      translate::TranslatePrefs::kPrefTranslateSiteBlacklistWithTime,
      translate::TranslatePrefs::kPrefTranslateWhitelists,
      translate::TranslatePrefs::kPrefTranslateDeniedCount,
      translate::TranslatePrefs::kPrefTranslateIgnoredCount,
      translate::TranslatePrefs::kPrefTranslateAcceptedCount,
      translate::TranslatePrefs::kPrefTranslateLastDeniedTimeForLanguage,
      translate::TranslatePrefs::kPrefTranslateTooOftenDeniedForLanguage,
      language::prefs::kAcceptLanguages,
  };
  for (const char* key : keys) {
    const PrefService::Preference* pref = prefs->FindPreference(key);
    if (pref)
      dict.SetKey(key, pref->GetValue()->Clone());
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

  auto languages_list = std::make_unique<base::ListValue>();
  for (const std::string& lang : languages)
    languages_list->AppendString(lang);

  base::DictionaryValue dict;
  dict.Set("languages", std::move(languages_list));
  dict.SetDouble("last_updated", last_updated.ToJsTime());
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

  base::DictionaryValue dict;
  if (!country.empty()) {
    dict.SetString("country", country);
    dict.SetBoolean("update", was_updated);
    dict.SetBoolean("overridden", !overridden_country.empty());
  }
  SendMessageToJs("countryUpdated", dict);
}

}  // namespace translate
