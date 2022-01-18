// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_TYPES_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_TYPES_H_

#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

enum class AppType {
  kUnknown = 0,
  kArc = 1,                // Android app.
  kBuiltIn = 2,            // Built-in app.
  kCrostini = 3,           // Linux (via Crostini) app.
  kChromeApp = 4,          // Chrome app.
  kWeb = 5,                // Web app.
  kMacOs = 6,              // Mac OS app.
  kPluginVm = 7,           // Plugin VM app, see go/pluginvm.
  kStandaloneBrowser = 8,  // Lacros browser app, see //docs/lacros.md.
  kRemote = 9,             // Remote app.
  kBorealis = 10,          // Borealis app, see go/borealis-app.
  kSystemWeb = 11,         // System web app.
  kStandaloneBrowserChromeApp = 12,  // Chrome app hosted in Lacros.
  kExtension = 13,                   // Browser extension.
};

// Whether an app is ready to launch, i.e. installed.
// Note the enumeration is used in UMA histogram so entries should not be
// re-ordered or removed. New entries should be added at the bottom.
enum class Readiness {
  kUnknown = 0,
  kReady,                // Installed and launchable.
  kDisabledByBlocklist,  // Disabled by SafeBrowsing.
  kDisabledByPolicy,     // Disabled by admin policy.
  kDisabledByUser,       // Disabled by explicit user action.
  kTerminated,           // Renderer process crashed.
  kUninstalledByUser,
  // Removed apps are purged from the registry cache and have their
  // associated memory freed. Subscribers are not notified of removed
  // apps, so publishers must set the app as uninstalled before
  // removing it.
  kRemoved,
  kUninstalledByMigration,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kUninstalledByMigration,
};

// How the app was installed.
// This should be kept in sync with histograms.xml, and InstallReason in
// enums.xml.
// Note the enumeration is used in UMA histogram so entries should not be
// re-ordered or removed. New entries should be added at the bottom.
enum class InstallReason {
  kUnknown = 0,
  kSystem,   // Installed with the system and is considered a part of the OS.
  kPolicy,   // Installed by policy.
  kOem,      // Installed by an OEM.
  kDefault,  // Preinstalled by default, but is not considered a system app.
  kSync,     // Installed by sync.
  kUser,     // Installed by user action.
  kSubApp,   // Installed by the SubApp API call.

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kSubApp,
};

// Where the app was installed from.
// This should be kept in sync with histograms.xml, and InstallSource in
// enums.xml.
// Note the enumeration is used in UMA histogram so entries should not be
// re-ordered or removed. New entries should be added at the bottom.
enum class InstallSource {
  kUnknown = 0,
  kSystem,          // Installed as part of Chrome OS.
  kSync,            // Installed from sync.
  kPlayStore,       // Installed from Play store.
  kChromeWebStore,  // Installed from Chrome web store.
  kBrowser,         // Installed from browser.

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kBrowser,
};

struct COMPONENT_EXPORT(APP_TYPES) App {
  App(AppType app_type, const std::string& app_id);

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  ~App();

  std::unique_ptr<App> Clone() const;

  AppType app_type;
  std::string app_id;

  Readiness readiness = Readiness::kUnknown;
  absl::optional<std::string> name;
  absl::optional<std::string> short_name;

  // An optional, publisher-specific ID for this app, e.g. for Android apps,
  // this field contains the Android package name, and for web apps, it
  // contains the start URL.
  absl::optional<std::string> publisher_id;

  absl::optional<std::string> description;
  absl::optional<std::string> version;
  std::vector<std::string> additional_search_terms;

  absl::optional<IconKey> icon_key;

  absl::optional<base::Time> last_launch_time;
  absl::optional<base::Time> install_time;

  // This vector must be treated atomically, if there is a permission
  // change, the publisher must send through the entire list of permissions.
  // Should contain no duplicate IDs.
  // If empty during updates, Subscriber can assume no changes.
  // There is no guarantee that this is sorted by any criteria.
  Permissions permissions;

  // Whether the app was installed by sync, policy or as a default app.
  InstallReason install_reason = InstallReason::kUnknown;

  // Where the app was installed from, e.g. from Play Store, from Chrome Web
  // Store, etc.
  InstallSource install_source = InstallSource::kUnknown;

  // An optional ID used for policy to identify the app.
  // For web apps, it contains the install URL.
  absl::optional<std::string> policy_id;

  // TODO(crbug.com/1253250): Add other App struct fields.

  // When adding new fields to the App type, the `Clone` function and the
  // `AppUpdate` class should also be updated.
};

// TODO(crbug.com/1253250): Remove these functions after migrating to non-mojo
// AppService.
COMPONENT_EXPORT(APP_TYPES)
AppType ConvertMojomAppTypToAppType(apps::mojom::AppType mojom_app_type);

COMPONENT_EXPORT(APP_TYPES)
mojom::AppType ConvertAppTypeToMojomAppType(AppType mojom_app_type);

COMPONENT_EXPORT(APP_TYPES)
Readiness ConvertMojomReadinessToReadiness(
    apps::mojom::Readiness mojom_readiness);

COMPONENT_EXPORT(APP_TYPES)
InstallReason ConvertMojomInstallReasonToInstallReason(
    apps::mojom::InstallReason mojom_install_reason);

COMPONENT_EXPORT(APP_TYPES)
InstallSource ConvertMojomInstallSourceToInstallSource(
    apps::mojom::InstallSource mojom_install_source);

COMPONENT_EXPORT(APP_TYPES)
std::unique_ptr<App> ConvertMojomAppToApp(const apps::mojom::AppPtr& mojom_app);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_TYPES_H_
