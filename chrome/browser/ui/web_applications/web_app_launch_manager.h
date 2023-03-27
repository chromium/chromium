// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_

#include "base/functional/callback_forward.h"

namespace apps {
struct AppLaunchParams;
}  // namespace apps

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// TODO(https://crbug.com/1427764): Delete this & move the static testing method
// to the WebAppLaunchProcess.
class WebAppLaunchManager {
 public:
  using OpenApplicationCallback = base::RepeatingCallback<content::WebContents*(
      apps::AppLaunchParams&& params)>;

  static void SetOpenApplicationCallbackForTesting(
      OpenApplicationCallback callback);

  // Created temporarily while this class is migrated to the command system.
  static OpenApplicationCallback& GetOpenApplicationCallbackForTesting();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_
