// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_UPDATE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_UPDATE_H_

#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/services/app_service/public/cpp/permission.h"

namespace apps {

class AppRegistryCacheTest;
struct IconKey;
struct RunOnOsLogin;

// Wraps two apps::AppPtr's, a prior state and a delta on top of that
// state. The state is conceptually the "sum" of all of the previous deltas,
// with "addition" or "merging" simply being that the most recent version of
// each field "wins".
//
// The state may be nullptr, meaning that there are no previous deltas.
// Alternatively, the delta may be nullptr, meaning that there is no change in
// state. At least one of state and delta must be non-nullptr.
//
// Almost all of an AppPtr's fields are optional. For example, if an app's name
// hasn't changed, then a delta doesn't necessarily have to contain a copy of
// the name, as the prior state should already contain it.
//
// The combination of the two (state and delta) can answer questions such as:
//  - What is the app's name? If the delta knows, that's the answer. Otherwise,
//    ask the state.
//  - Is the app ready to launch (i.e. installed)? Likewise, if the delta says
//    yes or no, that's the answer. Otherwise, the delta says "unknown", so ask
//    the state.
//  - Was the app *freshly* installed (i.e. it previously wasn't installed, but
//    now is)? Has its readiness changed? Check if the delta says "installed"
//    and the state says either "uninstalled" or unknown.
//
// An AppUpdate is read-only once constructed. All of its fields and methods
// are const. The constructor caller must guarantee that the AppPtr references
// remain valid for the lifetime of the AppUpdate.
//
// See components/services/app_service/README.md for more details.
class COMPONENT_EXPORT(APP_UPDATE) AppUpdate {
 public:
  // Modifies `new_delta` by copying over all of `delta`'s known fields: those
  // fields whose values aren't "unknown". The `new_delta` may not be nullptr.
  //
  // For `icon_key`, if `new_delta`'s `update_version` is true, keep that as
  // true. Otherwise, copying `delta`'s `icon_key` if it has a value.
  static void MergeDelta(App* new_delta, App* delta);

  // Modifies `state` by copying over all of `delta`'s known fields: those
  // fields whose values aren't "unknown". The `state` may not be nullptr.
  //
  // For `icon_key`, if `delta`'s `update_version` is true, increase `state`'s
  // `update_version`.
  static void Merge(App* state, const App* delta);

  // Returns true if there are some changed for `delta` compared with `state`.
  // Otherwise, returns false. `state` and `delta` must have the same`app_id`.
  static bool IsChanged(const App* state, const App* delta);

  // At most one of |state| or |delta| may be nullptr.
  AppUpdate(const App* state, const App* delta, const AccountId& account_id);

  AppUpdate(const AppUpdate&);
  AppUpdate& operator=(const AppUpdate&);

  // Returns whether this is the first update for the given AppId.
  // Equivalently, there are no previous deltas for the AppId.
  bool StateIsNull() const;

  apps::AppType AppType() const;

  const std::string& AppId() const;

  apps::Readiness Readiness() const;
  apps::Readiness PriorReadiness() const;
  bool ReadinessChanged() const;

  // The full name of the app. This is the name that should be used by default
  // in most UIs.
  const std::string& Name() const;
  bool NameChanged() const;

  // A possibly shortened version of the app name. May omit branding (e.g.
  // "Google" prefixes) or rely on abbreviations (e.g. "YT Music"). If the
  // developer/publisher does not supply a short name, this will be the same as
  // the Name() field. May be used in UIs where space is limited and/or we want
  // to optimize for scannability.
  const std::string& ShortName() const;
  bool ShortNameChanged() const;

  // The publisher-specific ID for this app, e.g. for Android apps, this field
  // contains the Android package name. May be empty if AppId() should be
  // considered as the canonical publisher ID.
  const std::string& PublisherId() const;
  bool PublisherIdChanged() const;

  // An optional PackageId for the package that installed this app. In general,
  // this will match the AppType() and PublisherId() of the app. However, this
  // is permitted to diverge for alternate installation methods, e.g. web apps
  // that are installed through the Play Store.
  const std::optional<PackageId> InstallerPackageId() const;
  bool InstallerPackageIdChanged() const;

