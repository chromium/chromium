// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/apc_internals/apc_internals_handler.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/password_scripts_fetcher_factory.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"

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

}  // namespace

APCInternalsHandler::~APCInternalsHandler() = default;

void APCInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "loaded", base::BindRepeating(&APCInternalsHandler::OnLoaded,
                                    base::Unretained(this)));
}

void APCInternalsHandler::OnLoaded(const base::Value::List& args) {
  AllowJavascript();

  // Provide information for initial page creation.
  FireWebUIListener("on-flags-information-received",
                    base::Value(GetAPCRelatedFlags()));
  FireWebUIListener("on-script-fetching-information-received",
                    base::Value(GetPasswordScriptFetcherInformation()));
  FireWebUIListener("on-autofill-assistant-information-received",
                    base::Value(GetAutofillAssistantInformation()));
}

// Returns a list of dictionaries that contain the name and the state of
// each APC-related feature.
base::Value::List APCInternalsHandler::GetAPCRelatedFlags() const {
  // We must use pointers to the features instead of copying the features,
  // because base::FeatureList::CheckFeatureIdentity (asserted, e.g., in
  // base::FeatureList::IsEnabled) checks that there is only one memory address
  // per feature.
  const base::Feature* const apc_features[] = {
#if BUILDFLAG(IS_ANDROID)
    &password_manager::features::kPasswordChangeInSettings,
    &password_manager::features::kPasswordDomainCapabilitiesFetching,
    &password_manager::features::kPasswordScriptsFetching,
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

        feature_entry.Set("parameters", base::Value(std::move(feature_params)));
      }
    }
    relevant_features.Append(base::Value(std::move(feature_entry)));
  }
  return relevant_features;
}

base::Value::Dict APCInternalsHandler::GetPasswordScriptFetcherInformation() {
#if BUILDFLAG(IS_ANDROID)
  content::BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  password_manager::PasswordScriptsFetcher* scripts_fetcher =
      PasswordScriptsFetcherFactory::GetForBrowserContext(browser_context);
  if (scripts_fetcher)
    return scripts_fetcher->GetDebugInformationForInternals();
#endif
  return base::Value::Dict();
}

base::Value::Dict APCInternalsHandler::GetAutofillAssistantInformation() const {
  base::Value::Dict result;
  result.Set("Country code", GetCountryCode());

  // TODO(crbug.com/1314010): Add default values once global instance of
  // AutofillAssistant exists and exposes more methods.
  static const char* const kAutofillAssistantSwitches[] = {
#if BUILDFLAG(IS_ANDROID)
    autofill_assistant::switches::kAutofillAssistantAnnotateDom,
    autofill_assistant::switches::kAutofillAssistantAuth,
    autofill_assistant::switches::kAutofillAssistantCupPublicKeyBase64,
    autofill_assistant::switches::kAutofillAssistantCupKeyVersion,
    autofill_assistant::switches::kAutofillAssistantForceFirstTimeUser,
    autofill_assistant::switches::kAutofillAssistantForceOnboarding,
    autofill_assistant::switches::
        kAutofillAssistantImplicitTriggeringDebugParameters,
    autofill_assistant::switches::kAutofillAssistantServerKey,
    autofill_assistant::switches::kAutofillAssistantUrl
#endif
  };

  const auto* command_line = base::CommandLine::ForCurrentProcess();
  for (const char* switch_name : kAutofillAssistantSwitches) {
    if (command_line->HasSwitch(switch_name)) {
      result.Set(switch_name, command_line->GetSwitchValueASCII(switch_name));
    }
  }
  return result;
}
