// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_navigation_handle_user_data.h"

#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/web_applications/navigation_capturing_process.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/navigation_capturing_metrics.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/launch_queue/launch_queue.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"

namespace web_app {

WebAppLaunchNavigationHandleUserData::~WebAppLaunchNavigationHandleUserData() =
    default;

// static
void WebAppLaunchNavigationHandleUserData::DispatchLaunchParams(
    content::WebContents* web_contents,
    webapps::LaunchParams launch_params) {
  CHECK(web_contents);
  WebAppTabHelper* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  launch_params.set_started_new_navigation(false);
  tab_helper->EnqueueLaunchParams(std::move(launch_params));
}

WebAppLaunchNavigationHandleUserData::WebAppLaunchNavigationHandleUserData(
    content::NavigationHandle& navigation_handle)
    : navigation_handle_(navigation_handle),
      web_contents_(navigation_handle.GetWebContents()),
      force_iph_off_(false) {
  CHECK(navigation_handle.IsInPrimaryMainFrame());
}

const webapps::AppId& WebAppLaunchNavigationHandleUserData::app_id() const {
  return app_id_;
}

const webapps::LaunchParams&
WebAppLaunchNavigationHandleUserData::GetLaunchParams() const {
  CHECK(launch_params_.has_value())
      << "Attempted to access launch params after they were consumed/moved.";
  return *launch_params_;
}

void WebAppLaunchNavigationHandleUserData::SetLaunchParams(
    webapps::LaunchParams launch_params) {
  app_id_ = launch_params.app_id();
  launch_params_ = std::move(launch_params);
  if (web_contents_) {
    WebAppTabHelper* tab_helper =
        WebAppTabHelper::FromWebContents(web_contents_);
    if (tab_helper) {
      tab_helper->EnsureLaunchQueue();
      tab_helper->SetPendingLaunchParamsHolder(GetWeakPtr());
    }
  }
}

base::WeakPtr<WebAppLaunchParamsHolder>
WebAppLaunchNavigationHandleUserData::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void WebAppLaunchNavigationHandleUserData::SetLaunchParamsMetadata(
    webapps::AppId app_id,
    GURL target_url,
    base::TimeTicks time_navigation_started) {
  if (!launch_params_) {
    launch_params_.emplace();
  }
  app_id_ = app_id;
  launch_params_->set_app_id(std::move(app_id));
  launch_params_->set_target_url(std::move(target_url));
  if (launch_params_->time_navigation_started_for_enqueue().is_null()) {
    launch_params_->set_time_navigation_started_for_enqueue(
        time_navigation_started);
  }

  if (web_contents_) {
    WebAppTabHelper* tab_helper =
        WebAppTabHelper::FromWebContents(web_contents_);
    if (tab_helper) {
      tab_helper->EnsureLaunchQueue();
      tab_helper->SetPendingLaunchParamsHolder(GetWeakPtr());
    }
  }
}

void WebAppLaunchNavigationHandleUserData::
    MaybePerformAppHandlingTasksInWebContents() {
  if (!launch_params_) {
    return;
  }
  WebAppTabHelper* tab_helper = WebAppTabHelper::FromWebContents(web_contents_);
  CHECK(tab_helper);

  // Extract app_id and target_url before moving launch_params_.
  const webapps::AppId app_id = launch_params_->app_id();
  const GURL target_url = launch_params_->target_url();

  // Keep started_new_navigation = true so Blink records correct metrics.
  launch_params_->set_started_new_navigation(true);
  if (tab_helper->EnsureLaunchQueue().IsInScope(*launch_params_,
                                                navigation_handle_->GetURL())) {
    tab_helper->EnqueueLaunchParams(std::move(*launch_params_));
  }

  launch_params_.reset();

  if (!is_navigation_capturing_) {
    return;
  }

  // Perform navigation capturing specific tasks below.
  apps::LaunchContainer container =
      tab_helper->is_in_app_window()
          ? apps::LaunchContainer::kLaunchContainerWindow
          : apps::LaunchContainer::kLaunchContainerTab;
  RecordLaunchMetrics(app_id, container,
                      apps::LaunchSource::kFromNavigationCapturing, target_url,
                      web_contents_);

  RecordNavigationCapturingDisplayModeMetrics(app_id, web_contents_,
                                              !tab_helper->is_in_app_window());

  if (!force_iph_off_) {
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_contents_);
    BrowserWindowInterface* browser =
        tab ? tab->GetBrowserWindowInterface() : nullptr;
    if (browser) {
      MaybeShowNavigationCaptureIph(app_id, browser->GetProfile(),
                                    browser->GetBrowserForMigrationOnly());
    }
  }
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(WebAppLaunchNavigationHandleUserData);

}  // namespace web_app
