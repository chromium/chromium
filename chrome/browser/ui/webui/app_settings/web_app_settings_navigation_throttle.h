// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_SETTINGS_WEB_APP_SETTINGS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_WEBUI_APP_SETTINGS_WEB_APP_SETTINGS_NAVIGATION_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

// A NavigationThrottle that blocks request when navigating to
// chrome://app-settings/<app-id> page with an invalid app-id.
class WebAppSettingsNavigationThrottle : public content::NavigationThrottle {
 public:
  // Returns a NavigationThrottle when:
  // - we are navigating to the new tab page, and
  // - the main frame is pointed at the new tab URL.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  static void DisableForTesting();

  explicit WebAppSettingsNavigationThrottle(content::NavigationHandle* handle);
  ~WebAppSettingsNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

 private:
  void ContinueCheckForApp(const webapps::AppId& app_id);

  base::WeakPtrFactory<WebAppSettingsNavigationThrottle> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_SETTINGS_WEB_APP_SETTINGS_NAVIGATION_THROTTLE_H_
