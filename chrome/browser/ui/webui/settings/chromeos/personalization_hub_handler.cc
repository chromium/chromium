// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/personalization_hub_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/personalization_entry_point.h"
#include "base/bind.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {
namespace settings {

PersonalizationHubHandler::PersonalizationHubHandler() = default;

PersonalizationHubHandler::~PersonalizationHubHandler() = default;

void PersonalizationHubHandler::RegisterMessages() {
  DCHECK(ash::features::IsPersonalizationHubEnabled());
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
  ash::personalization_app::LogPersonalizationEntryPoint(
      ash::PersonalizationEntryPoint::kSettings);
  web_app::LaunchSystemWebAppAsync(Profile::FromWebUI(web_ui()),
                                   ash::SystemWebAppType::PERSONALIZATION);
}

}  // namespace settings
}  // namespace chromeos
