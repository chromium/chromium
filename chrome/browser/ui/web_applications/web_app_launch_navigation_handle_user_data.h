// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_NAVIGATION_HANDLE_USER_DATA_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_NAVIGATION_HANDLE_USER_DATA_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace web_app {

// Data that is attached to a NavigationHandle which describes what app (if any)
// was launched as part of the navigation. When the navigation completes this is
// used to queue launch params, record launch and navigation timing metrics, and
// maybe show navigation capturing IPH.
class WebAppLaunchNavigationHandleUserData
    : public content::NavigationHandleUserData<
          WebAppLaunchNavigationHandleUserData> {
 public:
  ~WebAppLaunchNavigationHandleUserData() override;

  const webapps::AppId& launched_app() const { return launched_app_; }

  // This method will queue launch params, record launch and navigation timing
  // metrics and maybe show a navigation capturing IPH. Should be called when it
  // is known that this navigation will commit.
  void MaybePerformAppHandlingTasksInWebContents();

 private:
  WebAppLaunchNavigationHandleUserData(
      content::NavigationHandle& navigation_handle,
      webapps::AppId launched_app,
      bool force_iph_off,
      base::TimeTicks time_navigation_started);

  friend NavigationHandleUserData;

  raw_ref<content::NavigationHandle> navigation_handle_;
  webapps::AppId launched_app_;
  bool force_iph_off_;
  base::TimeTicks time_navigation_started_;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_NAVIGATION_HANDLE_USER_DATA_H_
