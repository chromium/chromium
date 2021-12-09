// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_TYPES_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_TYPES_H_

#include <string>
#include <utility>

#include "components/services/app_service/public/cpp/icon_types.h"
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

  absl::optional<IconKey> icon_key;

  // TODO(crbug.com/1253250): Add other App struct fields.

  // When adding new fields to the App type, the `Clone` function and the
  // `AppUpdate` class should also be updated.
};

// TODO(crbug.com/1253250): Remove these functions after migrating to non-mojo
// AppService.
COMPONENT_EXPORT(APP_TYPES)
AppType ConvertMojomAppTypToAppType(apps::mojom::AppType mojom_app_type);

COMPONENT_EXPORT(APP_TYPES)
Readiness ConvertMojomReadinessToReadiness(
    apps::mojom::Readiness mojom_readiness);

COMPONENT_EXPORT(APP_TYPES)
std::unique_ptr<App> ConvertMojomAppToApp(const apps::mojom::AppPtr& mojom_app);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_TYPES_H_
