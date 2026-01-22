// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_filter.h"

#include "base/check_is_test.h"

namespace web_app {

WebAppFilter::WebAppFilter() = default;
WebAppFilter::WebAppFilter(const WebAppFilter&) = default;

// static
WebAppFilter WebAppFilter::OpensInBrowserTab() {
  WebAppFilter filter;
  filter.opens_in_browser_tab_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::OpensInDedicatedWindow() {
  WebAppFilter filter;
  filter.opens_in_dedicated_window_ = true;
  return filter;
}

// static
#if !BUILDFLAG(IS_CHROMEOS)
WebAppFilter WebAppFilter::CapturesLinksInScope() {
  WebAppFilter filter;
  filter.captures_links_in_scope_ = true;
  return filter;
}
#endif

// static
WebAppFilter WebAppFilter::IsIsolatedApp() {
  WebAppFilter filter;
  filter.isolated_app_filter_ = {{}};
  return filter;
}

// static
WebAppFilter WebAppFilter::IsDevModeIsolatedApp() {
  WebAppFilter filter;
  filter.isolated_app_filter_ = {{.must_be_in_dev_mode = true}};
  return filter;
}

// static
WebAppFilter WebAppFilter::IsIsolatedSubApp() {
  WebAppFilter filter;
  filter.isolated_app_filter_ = {{.is_sub_app = true}};
  return filter;
}

// static
WebAppFilter WebAppFilter::PolicyInstalledIsolatedWebApp() {
  WebAppFilter filter;
  filter.isolated_app_filter_ = {{.must_be_policy_installed = true}};
  return filter;
}

// static
WebAppFilter WebAppFilter::UserInstalledIsolatedWebApp() {
  WebAppFilter filter;
  filter.isolated_app_filter_ = {{.must_be_user_installed = true}};
  return filter;
}

// static
WebAppFilter WebAppFilter::IsIsolatedWebAppWithOnlyUserManagement() {
  WebAppFilter filter;
  filter.isolated_app_filter_ = {{.must_have_no_external_management = true}};
  return filter;
}

// static
WebAppFilter WebAppFilter::IsCraftedApp() {
  WebAppFilter filter;
  filter.is_crafted_app_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::IsCraftedAppAndOpensInDedicatedWindow() {
  WebAppFilter filter;
  filter.is_crafted_app_and_opens_in_dedicated_window_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::IsSuggestedApp() {
  WebAppFilter filter;
  filter.is_suggested_app_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::DisplaysBadgeOnOs() {
  WebAppFilter filter;
  filter.displays_badge_on_os_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::SupportsOsNotifications() {
  WebAppFilter filter;
  filter.supports_os_notifications_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::InstalledInChrome() {
  WebAppFilter filter;
  filter.installed_in_chrome_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::InstalledInOperatingSystemForTesting() {
  CHECK_IS_TEST();
  WebAppFilter filter;
  filter.installed_in_os_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::IsDiyWithOsShortcut() {
  WebAppFilter filter;
  filter.is_diy_with_os_shortcut_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::LaunchableFromInstallApi() {
  WebAppFilter filter;
  filter.launchable_from_install_api_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::IsTrusted() {
  WebAppFilter filter;
  filter.is_app_trusted_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::IsIsolatedWebAppIncludingUninstalling() {
  WebAppFilter filter;
  filter.is_isolated_apps_including_uninstalling_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::IsAppSuggestedForMigration() {
  WebAppFilter filter;
  filter.is_app_suggested_from_migration_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::IsAppSurfaceableToUser() {
  WebAppFilter filter;
  filter.is_app_surfaceable_to_user_ = true;
  return filter;
}

}  // namespace web_app
