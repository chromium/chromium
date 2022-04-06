// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/apc_internals/apc_internals_handler.h"

#include <string>

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/values.h"
#include "chrome/browser/password_manager/password_scripts_fetcher_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/web_contents.h"

APCInternalsHandler::~APCInternalsHandler() = default;

void APCInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "loaded", base::BindRepeating(&APCInternalsHandler::OnLoaded,
                                    base::Unretained(this)));
}

void APCInternalsHandler::OnLoaded(const base::Value::List& args) {
  AllowJavascript();

  // Provide information for initial page creation.
  FireWebUIListener("notify-about-flags", base::Value(GetAPCRelatedFlags()));
  FireWebUIListener("notify-about-script-fetching",
                    base::Value(GetPasswordScriptFetcherInformation()));
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
