// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/apc_internals/apc_internals_handler.h"

#include <array>
#include <string>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/metrics/field_trial_params.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill_assistant/password_change/apc_client.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_scripts_fetcher_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/apc_internals/apc_internals_logins_request.h"
#include "components/autofill_assistant/browser/public/prefs.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

using password_manager::PasswordScriptsFetcher;

namespace {

// TODO(1311324): Reduce the level of code duplication between
// autofill_assistant::ClientAndroid and the helper method in
// chrome/browser/password_manager/password_scripts_fetcher_factory.cc.
std::string GetCountryCode() {
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  // Use fallback "ZZ" if no country is available.
  if (!variations_service || variations_service->GetLatestCountry().empty())
    return "ZZ";
  return base::ToUpperASCII(variations_service->GetLatestCountry());
}

const std::array<base::StringPiece, 4>& GetAssistantPrefs() {
  static const std::array<base::StringPiece, 4> prefs = {
      autofill_assistant::prefs::kAutofillAssistantEnabled,
      autofill_assistant::prefs::kAutofillAssistantConsent,
      autofill_assistant::prefs::kAutofillAssistantTriggerScriptsEnabled,
      autofill_assistant::prefs::
          kAutofillAssistantTriggerScriptsIsFirstTimeUser,
  };
  return prefs;
}

}  // namespace

APCInternalsHandler::APCInternalsHandler() = default;

APCInternalsHandler::~APCInternalsHandler() = default;

