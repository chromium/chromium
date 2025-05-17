// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_THROTTLE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

class Profile;

namespace web_app {

// Throttle that is used to wait with IWA navigation until the required modules
// (e.g. web app provider, ...) are available / initialized.
class IsolatedWebAppThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);
  explicit IsolatedWebAppThrottle(
      content::NavigationThrottleRegistry& registry);
  ~IsolatedWebAppThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

 private:
  Profile* profile();
  bool is_isolated_web_app_navigation();

  base::WeakPtrFactory<IsolatedWebAppThrottle> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_THROTTLE_H_
