// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/services/hats/os_settings_hats_manager.h"
#include "chrome/browser/ui/webui/ash/settings/services/hats/os_settings_hats_manager_factory.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace ash::settings {
OsSettingsHatsManager::OsSettingsHatsManager(content::BrowserContext* context)
    : context_(context) {}

OsSettingsHatsManager::~OsSettingsHatsManager() = default;

void OsSettingsHatsManager::SetSettingsUsedSearch(bool set_search) {
  has_user_used_search = set_search;
}

void OsSettingsHatsManager::MaybeSendSettingsHats() {
  // Do not run more than one HATS survey.
  if (hats_notification_controller_) {
    return;
  }

  const HatsConfig& config = kHatsOsSettingsSearchSurvey;

  if (::ash::HatsNotificationController::ShouldShowSurveyToProfile(
          Profile::FromBrowserContext(context_), config)) {
    Profile* profile = Profile::FromBrowserContext(context_);
    base::flat_map<std::string, std::string> product_specific_data;

    if (has_user_used_search) {
      product_specific_data.insert_or_assign("has_user_used_os_settings_search",
                                             "true");
    }

    hats_notification_controller_ =
        base::MakeRefCounted<ash::HatsNotificationController>(
            profile, config, product_specific_data);
  }
}

}  // namespace ash::settings
