// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"

namespace apps {

// Information about an app. See components/services/app_service/README.md.
struct COMPONENT_EXPORT(APP_TYPES) App {
  App(AppType app_type, const std::string& app_id);

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  ~App();

  bool operator==(const App& other) const;
  bool operator!=(const App& other) const;

  std::unique_ptr<App> Clone() const;

  // Adds a new field for `extra`. The type `T` can be any type, e.g. int,
  // double, string, base::Value::Dict, base::Value::List, base::Value, etc. The
  // value is saved in base::Value::Dict `extra`. If the type `T` can't be
  // converted to base::Value, an explicit convert function can be added to
  // convert `value` to base::Value.
  template <typename T>
  void SetExtraField(const std::string& field_name, T&& value) {
    if (!extra.has_value()) {
      extra = base::Value::Dict();
    }
    extra->Set(field_name, value);
  }

  AppType app_type;
  std::string app_id;

  Readiness readiness = Readiness::kUnknown;

  // The full name of the app. Will be used in most UIs.
  std::optional<std::string> name;
  // A shortened version of the app name. May omit branding (e.g.
  // "Google" prefixes) or rely on abbreviations (e.g. "YT Music"). If no
  // `short_name` is supplied, the `name` will be used instead.
  // The `short_name` may be used in UIs where space is limited and/or we want
  // to optimize for scannability.
  std::optional<std::string> short_name;

  // An optional, publisher-specific ID for this app, e.g. for Android apps,
  // this field contains the Android package name, and for web apps, it
  // contains the start URL.
  std::optional<std::string> publisher_id;

  // An optional ID for the package that installed this app. In general, this
  // will match the `app_type` and `publisher_id` of the app. However, this is
  // permitted to diverge for alternate installation methods, e.g. web apps that
  // are installed through the Play Store.
  std::optional<PackageId> installer_package_id;

  std::optional<std::string> description;
  std::optional<std::string> version;
  std::vector<std::string> additional_search_terms;

  std::optional<IconKey> icon_key;

  std::optional<base::Time> last_launch_time;
  std::optional<base::Time> install_time;

  // This vector must be treated atomically, if there is a permission
  // change, the publisher must send through the entire list of permissions.
  // Should contain no duplicate IDs.
  // If empty during updates, Subscriber can assume no changes.
  // There is no guarantee that this is sorted by any criteria.
  Permissions permissions;

  // The main reason why this app is currently installed on the device (e.g.
  // because it is required by Policy). This may change over time and is not
  // necessarily the reason why the app was originally installed.
  InstallReason install_reason = InstallReason::kUnknown;

  // How installation of the app was triggered on this device. Either a UI
  // surface (e.g. Play Store), or a system component (e.g. Sync).
  InstallSource install_source = InstallSource::kUnknown;

  // IDs used for policy to identify the app.
  // For web apps, it contains the install URL(s).
  std::vector<std::string> policy_ids;

  // Whether the app is an extensions::Extensions where is_platform_app()
  // returns true.
  std::optional<bool> is_platform_app;

  std::optional<bool> recommendable;
  std::optional<bool> searchable;
  std::optional<bool> show_in_launcher;
  std::optional<bool> show_in_shelf;
  std::optional<bool> show_in_search;
  std::optional<bool> show_in_management;

  // True if the app is able to handle intents and should be shown in intent
  // surfaces.
  std::optional<bool> handles_intents;

  // Whether the app publisher allows the app to be uninstalled.
  std::optional<bool> allow_uninstall;

  // Whether the app icon should add the notification badging.
  std::optional<bool> has_badge;

  // Paused apps cannot be launched, and any running apps that become paused
  // will be stopped. This is independent of whether or not the app is ready to
  // be launched (defined by the Readiness field).
  std::optional<bool> paused;

  // This vector stores all the intent filters defined in this app. Each
  // intent filter defines a matching criteria for whether an intent can
  // be handled by this app. One app can have multiple intent filters.
  IntentFilters intent_filters;

  // Whether the app can be free resized. If this is true, various resizing
  // operations will be restricted.
  std::optional<bool> resize_locked;

  // Whether the app's display mode is in the browser or otherwise.
  WindowMode window_mode = WindowMode::kUnknown;

  // Whether the app runs on os login in a new window or not.
  std::optional<RunOnOsLogin> run_on_os_login;

  // Whether the app can be closed by the user.
  std::optional<bool> allow_close;

  // If the value is false, User will not be able to select open mode (i.e.
  // open in new window or in new tab) for the app. The app will default to be
  // opened in a new window.
  std::optional<bool> allow_window_mode_selection;

  // Storage space size for app and associated data.
  std::optional<uint64_t> app_size_in_bytes;
  std::optional<uint64_t> data_size_in_bytes;

  // App-specified supported locales.
  std::vector<std::string> supported_locales;
  // Currently selected locale, empty string means system language is used.
  // ARC-specific note: Based on Android implementation, `selected_locale`
  //  is not necessarily part of `supported_locales`.
  std::optional<std::string> selected_locale;

  // The extra information used by the app platform(e.g. ARC, GuestOS) for an
  // app. `extra` needs to be modified as a whole, and we can't only modify part
  // of `extra`. AppService doesn't use the fields saved in `extra`. App
  // publishers modify the content saved in `extra`.
  std::optional<base::Value::Dict> extra;

  // When adding new fields to the App type, the `Clone` function, the
  // `operator==` function, and the `AppUpdate` class should also be updated. If
  // the new fields should be saved, below functions should be updated:
  // `AppStorage::IsAppChanged`
  // `AppStorageFileHandler::ConvertAppsToValue`
  // `AppStorageFileHandler::ConvertValueToApps`
};

using AppPtr = std::unique_ptr<App>;

COMPONENT_EXPORT(APP_TYPES)
bool IsEqual(const std::vector<AppPtr>& source,
             const std::vector<AppPtr>& target);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_H_
