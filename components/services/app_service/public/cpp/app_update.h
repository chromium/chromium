// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_UPDATE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_UPDATE_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

struct IconKey;
struct RunOnOsLogin;

// Wraps two apps::mojom::AppPtr's, a prior state and a delta on top of that
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
//
// TODO(crbug.com/1253250): Remove all apps::mojom related code.
// 1. Modify comments.
// 2. Replace mojom related functions with non-mojom functions.
class COMPONENT_EXPORT(APP_UPDATE) AppUpdate {
 public:
  // Modifies |state| by copying over all of |delta|'s known fields: those
  // fields whose values aren't "unknown". The |state| may not be nullptr.
  static void Merge(apps::mojom::App* state, const apps::mojom::App* delta);
  static void Merge(App* state, const App* delta);

  // At most one of |state| or |delta| may be nullptr.
  AppUpdate(const apps::mojom::App* state,
            const apps::mojom::App* delta,
            const AccountId& account_id);
  AppUpdate(const App* state, const App* delta, const AccountId& account_id);

  AppUpdate(const AppUpdate&) = delete;
  AppUpdate& operator=(const AppUpdate&) = delete;

  // Returns whether this is the first update for the given AppId.
  // Equivalently, there are no previous deltas for the AppId.
  bool StateIsNull() const;

  apps::mojom::AppType AppType() const;
  apps::AppType GetAppType() const;

  const std::string& AppId() const;
  const std::string& GetAppId() const;

  apps::mojom::Readiness Readiness() const;
  apps::mojom::Readiness PriorReadiness() const;
  apps::Readiness GetReadiness() const;
  apps::Readiness GetPriorReadiness() const;
  bool ReadinessChanged() const;

  const std::string& Name() const;
  const std::string& GetName() const;
  bool NameChanged() const;

  const std::string& ShortName() const;
  const std::string& GetShortName() const;
  bool ShortNameChanged() const;

  // The publisher-specific ID for this app, e.g. for Android apps, this field
  // contains the Android package name. May be empty if AppId() should be
  // considered as the canonical publisher ID.
  const std::string& PublisherId() const;
  const std::string& GetPublisherId() const;
  bool PublisherIdChanged() const;

  const std::string& Description() const;
  const std::string& GetDescription() const;
  bool DescriptionChanged() const;

  const std::string& Version() const;
  const std::string& GetVersion() const;
  bool VersionChanged() const;

  std::vector<std::string> AdditionalSearchTerms() const;
  std::vector<std::string> GetAdditionalSearchTerms() const;
  bool AdditionalSearchTermsChanged() const;

  apps::mojom::IconKeyPtr IconKey() const;
  absl::optional<apps::IconKey> GetIconKey() const;
  bool IconKeyChanged() const;

  base::Time LastLaunchTime() const;
  base::Time GetLastLaunchTime() const;
  bool LastLaunchTimeChanged() const;

  base::Time InstallTime() const;
  base::Time GetInstallTime() const;
  bool InstallTimeChanged() const;

  std::vector<apps::mojom::PermissionPtr> Permissions() const;
  apps::Permissions GetPermissions() const;
  bool PermissionsChanged() const;

  apps::mojom::InstallReason InstallReason() const;
  apps::InstallReason GetInstallReason() const;
  bool InstallReasonChanged() const;

  apps::mojom::InstallSource InstallSource() const;
  apps::InstallSource GetInstallSource() const;
  bool InstallSourceChanged() const;

  // An optional ID used for policy to identify the app.
  // For web apps, it contains the install URL.
  const std::string& PolicyId() const;
  const std::string& GetPolicyId() const;
  bool PolicyIdChanged() const;

  apps::mojom::OptionalBool InstalledInternally() const;

  apps::mojom::OptionalBool IsPlatformApp() const;
  absl::optional<bool> GetIsPlatformApp() const;
  bool IsPlatformAppChanged() const;

  apps::mojom::OptionalBool Recommendable() const;
  absl::optional<bool> GetRecommendable() const;
  bool RecommendableChanged() const;

  apps::mojom::OptionalBool Searchable() const;
  absl::optional<bool> GetSearchable() const;
  bool SearchableChanged() const;

  apps::mojom::OptionalBool ShowInLauncher() const;
  absl::optional<bool> GetShowInLauncher() const;
  bool ShowInLauncherChanged() const;

  apps::mojom::OptionalBool ShowInShelf() const;
  absl::optional<bool> GetShowInShelf() const;
  bool ShowInShelfChanged() const;

  apps::mojom::OptionalBool ShowInSearch() const;
  absl::optional<bool> GetShowInSearch() const;
  bool ShowInSearchChanged() const;

  apps::mojom::OptionalBool ShowInManagement() const;
  absl::optional<bool> GetShowInManagement() const;
  bool ShowInManagementChanged() const;

  apps::mojom::OptionalBool HandlesIntents() const;
  absl::optional<bool> GetHandlesIntents() const;
  bool HandlesIntentsChanged() const;

  apps::mojom::OptionalBool AllowUninstall() const;
  absl::optional<bool> GetAllowUninstall() const;
  bool AllowUninstallChanged() const;

  apps::mojom::OptionalBool HasBadge() const;
  absl::optional<bool> GetHasBadge() const;
  bool HasBadgeChanged() const;

  apps::mojom::OptionalBool Paused() const;
  absl::optional<bool> GetPaused() const;
  bool PausedChanged() const;

  std::vector<apps::mojom::IntentFilterPtr> IntentFilters() const;
  apps::IntentFilters GetIntentFilters() const;
  bool IntentFiltersChanged() const;

  apps::mojom::OptionalBool ResizeLocked() const;
  absl::optional<bool> GetResizeLocked() const;
  bool ResizeLockedChanged() const;

  apps::mojom::WindowMode WindowMode() const;
  apps::WindowMode GetWindowMode() const;
  bool WindowModeChanged() const;

  apps::mojom::RunOnOsLoginPtr RunOnOsLogin() const;
  absl::optional<apps::RunOnOsLogin> GetRunOnOsLogin() const;
  bool RunOnOsLoginChanged() const;

  const ::AccountId& AccountId() const;

 private:
  raw_ptr<const apps::mojom::App> mojom_state_ = nullptr;
  raw_ptr<const apps::mojom::App> mojom_delta_ = nullptr;

  raw_ptr<const apps::App> state_ = nullptr;
  raw_ptr<const apps::App> delta_ = nullptr;

  const ::AccountId& account_id_;
};

// For logging and debug purposes.
COMPONENT_EXPORT(APP_UPDATE)
std::ostream& operator<<(std::ostream& out, const AppUpdate& app);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_UPDATE_H_
