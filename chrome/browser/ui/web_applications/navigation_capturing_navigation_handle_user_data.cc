// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/navigation_capturing_navigation_handle_user_data.h"

#include "base/strings/to_string.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/navigation_capturing_log.h"
#include "chrome/browser/web_applications/web_app_launch_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "ui/base/window_open_disposition.h"

namespace web_app {

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::Disabled() {
  return NavigationCapturingRedirectionInfo(
      /*source_browser_app_id=*/std::nullopt,
      /* source_tab_app_id= */ std::nullopt,
      NavigationHandlingInitialResult::kNotHandledByNavigationHandling,
      /*first_navigation_app_id=*/std::nullopt, WindowOpenDisposition::UNKNOWN);
}

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::AuxiliaryContext(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const std::optional<webapps::AppId>& source_tab_app_id,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id, source_tab_app_id,
      NavigationHandlingInitialResult::kAuxContext,
      /*first_navigation_app_id=*/std::nullopt, disposition);
}

// Created for user-modified or capturable navigations that don't have an
// initial controlling app of the first url.
// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::NoInitialActionRedirectionHandlingEligible(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const std::optional<webapps::AppId>& source_tab_app_id,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id, source_tab_app_id,
      NavigationHandlingInitialResult::kBrowserTab,
      /*first_navigation_app_id=*/std::nullopt, disposition);
}

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::ForcedNewContext(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const std::optional<webapps::AppId>& source_tab_app_id,
    const webapps::AppId& capturing_app_id,
    blink::mojom::DisplayMode capturing_display_mode,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id, source_tab_app_id,
      capturing_display_mode == blink::mojom::DisplayMode::kBrowser
          ? NavigationHandlingInitialResult::kForcedNewAppContextBrowserTab
          : NavigationHandlingInitialResult::kForcedNewAppContextAppWindow,
      capturing_app_id, disposition);
}

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::CapturedNewContext(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const std::optional<webapps::AppId>& source_tab_app_id,
    const webapps::AppId& capturing_app_id,
    blink::mojom::DisplayMode capturing_display_mode,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id, source_tab_app_id,
      capturing_display_mode == blink::mojom::DisplayMode::kBrowser
          ? NavigationHandlingInitialResult::kNavigateCapturedNewBrowserTab
          : NavigationHandlingInitialResult::kNavigateCapturedNewAppWindow,
      capturing_app_id, disposition);
}

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::CapturedNavigateExisting(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const std::optional<webapps::AppId>& source_tab_app_id,
    const webapps::AppId& capturing_app_id,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id, source_tab_app_id,
      NavigationHandlingInitialResult::kNavigateCapturingNavigateExisting,
      capturing_app_id, disposition);
}

NavigationCapturingRedirectionInfo::~NavigationCapturingRedirectionInfo() =
    default;
NavigationCapturingRedirectionInfo::NavigationCapturingRedirectionInfo(
    const NavigationCapturingRedirectionInfo& navigation_info) = default;
NavigationCapturingRedirectionInfo&
NavigationCapturingRedirectionInfo::operator=(
    const NavigationCapturingRedirectionInfo&) = default;

base::Value NavigationCapturingRedirectionInfo::ToDebugData() const {
  return base::Value(
      base::Value::Dict()
          .Set("initial_nav_handling_result",
               base::ToString(initial_nav_handling_result()))
          .Set("source_browser_app_id",
               source_browser_app_id().value_or("<none>"))
          .Set("source_tab_app_id", source_tab_app_id().value_or("<none>"))
          .Set("first_navigation_app_id",
               first_navigation_app_id().value_or("<none>"))
          .Set("disposition", base::ToString(disposition())));
}

NavigationCapturingRedirectionInfo::NavigationCapturingRedirectionInfo(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const std::optional<webapps::AppId>& source_tab_app_id,
    NavigationHandlingInitialResult initial_nav_handling_result,
    const std::optional<webapps::AppId>& first_navigation_app_id,
    WindowOpenDisposition disposition)
    : source_browser_app_id_(source_browser_app_id),
      source_tab_app_id_(source_tab_app_id),
      initial_nav_handling_result_(initial_nav_handling_result),
      first_navigation_app_id_(first_navigation_app_id),
      disposition_(disposition) {}

NavigationCapturingNavigationHandleUserData::
    ~NavigationCapturingNavigationHandleUserData() = default;

NavigationCapturingNavigationHandleUserData::
    NavigationCapturingNavigationHandleUserData(
        content::NavigationHandle& navigation_handle,
        std::optional<NavigationCapturingRedirectionInfo> redirection_info,
        std::optional<webapps::AppId> launched_app,
        bool force_iph_off)
    : navigation_handle_(navigation_handle),
      redirection_info_(std::move(redirection_info)),
      launched_app_(std::move(launched_app)),
      force_iph_off_(force_iph_off) {}

void NavigationCapturingNavigationHandleUserData::SetLaunchedAppState(
    std::optional<webapps::AppId> launched_app,
    bool force_iph_off) {
  launched_app_ = launched_app;
  force_iph_off_ = force_iph_off;
}

void NavigationCapturingNavigationHandleUserData::
    MaybePerformAppHandlingTasksInWebContents() {
  if (!launched_app_.has_value()) {
    return;
  }

  const webapps::AppId& app_id = *launched_app_;
  content::WebContents* web_contents = navigation_handle_->GetWebContents();

  EnqueueLaunchParams(
      web_contents, app_id, navigation_handle_->GetURL(),
      /*wait_for_navigation_to_complete=*/!navigation_handle_->HasCommitted());

  WebAppTabHelper* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);

  apps::LaunchContainer container =
      tab_helper->is_in_app_window()
          ? apps::LaunchContainer::kLaunchContainerWindow
          : apps::LaunchContainer::kLaunchContainerTab;
  RecordLaunchMetrics(app_id, container,
                      apps::LaunchSource::kFromNavigationCapturing,
                      navigation_handle_->GetURL(), web_contents);

  WebAppProvider::GetForWebContents(web_contents)
      ->navigation_capturing_log()
      .StoreNavigationCapturedDebugData(ToDebugData());

  if (!force_iph_off_) {
    // TODO(crbug.com/371237535): Avoid reliance on FindBrowserWithTab and
    // instead pass in the Browser instance earlier.
    Browser* browser = chrome::FindBrowserWithTab(web_contents);
    MaybeShowNavigationCaptureIph(app_id, browser->profile(), browser);
  }
}

base::Value NavigationCapturingNavigationHandleUserData::ToDebugData() const {
  return base::Value(
      base::Value::Dict()
          .Set("launched_app", launched_app_.value_or("<none>"))
          .Set("force_iph_off", force_iph_off_)
          .Set("handle",
               base::Value::Dict().Set(
                   "url",
                   navigation_handle_->GetURL().possibly_invalid_spec())));
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(
    NavigationCapturingNavigationHandleUserData);

}  // namespace web_app