  const std::string& Description() const;
  bool DescriptionChanged() const;

  const std::string& Version() const;
  bool VersionChanged() const;

  const std::vector<std::string>& AdditionalSearchTerms() const;
  bool AdditionalSearchTermsChanged() const;

  std::optional<apps::IconKey> IconKey() const;
  bool IconKeyChanged() const;

  base::Time LastLaunchTime() const;
  bool LastLaunchTimeChanged() const;

  base::Time InstallTime() const;
  bool InstallTimeChanged() const;

  apps::Permissions Permissions() const;
  bool PermissionsChanged() const;

  // The main reason why this app is currently installed on the device (e.g.
  // because it is required by Policy). This may change over time and is not
  // necessarily the reason why the app was originally installed.
  apps::InstallReason InstallReason() const;
  bool InstallReasonChanged() const;

  // How installation of the app was triggered on this device. Either a UI
  // surface (e.g. Play Store), or a system component (e.g. Sync).
  apps::InstallSource InstallSource() const;
  bool InstallSourceChanged() const;

  // IDs used for policy to identify the app.
  // For web apps, it contains the install URL(s).
  const std::vector<std::string>& PolicyIds() const;
  bool PolicyIdsChanged() const;

  bool InstalledInternally() const;

  std::optional<bool> IsPlatformApp() const;
  bool IsPlatformAppChanged() const;

  std::optional<bool> Recommendable() const;
  bool RecommendableChanged() const;

  std::optional<bool> Searchable() const;
  bool SearchableChanged() const;

  std::optional<bool> ShowInLauncher() const;
  bool ShowInLauncherChanged() const;

  std::optional<bool> ShowInShelf() const;
  bool ShowInShelfChanged() const;

  std::optional<bool> ShowInSearch() const;
  bool ShowInSearchChanged() const;

  std::optional<bool> ShowInManagement() const;
  bool ShowInManagementChanged() const;

  std::optional<bool> HandlesIntents() const;
  bool HandlesIntentsChanged() const;

  std::optional<bool> AllowUninstall() const;
  bool AllowUninstallChanged() const;

  std::optional<bool> HasBadge() const;
  bool HasBadgeChanged() const;

  std::optional<bool> Paused() const;
  bool PausedChanged() const;

  apps::IntentFilters IntentFilters() const;
  bool IntentFiltersChanged() const;

  std::optional<bool> ResizeLocked() const;
  bool ResizeLockedChanged() const;

  std::optional<bool> AllowWindowModeSelection() const;
  bool AllowWindowModeSelectionChanged() const;

  apps::WindowMode WindowMode() const;
  bool WindowModeChanged() const;

  std::optional<apps::RunOnOsLogin> RunOnOsLogin() const;
  bool RunOnOsLoginChanged() const;

  std::optional<bool> AllowClose() const;
  bool AllowCloseChanged() const;

  const ::AccountId& AccountId() const;

  std::optional<uint64_t> AppSizeInBytes() const;
  bool AppSizeInBytesChanged() const;

  std::optional<uint64_t> DataSizeInBytes() const;
  bool DataSizeInBytesChanged() const;

  // App-specified supported locales.
  const std::vector<std::string>& SupportedLocales() const;
  bool SupportedLocalesChanged() const;

  // Currently selected locale, empty string means system language is used.
  // ARC-specific note: Based on Android implementation, `selected_locale`
  //  is not necessarily part of `supported_locales`.
  std::optional<std::string> SelectedLocale() const;
  bool SelectedLocaleChanged() const;

  std::optional<base::Value::Dict> Extra() const;
  bool ExtraChanged() const;

  const App* State() const { return state_.get(); }
  const App* Delta() const { return delta_.get(); }

 private:
  friend class AppRegistryCacheTest;

  raw_ptr<const apps::App, DanglingUntriaged> state_ = nullptr;
  raw_ptr<const apps::App, DanglingUntriaged> delta_ = nullptr;

  raw_ref<const ::AccountId> account_id_;
};

// For logging and debug purposes.
COMPONENT_EXPORT(APP_UPDATE)
std::ostream& operator<<(std::ostream& out, const AppUpdate& app);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_UPDATE_H_
