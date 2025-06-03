// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_utils.h"

#include "base/feature_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/search/ntp_features.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace customize_chrome {

bool IsWallpaperSearchEnabledForProfile(Profile* profile) {
  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return IsEnUSLocaleOnlyFeatureEnabled(
             ntp_features::kCustomizeChromeWallpaperSearch) &&
         base::FeatureList::IsEnabled(
             optimization_guide::features::kOptimizationGuideModelExecution) &&
         (optimization_guide_keyed_service &&
          optimization_guide_keyed_service
              ->ShouldFeatureBeCurrentlyEnabledForUser(
                  optimization_guide::UserVisibleFeatureKey::kWallpaperSearch));
}

// TODO(crbug.com/415116961): Add unit tests for this function.
void MaybeDisableExtensionOverridingNtp(
    content::BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    return;
  }

  // Ensure that the extension service has not already shut down.
  if (!extensions::ExtensionSystem::Get(browser_context)->extension_service()) {
    return;
  }

  extensions::ExtensionId extension_id;
  while (const extensions::Extension* current_extension =
             extensions::GetExtensionOverridingNewTabPage(browser_context)) {
    if (!current_extension || extension_id == current_extension->id()) {
      return;
    }

    extension_id = current_extension->id();
    extensions::ExtensionRegistrar::Get(browser_context)
        ->DisableExtension(extension_id,
                           {extensions::disable_reason::DISABLE_USER_ACTION});
  }
}

}  // namespace customize_chrome
