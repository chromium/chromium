// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_navigation_handle_user_data.h"

#include "base/time/time.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/navigation_capturing_metrics.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"

namespace web_app {

WebAppLaunchNavigationHandleUserData::~WebAppLaunchNavigationHandleUserData() =
    default;

WebAppLaunchNavigationHandleUserData::WebAppLaunchNavigationHandleUserData(
    content::NavigationHandle& navigation_handle,
    webapps::AppId launched_app,
    bool force_iph_off,
    base::TimeTicks time_navigation_started)
    : navigation_handle_(navigation_handle),
      launched_app_(std::move(launched_app)),
      force_iph_off_(force_iph_off),
      time_navigation_started_(time_navigation_started) {}

void WebAppLaunchNavigationHandleUserData::
    MaybePerformAppHandlingTasksInWebContents() {
  const webapps::AppId& app_id = launched_app_;
  content::WebContents* web_contents = navigation_handle_->GetWebContents();

  EnqueueLaunchParams(
      web_contents, app_id, navigation_handle_->GetURL(),
      /*wait_for_navigation_to_complete=*/!navigation_handle_->HasCommitted(),
      time_navigation_started_);

  WebAppTabHelper* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);

  apps::LaunchContainer container =
      tab_helper->is_in_app_window()
          ? apps::LaunchContainer::kLaunchContainerWindow
          : apps::LaunchContainer::kLaunchContainerTab;
  RecordLaunchMetrics(app_id, container,
                      apps::LaunchSource::kFromNavigationCapturing,
                      navigation_handle_->GetURL(), web_contents);

  RecordNavigationCapturingDisplayModeMetrics(app_id, web_contents,
                                              !tab_helper->is_in_app_window());

  if (!force_iph_off_) {
    // TODO(crbug.com/371237535): Avoid reliance on FindBrowserWithTab and
    // instead pass in the Browser instance earlier.
    Browser* browser = chrome::FindBrowserWithTab(web_contents);
    MaybeShowNavigationCaptureIph(app_id, browser->profile(), browser);
  }
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(WebAppLaunchNavigationHandleUserData);

}  // namespace web_app
