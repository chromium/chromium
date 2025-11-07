// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILTER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILTER_H_

namespace web_app {

// Describe capabilities that web apps can have. Used to query for apps within
// the WebAppRegistrar that have certain capabilities.
class WebAppFilter {
 public:
  // Only consider web apps whose effective display mode is a browser tab.
  static WebAppFilter OpensInBrowserTab();
  // Only consider web apps whose effective display mode is a dedicated window
  // (essentially any display mode other than a browser tab).
  static WebAppFilter OpensInDedicatedWindow();
  // Only consider web apps that capture links in scope.
  static WebAppFilter CapturesLinksInScope();
  // Only consider isolated web apps, that are not scheduled for uninstallation,
  // like stub ones. To also consider stub apps, use
  // `IsIsolatedWebAppIncludingUninstalling()` instead.
  static WebAppFilter IsIsolatedApp();
  // Only consider force-installed Isolated Web Apps.
  static WebAppFilter PolicyInstalledIsolatedWebApp();
  // Only consider crafted web apps (not DIY apps).
  static WebAppFilter IsCraftedApp();
  // Only consider crafted web apps that are set to open in a dedicated window.
  static WebAppFilter IsCraftedAppAndOpensInDedicatedWindow();
  // Only consider apps that are not installed on this device, but are suggested
  // from other devices.
  static WebAppFilter IsSuggestedApp();
  // Only consider web apps that support app badging via the OS.
  static WebAppFilter DisplaysBadgeOnOs();
  // Only consider web apps that support OS notifications.
  static WebAppFilter SupportsOsNotifications();
  // Only consider web apps that have been installed in Chrome.
  // IMPORTANT: This value can tell you which installed app best fits the scope
  // you supply, but should NOT be used to infer capabilities of said web app!
  // Because a web app can be installed _without_ OS integration, which means
  // that it will lack the ability to open as a Standalone app or send OS
  // notifications, to name a couple of examples. New code should not use this
  // filter, but query for web app capabilities, with the functions above. If
  // there's no match then reach out to see if such a function can be added.
  static WebAppFilter InstalledInChrome();
  // Only consider web apps that have been installed in Chrome and have been
  // integrated into the OS. This function should only be used for testing.
  static WebAppFilter InstalledInOperatingSystemForTesting();

  // Only consider web apps that are DIY apps with OS shortcuts.
  static WebAppFilter IsDiyWithOsShortcut();

  // Only consider web apps that open in a dedicated window (see above), or were
  // installed by the user. Used by the Web Install API.
  static WebAppFilter LaunchableFromInstallApi();

  // Only consider web apps that have been installed in Chrome by trusted
  // sources, like admin or preinstalled apps.
  static WebAppFilter IsTrusted();

  // Consider any isolated web apps, even ones that are stubs and have
  // `is_uninstalling` set to true.
  static WebAppFilter IsIsolatedWebAppIncludingUninstalling();

  WebAppFilter(const WebAppFilter&);
  WebAppFilter& operator=(const WebAppFilter&) = default;
  ~WebAppFilter() = default;

 private:
  friend class WebAppRegistrar;

  WebAppFilter();

  bool opens_in_browser_tab_ = false;
  bool opens_in_dedicated_window_ = false;

// ChromeOS stores the per-app capturing setting in PreferredAppsImpl, not here.
#if !defined(IS_CHROMEOS)
  bool captures_links_in_scope_ = false;
#endif

  bool is_isolated_app_ = false;
  bool is_crafted_app_ = false;
  bool is_suggested_app_ = false;
  bool displays_badge_on_os_ = false;
  bool supports_os_notifications_ = false;
  bool installed_in_chrome_ = false;
  bool installed_in_os_ = false;
  bool is_diy_with_os_shortcut_ = false;
  bool launchable_from_install_api_ = false;
  bool is_policy_installed_iwa = false;
  // Having is_crafted_app_ and opens_in_dedicated_window_ set to true
  // separately would result in matching any app for which either filter is
  // true. So use a separate field for the combination of the two. In the
  // future we might want to have a more generic "and" mechanism for filters.
  bool is_crafted_app_and_opens_in_dedicated_window_ = false;
  bool is_app_trusted_ = false;
  bool is_isolated_apps_including_uninstalling_ = false;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILTER_H_
