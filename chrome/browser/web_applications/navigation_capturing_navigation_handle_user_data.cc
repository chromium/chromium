// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/navigation_capturing_navigation_handle_user_data.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "ui/base/window_open_disposition.h"

namespace web_app {

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::Disabled() {
  return NavigationCapturingRedirectionInfo(
      /*app_id_source_browser=*/std::nullopt,
      NavigationHandlingInitialResult::kNotHandledByNavigationHandling,
      /*first_navigation_app_id=*/std::nullopt, WindowOpenDisposition::UNKNOWN,
      InitialNavigationCapturedBehavior::kNotHandled);
}

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::AuxiliaryContext(
    const std::optional<webapps::AppId>& source_browser_app_id,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id, NavigationHandlingInitialResult::kAuxContext,
      /*first_navigation_app_id=*/std::nullopt, disposition,
      InitialNavigationCapturedBehavior::kNotHandled);
}

// Created for user-modified or capturable navigations that don't have an
// initial controlling app of the first url.
// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::NoInitialActionRedirectionHandlingEligible(
    const std::optional<webapps::AppId>& source_browser_app_id,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id, NavigationHandlingInitialResult::kBrowserTab,
      /*first_navigation_app_id=*/std::nullopt, disposition,
      InitialNavigationCapturedBehavior::kNotHandled);
}

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::ForcedNewContext(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const webapps::AppId controlling_app_id,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id,
      source_browser_app_id
          ? NavigationHandlingInitialResult::kForcedNewAppContext
          : NavigationHandlingInitialResult::kBrowserTab,
      controlling_app_id, disposition,
      InitialNavigationCapturedBehavior::kNotHandled);
}

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::CapturedNewContext(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const webapps::AppId controlling_app_id,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id, NavigationHandlingInitialResult::kNavigateCaptured,
      controlling_app_id, disposition,
      InitialNavigationCapturedBehavior::kNavigatedNew);
}

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::CapturedNavigateExisting(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const webapps::AppId controlling_app_id,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id, NavigationHandlingInitialResult::kNavigateCaptured,
      controlling_app_id, disposition,
      InitialNavigationCapturedBehavior::kNavigatedExisting);
}

NavigationCapturingRedirectionInfo::~NavigationCapturingRedirectionInfo() =
    default;
NavigationCapturingRedirectionInfo::NavigationCapturingRedirectionInfo(
    const NavigationCapturingRedirectionInfo& navigation_info) = default;
NavigationCapturingRedirectionInfo&
NavigationCapturingRedirectionInfo::operator=(
    const NavigationCapturingRedirectionInfo&) = default;

NavigationCapturingRedirectionInfo::NavigationCapturingRedirectionInfo(
    const std::optional<webapps::AppId>& source_browser_app_id,
    NavigationHandlingInitialResult initial_nav_handling_result,
    const std::optional<webapps::AppId>& first_navigation_app_id,
    WindowOpenDisposition disposition,
    InitialNavigationCapturedBehavior effective_launch_handling_mode)
    : app_id_source_browser_(source_browser_app_id),
      initial_nav_handling_result_(initial_nav_handling_result),
      first_navigation_app_id_(first_navigation_app_id),
      disposition_(disposition),
      effective_launch_handling_mode_(effective_launch_handling_mode) {}

NavigationCapturingNavigationHandleUserData::
    ~NavigationCapturingNavigationHandleUserData() = default;

NavigationCapturingNavigationHandleUserData::
    NavigationCapturingNavigationHandleUserData(
        content::NavigationHandle& navigation_handle,
        NavigationCapturingRedirectionInfo redirection_info)
    : redirection_info_(std::move(redirection_info)) {}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(
    NavigationCapturingNavigationHandleUserData);

}  // namespace web_app
