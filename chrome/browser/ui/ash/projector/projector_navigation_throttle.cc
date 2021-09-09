// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_navigation_throttle.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chromeos/components/projector_app/projector_app_constants.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace ash {

// static
std::unique_ptr<ProjectorNavigationThrottle>
ProjectorNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  if (!features::IsProjectorEnabled())
    return nullptr;

  if (!navigation_handle->IsInMainFrame())
    return nullptr;

  // Can't use std::make_unique because the constructor is private.
  return base::WrapUnique(new ProjectorNavigationThrottle(navigation_handle));
}

ProjectorNavigationThrottle::~ProjectorNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
ProjectorNavigationThrottle::WillStartRequest() {
  GURL url = navigation_handle()->GetURL();
  if (url.GetOrigin() ==
      GURL(chromeos::kChromeUIUntrustedProjectorPwaUrl).GetOrigin()) {
    // |url| should look like https://projector.apps.chrome/xyz.
    // TODO(b/195975836): Update this comment after URL format finalized.
    GURL override_url = ConstructOverrideUrl(url);
    // TODO(b/195975836): Add metrics to track the number of launches this way.
    LaunchProjectorApp(override_url);
    return NavigationThrottle::CANCEL_AND_IGNORE;
  }
  return NavigationThrottle::PROCEED;
}

const char* ProjectorNavigationThrottle::GetNameForLogging() {
  return "ProjectorNavigationThrottle";
}

ProjectorNavigationThrottle::ProjectorNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle),
      profile_(Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())) {}

GURL ProjectorNavigationThrottle::ConstructOverrideUrl(const GURL& url) {
  std::string override_url = chromeos::kChromeUITrustedProjectorAppUrl;
  // TODO(b/195975836): Validate `url`.
  if (url.path().length() > 1)
    override_url += url.path().substr(1);
  GURL result(override_url);
  DCHECK(result.is_valid());
  return result;
}

void ProjectorNavigationThrottle::LaunchProjectorApp(const GURL& override_url) {
  web_app::SystemAppLaunchParams launch_params;
  launch_params.url = override_url;

  // TODO(b/195975836): Migrate out of Ash once launching SWAs also works with
  // Lacros.
  web_app::LaunchSystemWebAppAsync(profile_, web_app::SystemAppType::PROJECTOR,
                                   launch_params);
}

}  // namespace ash
