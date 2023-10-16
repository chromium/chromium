// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/personalization/personalization_hub_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/personalization_entry_point.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "content/public/browser/web_ui.h"

namespace ash::settings {

PersonalizationHubHandler::PersonalizationHubHandler() = default;

PersonalizationHubHandler::~PersonalizationHubHandler() = default;

void PersonalizationHubHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "openPersonalizationHub",
      base::BindRepeating(
          &PersonalizationHubHandler::HandleOpenPersonalizationHub,
          base::Unretained(this)));
}

void PersonalizationHubHandler::HandleOpenPersonalizationHub(
    const base::Value::List& args) {
  CHECK_EQ(0U, args.size());
  // Record entry point metric to Personalization Hub through Settings.
  personalization_app::LogPersonalizationEntryPoint(
      PersonalizationEntryPoint::kSettings);
  LaunchSystemWebAppAsync(Profile::FromWebUI(web_ui()),
                          SystemWebAppType::PERSONALIZATION);
}

}  // namespace ash::settings
