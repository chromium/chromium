// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILTER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILTER_H_

#include <memory>
#include <optional>
#include <variant>

#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_management_type.h"

namespace web_app {

// Describe capabilities that web apps can have. Used to query for apps within
// the WebAppRegistrar that have certain capabilities.
class WebAppFilter {
 public:
  // Only consider web apps whose effective display mode is a browser tab, or it
  // is undefined.
  static WebAppFilter OpensInBrowserTab();
  // Only consider web apps whose effective display mode is a dedicated window
  // (essentially any display mode other than a browser tab).
  static WebAppFilter OpensInDedicatedWindow();
  // Only consider web apps that were installed by the user.
  static WebAppFilter InstalledByUser();
  // Only consider web apps that were preinstalled by the device.
  static WebAppFilter InstalledByDefaultManagement();
  // Only consider isolated web apps, that are not scheduled for uninstallation,
  // like stub ones. To also consider stub apps, use
  // `IsIsolatedWebAppIncludingUninstalling()` instead.
  static WebAppFilter IsIsolatedApp();
  // Only consider sub apps of isolated web apps (connected via parent_app_id).
  static WebAppFilter IsIsolatedSubApp();
  // Only consider isolated web apps installed in developer mode, that are not
  // scheduled for uninstallation, like stub ones.
  static WebAppFilter IsDevModeIsolatedApp();
  // Only consider force-installed Isolated Web Apps.
  static WebAppFilter PolicyInstalledIsolatedWebApp();
  // Only consider user installed Isolated Web Apps
  static WebAppFilter UserInstalledIsolatedWebApp();
  // Only consider user installed Isolated Web Apps without any external
  // management (policy/kiosk/shimless).
  static WebAppFilter IsIsolatedWebAppWithOnlyUserManagement();
  // Only consider crafted web apps (not DIY apps).
  static WebAppFilter IsCraftedApp();
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

  // Only consider web apps that are in the middle of an app migration, and will
  // be treated rightfully so.
  static WebAppFilter IsAppSuggestedForMigration();

  // Only consider web apps that can be surfaced to the user for use in various
  // UX surfaces. Apps that are not in the registry, or are suggested for
  // migration are not included here.
  static WebAppFilter IsAppSurfaceableToUser();

  // Only consider web apps that are valid sources to be migrated to a different
  // app. This mainly includes apps that have valid OS integration and are not
  // installed by policy.
  static WebAppFilter IsAppValidMigrationSource();

  // Only consider web apps that are eligible for manifest updates. This
  // includes apps in all states (including suggested from migration and
  // suggested from sync) as long as they are not marked for uninstallation.
  static WebAppFilter IsAppEligibleForManifestUpdate();

  WebAppFilter(const WebAppFilter&);
  WebAppFilter(WebAppFilter&&) noexcept;
  WebAppFilter& operator=(WebAppFilter&&) noexcept;
  ~WebAppFilter();

  friend WebAppFilter operator&(WebAppFilter lhs, WebAppFilter rhs);
  friend WebAppFilter operator|(WebAppFilter lhs, WebAppFilter rhs);

  // Logical exclusion. This operator returns a filter that matches all apps
  // that are eligible for manifest updates (i.e., not stubs or uninstalling),
  // EXCEPT for those that match the provided `filter`.
  //
  // Example:
  // If the registrar contains apps {A, B, C, D} and `filter` matches {A, B},
  // then `!filter` will match {C, D}.
  friend WebAppFilter operator!(WebAppFilter filter);

 private:
  friend class WebAppRegistrar;

  enum class SimpleCondition {
    kIsDiy,
    kWasInstalledByUser,
    kInstalledByTrustedSource,
    kIsolatedApp,
    kIsolatedAppDevMode,
    kIsolatedSubApp,
    kOpensInDedicatedWindow
  };

  using InstallStateSet = base::EnumSet<proto::InstallState,
                                        proto::InstallState_MIN,
                                        proto::InstallState_MAX>;

  struct ManagementRequirement {
    enum class Type { kHasAny, kHasAll } type;
    WebAppManagementTypes sources;
  };

  using LeafFilter =
      std::variant<ManagementRequirement, InstallStateSet, SimpleCondition>;

  static WebAppFilter HasSource(WebAppManagement::Type source);
  static WebAppFilter HasAnySource(WebAppManagementTypes sources);
  static WebAppFilter HasAllSources(WebAppManagementTypes sources);

  static WebAppFilter InstallStateIs(proto::InstallState state);
  static WebAppFilter InstallStateIsAnyOf(InstallStateSet states);

  static WebAppFilter IsInRegistrar();

  static WebAppFilter IsTrue(SimpleCondition condition);

  struct BinaryOp {
    enum class Op {
      kAnd,
      kOr,
      // Matches `left`, excluding matches for `right`.
      kExclude
    };
    BinaryOp(std::unique_ptr<WebAppFilter> left,
             std::unique_ptr<WebAppFilter> right,
             Op op);
    ~BinaryOp();
    BinaryOp(const BinaryOp&);
    BinaryOp(BinaryOp&&) noexcept;
    BinaryOp& operator=(BinaryOp&&) noexcept;

    std::unique_ptr<WebAppFilter> left;
    std::unique_ptr<WebAppFilter> right;
    Op op;
  };

  explicit WebAppFilter(LeafFilter leaf);
  WebAppFilter(WebAppFilter left, WebAppFilter right, BinaryOp::Op op);

  std::variant<LeafFilter, BinaryOp> data_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILTER_H_
