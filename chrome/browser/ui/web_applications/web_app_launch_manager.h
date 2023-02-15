// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace apps {
struct AppLaunchParams;
}  // namespace apps

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class WebAppProvider;

// Handles launch requests for Desktop PWAs and bookmark apps.
// Web applications have type AppType::kWeb in the app registry.
class WebAppLaunchManager {
 public:
  using OpenApplicationCallback = base::RepeatingCallback<content::WebContents*(
      apps::AppLaunchParams&& params)>;

  explicit WebAppLaunchManager(Profile* profile);
  WebAppLaunchManager(const WebAppLaunchManager&) = delete;
  WebAppLaunchManager& operator=(const WebAppLaunchManager&) = delete;
  virtual ~WebAppLaunchManager();

  content::WebContents* OpenApplication(apps::AppLaunchParams&& params);

  static void SetOpenApplicationCallbackForTesting(
      OpenApplicationCallback callback);

  // Created temporarily while this class is migrated to the command system.
  static OpenApplicationCallback& GetOpenApplicationCallbackForTesting();

 private:
  const raw_ptr<Profile> profile_;
  const raw_ptr<WebAppProvider> provider_;

  base::WeakPtrFactory<WebAppLaunchManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_