void APCInternalsHandler::RegisterMessages() {
  password_manager::PasswordManagerClient* password_manager_client =
      ChromePasswordManagerClient::FromWebContents(web_ui()->GetWebContents());
  profile_password_store_ = password_manager_client->GetProfilePasswordStore();
  account_password_store_ = password_manager_client->GetAccountPasswordStore();

  web_ui()->RegisterMessageCallback(
      "loaded", base::BindRepeating(&APCInternalsHandler::OnLoaded,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "get-script-cache",
      base::BindRepeating(&APCInternalsHandler::OnScriptCacheRequested,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "refresh-script-cache",
      base::BindRepeating(&APCInternalsHandler::OnRefreshScriptCacheRequested,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "toggle-user-pref",
      base::BindRepeating(&APCInternalsHandler::OnToggleUserPref,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "remove-user-pref",
      base::BindRepeating(&APCInternalsHandler::OnRemoveUserPref,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "set-autofill-assistant-url",
      base::BindRepeating(&APCInternalsHandler::OnSetAutofillAssistantUrl,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "set-autofill-assistant-country-code",
      base::BindRepeating(
          &APCInternalsHandler::OnSetAutofillAssistantCountryCode,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "launch-script",
      base::BindRepeating(&APCInternalsHandler::GetLoginsAndTryLaunchScript,
                          base::Unretained(this)));
}

void APCInternalsHandler::OnLoaded(const base::Value::List& args) {
  AllowJavascript();

  // Provide information for initial page creation.
  FireWebUIListener("on-flags-information-received", GetAPCRelatedFlags());
  FireWebUIListener("on-script-fetching-information-received",
                    GetPasswordScriptFetcherInformation());
  UpdatePrefsInformation();
  UpdateAutofillAssistantInformation();
  OnRefreshScriptCacheRequested(base::Value::List());
}

void APCInternalsHandler::UpdatePrefsInformation() {
  FireWebUIListener("on-prefs-information-received", GetAPCRelatedPrefs());
}

void APCInternalsHandler::UpdateAutofillAssistantInformation() {
  FireWebUIListener("on-autofill-assistant-information-received",
                    GetAutofillAssistantInformation());
}

void APCInternalsHandler::OnScriptCacheRequested(
    const base::Value::List& args) {
  FireWebUIListener("on-script-cache-received",
                    GetPasswordScriptFetcherCache());
}

void APCInternalsHandler::OnRefreshScriptCacheRequested(
    const base::Value::List& args) {
  if (PasswordScriptsFetcher* scripts_fetcher = GetPasswordScriptsFetcher();
      scripts_fetcher) {
    scripts_fetcher->RefreshScriptsIfNecessary(
        base::BindOnce(&APCInternalsHandler::OnScriptCacheRequested,
                       weak_ptr_factory_.GetWeakPtr(), base::Value::List()));
  }
}

void APCInternalsHandler::OnToggleUserPref(const base::Value::List& args) {
  if (args.size() == 1 && args.front().is_string()) {
    const std::string& pref_name = args.front().GetString();
    // Only allow modifying the prefs that are supposed to be shown here.
    CHECK(base::Contains(GetAssistantPrefs(), pref_name));

    PrefService* pref_service =
        Profile::FromBrowserContext(
            web_ui()->GetWebContents()->GetBrowserContext())
            ->GetPrefs();
    CHECK(pref_service);
    pref_service->SetBoolean(pref_name, !pref_service->GetBoolean(pref_name));
    UpdatePrefsInformation();
  }
}

void APCInternalsHandler::OnRemoveUserPref(const base::Value::List& args) {
  if (args.size() == 1 && args.front().is_string()) {
    const std::string& pref_name = args.front().GetString();
    // Only allow removing the prefs that are supposed to be shown here.
    CHECK(base::Contains(GetAssistantPrefs(), pref_name));

    PrefService* pref_service =
        Profile::FromBrowserContext(
            web_ui()->GetWebContents()->GetBrowserContext())
            ->GetPrefs();
    CHECK(pref_service);
    pref_service->ClearPref(pref_name);
    UpdatePrefsInformation();
  }
}

void APCInternalsHandler::OnSetAutofillAssistantUrl(
    const base::Value::List& args) {
  if (args.size() == 1 && args.front().is_string()) {
    const std::string& autofill_assistant_url = args.front().GetString();
    auto* command_line = base::CommandLine::ForCurrentProcess();

    command_line->RemoveSwitch(
        autofill_assistant::switches::kAutofillAssistantUrl);

    command_line->AppendSwitchASCII(
        autofill_assistant::switches::kAutofillAssistantUrl,
        autofill_assistant_url);

    UpdateAutofillAssistantInformation();
  }
}

void APCInternalsHandler::OnSetAutofillAssistantCountryCode(
    const base::Value::List& args) {
  if (args.size() == 1 && args.front().is_string()) {
    const std::string& autofill_assistant_country_code =
        args.front().GetString();
    auto* command_line = base::CommandLine::ForCurrentProcess();

    command_line->RemoveSwitch(
        variations::switches::kVariationsOverrideCountry);

    command_line->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry,
        autofill_assistant_country_code);

    UpdateAutofillAssistantInformation();
  }
}

PasswordScriptsFetcher* APCInternalsHandler::GetPasswordScriptsFetcher() {
  return PasswordScriptsFetcherFactory::GetForBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());
}

// Returns a list of dictionaries that contain the name and the state of
// each APC-related feature.
base::Value::List APCInternalsHandler::GetAPCRelatedFlags() const {
  // We must use pointers to the features instead of copying the features,
  // because base::FeatureList::CheckFeatureIdentity (asserted, e.g., in
  // base::FeatureList::IsEnabled) checks that there is only one memory address
  // per feature.
  const base::Feature* const apc_features[] = {
    &password_manager::features::kPasswordChange,
    &password_manager::features::kPasswordChangeInSettings,
    &password_manager::features::kPasswordScriptsFetching,
    &password_manager::features::kPasswordDomainCapabilitiesFetching,
    &password_manager::features::kForceEnablePasswordDomainCapabilities,
#if !BUILDFLAG(IS_ANDROID)
    &features::kUnifiedSidePanel,
#endif
  };

  base::Value::List relevant_features;
  for (const base::Feature* const feature : apc_features) {
    base::Value::Dict feature_entry;
    feature_entry.Set("name", feature->name);
    bool is_enabled = base::FeatureList::IsEnabled(*feature);
    feature_entry.Set("enabled", is_enabled);
    if (is_enabled) {
      // Obtain feature parameters
      base::FieldTrialParams params;
      if (base::GetFieldTrialParamsByFeature(*feature, &params)) {
        // Convert to dictionary.
        base::Value::Dict feature_params;
        for (const auto& [param_name, param_state] : params)
          feature_params.Set(param_name, param_state);

        feature_entry.Set("parameters", std::move(feature_params));
      }
    }
    relevant_features.Append(std::move(feature_entry));
  }
  return relevant_features;
}

base::Value::List APCInternalsHandler::GetAPCRelatedPrefs() {
  PrefService* pref_service =
      Profile::FromBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext())
          ->GetPrefs();
  if (!pref_service)
    return base::Value::List();

  // A helper function to output information about which store a pref setting
  // comes from.
  auto GetControlLevel =
      [](const PrefService::Preference* preference) -> base::StringPiece {
    if (!preference)
      return "";
    if (preference->IsDefaultValue()) {
      return "Default";
    } else if (preference->IsUserControlled()) {
      return "User";
    } else if (preference->IsManaged()) {
      return "Policy";
    } else {
      return "Other";
    }
  };

  base::Value::List result;
  for (base::StringPiece pref : GetAssistantPrefs()) {
    base::Value::Dict pref_info;
    pref_info.Set("name", pref);
    pref_info.Set("value", pref_service->GetBoolean(pref));
    // `FindPreference` does not yet support base::StringPiece`.
    pref_info.Set(
        "control_level",
        GetControlLevel(pref_service->FindPreference(std::string(pref))));
    result.Append(std::move(pref_info));
  }

  return result;
}

base::Value::Dict APCInternalsHandler::GetPasswordScriptFetcherInformation() {
  if (PasswordScriptsFetcher* scripts_fetcher = GetPasswordScriptsFetcher();
      scripts_fetcher) {
    return scripts_fetcher->GetDebugInformationForInternals();
  }
  return base::Value::Dict();
}

base::Value::List APCInternalsHandler::GetPasswordScriptFetcherCache() {
  if (PasswordScriptsFetcher* scripts_fetcher = GetPasswordScriptsFetcher();
      scripts_fetcher) {
    return scripts_fetcher->GetCacheEntries();
  }
  return base::Value::List();
}

base::Value::Dict APCInternalsHandler::GetAutofillAssistantInformation() const {
  base::Value::Dict result;
  result.Set("Country code", GetCountryCode());

  // TODO(crbug.com/1314010): Add default values once global instance of
  // AutofillAssistant exists and exposes more methods.
  static base::StringPiece kAutofillAssistantSwitches[] = {
      autofill_assistant::switches::kAutofillAssistantAnnotateDom,
      autofill_assistant::switches::kAutofillAssistantAuth,
      autofill_assistant::switches::kAutofillAssistantCupPublicKeyBase64,
      autofill_assistant::switches::kAutofillAssistantCupKeyVersion,
      autofill_assistant::switches::kAutofillAssistantForceFirstTimeUser,
      autofill_assistant::switches::kAutofillAssistantForceOnboarding,
      autofill_assistant::switches::
          kAutofillAssistantImplicitTriggeringDebugParameters,
      autofill_assistant::switches::kAutofillAssistantServerKey,
      autofill_assistant::switches::kAutofillAssistantUrl,
  };

  const auto* command_line = base::CommandLine::ForCurrentProcess();
  for (base::StringPiece switch_name : kAutofillAssistantSwitches) {
    if (command_line->HasSwitch(switch_name)) {
      result.Set(switch_name, command_line->GetSwitchValueASCII(switch_name));
    }
  }
  return result;
}

void APCInternalsHandler::GetLoginsAndTryLaunchScript(
    const base::Value::List& args) {
  if (!profile_password_store_)
    return;

  if (args.size() == 3 && base::ranges::all_of(args.cbegin(), args.cend(),
                                               &base::Value::is_string)) {
    GURL url = GURL(args.front().GetString());
    url::Origin origin = url::Origin::Create(url);
    password_manager::PasswordFormDigest digest(
        password_manager::PasswordForm::Scheme::kHtml, origin.GetURL().spec(),
        GURL());

    // Check whether to pass debug parameters.
    const std::string& bundle_id = args[1].GetString();
    const std::string& ldap = args[2].GetString();
    if (!bundle_id.empty() && !ldap.empty()) {
      debug_run_information_ = ApcClient::DebugRunInformation{
          .bundle_id = bundle_id, .socket_id = ldap};
    } else {
      debug_run_information_.reset();
    }

    pending_logins_requests_.emplace_back(
        std::make_unique<APCInternalsLoginsRequest>(
            base::BindOnce(&APCInternalsHandler::LaunchScript,
                           base::Unretained(this)),
            base::BindOnce(&APCInternalsHandler::OnLoginsRequestFinished,
                           base::Unretained(this))));

    pending_logins_requests_.back()->IncreaseWaitCounter();
    if (account_password_store_)
      pending_logins_requests_.back()->IncreaseWaitCounter();

    profile_password_store_->GetLogins(
        digest, pending_logins_requests_.back()->GetWeakPtr());

    if (account_password_store_)
      account_password_store_->GetLogins(
          digest, pending_logins_requests_.back()->GetWeakPtr());
  }
}

void APCInternalsHandler::OnLoginsRequestFinished(
    APCInternalsLoginsRequest* finished_request) {
  base::EraseIf(pending_logins_requests_,
                base::MatchesUniquePtr(finished_request));
}

void APCInternalsHandler::LaunchScript(const GURL& url,
                                       const std::string& username) {
#if !BUILDFLAG(IS_ANDROID)
  NavigateParams params(Profile::FromBrowserContext(
                            web_ui()->GetWebContents()->GetBrowserContext()),
                        url, ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      Navigate(&params);

  if (navigation_handle) {
    ApcClient* apc_client = ApcClient::GetOrCreateForWebContents(
        navigation_handle.get()->GetWebContents());
    apc_client->Start(url, username,
                      /*skip_login=*/false,
                      /*callback=*/base::DoNothing(), debug_run_information_);
  }
#endif  // !IS_ANDROID
}
