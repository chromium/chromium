// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/controlled_frame/controlled_frame.h"

#include <string>

#include "base/containers/contains.h"
#include "chrome/common/initialize_extensions_client.h"
#include "content/public/common/content_features.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/command_line.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "url/url_constants.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
namespace {
bool IsRunningInKioskMode() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceAppMode);
}
}  // namespace
#endif

namespace controlled_frame {

bool AvailabilityCheck(const std::string& api_full_name,
                       const extensions::Extension* extension,
                       extensions::Feature::Context context,
                       const GURL& url,
                       extensions::Feature::Platform platform,
                       int context_id,
                       bool check_developer_mode,
                       const extensions::ContextData& context_data) {
  bool is_allowed_for_scheme = url.SchemeIs("isolated-app");

#if BUILDFLAG(IS_CHROMEOS)
  // Also allow API exposure in ChromeOS Kiosk mode for web apps.
  if (base::FeatureList::IsEnabled(features::kWebKioskEnableIwaApis) &&
      IsRunningInKioskMode() && url.SchemeIs(url::kHttpsScheme)) {
    is_allowed_for_scheme = true;
  }
#endif

  // Verify that the current context is an Isolated Web App and the API name is
  // in our expected list.
  return !extension && is_allowed_for_scheme &&
         context == extensions::Feature::WEB_PAGE_CONTEXT &&
         context_data.IsIsolatedApplication() &&
         base::Contains(GetControlledFrameFeatureList(), api_full_name);
}

extensions::Feature::FeatureDelegatedAvailabilityCheckMap
CreateAvailabilityCheckMap() {
  extensions::Feature::FeatureDelegatedAvailabilityCheckMap map;
  for (const auto* item : GetControlledFrameFeatureList()) {
    map.emplace(item, base::BindRepeating(&AvailabilityCheck));
  }
  return map;
}

}  // namespace controlled_frame
