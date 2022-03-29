// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_update.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/macros.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"

namespace {

std::vector<apps::PermissionPtr> ConvertMojomPermissionsToPermissions(
    const std::vector<apps::mojom::PermissionPtr>& mojom_permissions) {
  std::vector<apps::PermissionPtr> permissions;
  for (const auto& mojom_permission : mojom_permissions) {
    permissions.push_back(
        apps::ConvertMojomPermissionToPermission(mojom_permission));
  }
  return permissions;
}

void CloneMojomPermissions(
    const std::vector<apps::mojom::PermissionPtr>& clone_from,
    std::vector<apps::mojom::PermissionPtr>* clone_to) {
  for (const auto& permission : clone_from) {
    clone_to->push_back(permission->Clone());
  }
}

void CloneStrings(const std::vector<std::string>& clone_from,
                  std::vector<std::string>* clone_to) {
  for (const auto& s : clone_from) {
    clone_to->push_back(s);
  }
}

std::vector<apps::IntentFilterPtr> ConvertMojomIntentFiltersToIntentFilters(
    const std::vector<apps::mojom::IntentFilterPtr>& mojom_intent_filters) {
  std::vector<apps::IntentFilterPtr> intent_filters;
  for (const auto& mojom_intent_filter : mojom_intent_filters) {
    intent_filters.push_back(
        apps::ConvertMojomIntentFilterToIntentFilter(mojom_intent_filter));
  }
  return intent_filters;
}

void CloneMojomIntentFilters(
    const std::vector<apps::mojom::IntentFilterPtr>& clone_from,
    std::vector<apps::mojom::IntentFilterPtr>* clone_to) {
  for (const auto& intent_filter : clone_from) {
    clone_to->push_back(intent_filter->Clone());
  }
}

absl::optional<apps::RunOnOsLogin> CloneRunOnOsLogin(
    const apps::RunOnOsLogin& login_data) {
  return apps::RunOnOsLogin(login_data.login_mode, login_data.is_managed);
}

}  // namespace

namespace apps {

// static
void AppUpdate::Merge(apps::mojom::App* state, const apps::mojom::App* delta) {
  DCHECK(state);
  if (!delta) {
    return;
  }

  if ((delta->app_type != state->app_type) ||
      (delta->app_id != state->app_id)) {
    LOG(ERROR) << "inconsistent (app_type, app_id): (" << delta->app_type
               << ", " << delta->app_id << ") vs (" << state->app_type << ", "
               << state->app_id << ") ";
    DCHECK(false);
    return;
  }

  // You can not merge removed states.
  DCHECK(state->readiness != mojom::Readiness::kRemoved);
  DCHECK(delta->readiness != mojom::Readiness::kRemoved);

  if (delta->readiness != apps::mojom::Readiness::kUnknown) {
    state->readiness = delta->readiness;
  }
  if (delta->name.has_value()) {
    state->name = delta->name;
  }
  if (delta->short_name.has_value()) {
    state->short_name = delta->short_name;
  }
  if (delta->publisher_id.has_value()) {
    state->publisher_id = delta->publisher_id;
  }
  if (delta->description.has_value()) {
    state->description = delta->description;
  }
  if (delta->version.has_value()) {
    state->version = delta->version;
  }
  if (!delta->additional_search_terms.empty()) {
    state->additional_search_terms.clear();
    CloneStrings(delta->additional_search_terms,
                 &state->additional_search_terms);
  }
  if (!delta->icon_key.is_null()) {
    state->icon_key = delta->icon_key.Clone();
  }
  if (delta->last_launch_time.has_value()) {
    state->last_launch_time = delta->last_launch_time;
  }
  if (delta->install_time.has_value()) {
    state->install_time = delta->install_time;
  }
  if (!delta->permissions.empty()) {
    state->permissions.clear();
    ::CloneMojomPermissions(delta->permissions, &state->permissions);
  }
  if (delta->install_reason != apps::mojom::InstallReason::kUnknown) {
    state->install_reason = delta->install_reason;
  }
  if (delta->install_source != apps::mojom::InstallSource::kUnknown) {
    state->install_source = delta->install_source;
  }
  if (delta->policy_id.has_value()) {
    state->policy_id = delta->policy_id;
  }
  if (delta->is_platform_app != apps::mojom::OptionalBool::kUnknown) {
    state->is_platform_app = delta->is_platform_app;
  }
  if (delta->recommendable != apps::mojom::OptionalBool::kUnknown) {
    state->recommendable = delta->recommendable;
  }
  if (delta->searchable != apps::mojom::OptionalBool::kUnknown) {
    state->searchable = delta->searchable;
  }
  if (delta->show_in_launcher != apps::mojom::OptionalBool::kUnknown) {
    state->show_in_launcher = delta->show_in_launcher;
  }
  if (delta->show_in_shelf != apps::mojom::OptionalBool::kUnknown) {
    state->show_in_shelf = delta->show_in_shelf;
  }
  if (delta->show_in_search != apps::mojom::OptionalBool::kUnknown) {
    state->show_in_search = delta->show_in_search;
  }
  if (delta->show_in_management != apps::mojom::OptionalBool::kUnknown) {
    state->show_in_management = delta->show_in_management;
  }
  if (delta->handles_intents != apps::mojom::OptionalBool::kUnknown) {
    state->handles_intents = delta->handles_intents;
  }
  if (delta->allow_uninstall != apps::mojom::OptionalBool::kUnknown) {
    state->allow_uninstall = delta->allow_uninstall;
  }
  if (delta->has_badge != apps::mojom::OptionalBool::kUnknown) {
    state->has_badge = delta->has_badge;
  }
  if (delta->paused != apps::mojom::OptionalBool::kUnknown) {
    state->paused = delta->paused;
  }
  if (!delta->intent_filters.empty()) {
    state->intent_filters.clear();
    ::CloneMojomIntentFilters(delta->intent_filters, &state->intent_filters);
  }
  if (delta->resize_locked != apps::mojom::OptionalBool::kUnknown) {
    state->resize_locked = delta->resize_locked;
  }
  if (delta->window_mode != apps::mojom::WindowMode::kUnknown) {
    state->window_mode = delta->window_mode;
  }
  if (!delta->run_on_os_login.is_null()) {
    state->run_on_os_login = delta->run_on_os_login.Clone();
  }

  // When adding new fields to the App Mojo type, this function should also be
  // updated.
}

// static
void AppUpdate::Merge(App* state, const App* delta) {
  DCHECK(state);
  if (!delta) {
    return;
  }

  if ((delta->app_type != state->app_type) ||
      (delta->app_id != state->app_id)) {
    NOTREACHED();
    return;
  }

  // You can not merge removed states.
  DCHECK_NE(state->readiness, Readiness::kRemoved);
  DCHECK_NE(delta->readiness, Readiness::kRemoved);

  SET_ENUM_VALUE(readiness, apps::Readiness::kUnknown);
  SET_OPTIONAL_VALUE(name)
  SET_OPTIONAL_VALUE(short_name)
  SET_OPTIONAL_VALUE(publisher_id)
  SET_OPTIONAL_VALUE(description)
  SET_OPTIONAL_VALUE(version)

  if (!delta->additional_search_terms.empty()) {
    state->additional_search_terms.clear();
    state->additional_search_terms = delta->additional_search_terms;
  }

  if (delta->icon_key.has_value()) {
    state->icon_key = std::move(*delta->icon_key->Clone());
  }

  SET_OPTIONAL_VALUE(last_launch_time);
  SET_OPTIONAL_VALUE(install_time);

  if (!delta->permissions.empty()) {
    state->permissions.clear();
    state->permissions = ClonePermissions(delta->permissions);
  }

  SET_ENUM_VALUE(install_reason, InstallReason::kUnknown);
  SET_ENUM_VALUE(install_source, InstallSource::kUnknown);
  SET_OPTIONAL_VALUE(policy_id);
  SET_OPTIONAL_VALUE(is_platform_app);
  SET_OPTIONAL_VALUE(recommendable);
  SET_OPTIONAL_VALUE(searchable);
  SET_OPTIONAL_VALUE(show_in_launcher);
  SET_OPTIONAL_VALUE(show_in_shelf);
  SET_OPTIONAL_VALUE(show_in_search);
  SET_OPTIONAL_VALUE(show_in_management);
  SET_OPTIONAL_VALUE(handles_intents);
  SET_OPTIONAL_VALUE(allow_uninstall);
  SET_OPTIONAL_VALUE(has_badge);
  SET_OPTIONAL_VALUE(paused);

  if (!delta->intent_filters.empty()) {
    state->intent_filters.clear();
    state->intent_filters = CloneIntentFilters(delta->intent_filters);
  }

  SET_OPTIONAL_VALUE(resize_locked)
  SET_ENUM_VALUE(window_mode, WindowMode::kUnknown)

  if (delta->run_on_os_login.has_value()) {
    state->run_on_os_login = CloneRunOnOsLogin(delta->run_on_os_login.value());
  }

  if (!delta->shortcuts.empty()) {
    state->shortcuts.clear();
    state->shortcuts = CloneShortcuts(delta->shortcuts);
  }

  // When adding new fields to the App type, this function should also be
  // updated.
}

AppUpdate::AppUpdate(const apps::mojom::App* state,
                     const apps::mojom::App* delta,
                     const ::AccountId& account_id)
    : mojom_state_(state), mojom_delta_(delta), account_id_(account_id) {
  DCHECK(mojom_state_ || mojom_delta_);
  if (mojom_state_ && mojom_delta_) {
    DCHECK(mojom_state_->app_type == delta->app_type);
    DCHECK(mojom_state_->app_id == delta->app_id);
  }
}

AppUpdate::AppUpdate(const App* state,
                     const App* delta,
                     const ::AccountId& account_id)
    : state_(state), delta_(delta), account_id_(account_id) {
  DCHECK(state_ || delta_);
  if (state_ && delta_) {
    DCHECK_EQ(state_->app_type, delta->app_type);
    DCHECK_EQ(state_->app_id, delta->app_id);
  }
}

bool AppUpdate::StateIsNull() const {
  if (ShouldUseNonMojom()) {
    return state_ == nullptr;
  }

  return mojom_state_ == nullptr;
}

apps::AppType AppUpdate::AppType() const {
  if (ShouldUseNonMojom()) {
    return delta_ ? delta_->app_type : state_->app_type;
  }
  return ConvertMojomAppTypToAppType(mojom_delta_ ? mojom_delta_->app_type
                                                  : mojom_state_->app_type);
}

const std::string& AppUpdate::AppId() const {
  if (ShouldUseNonMojom()) {
    return delta_ ? delta_->app_id : state_->app_id;
  }
  return mojom_delta_ ? mojom_delta_->app_id : mojom_state_->app_id;
}

apps::Readiness AppUpdate::Readiness() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_DEFAULT_VALUE(readiness, apps::Readiness::kUnknown)
  }

  if (mojom_delta_ &&
      (mojom_delta_->readiness != apps::mojom::Readiness::kUnknown)) {
    return ConvertMojomReadinessToReadiness(mojom_delta_->readiness);
  }
  if (mojom_state_) {
    return ConvertMojomReadinessToReadiness(mojom_state_->readiness);
  }
  return apps::Readiness::kUnknown;
}

apps::Readiness AppUpdate::PriorReadiness() const {
  if (ShouldUseNonMojom()) {
    return state_ ? state_->readiness : apps::Readiness::kUnknown;
  }

  return ConvertMojomReadinessToReadiness(
      mojom_state_ ? mojom_state_->readiness
                   : apps::mojom::Readiness::kUnknown);
}

bool AppUpdate::ReadinessChanged() const {
  if (ShouldUseNonMojom()) {
    IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(readiness, Readiness::kUnknown)
  }

  return mojom_delta_ &&
         (mojom_delta_->readiness != apps::mojom::Readiness::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->readiness != mojom_state_->readiness));
}

const std::string& AppUpdate::Name() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(name, base::EmptyString())
  }

  if (mojom_delta_ && mojom_delta_->name.has_value()) {
    return mojom_delta_->name.value();
  }
  if (mojom_state_ && mojom_state_->name.has_value()) {
    return mojom_state_->name.value();
  }
  return base::EmptyString();
}

bool AppUpdate::NameChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(name)

  return mojom_delta_ && mojom_delta_->name.has_value() &&
         (!mojom_state_ || (mojom_delta_->name != mojom_state_->name));
}

const std::string& AppUpdate::ShortName() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(short_name, base::EmptyString())
  }

  if (mojom_delta_ && mojom_delta_->short_name.has_value()) {
    return mojom_delta_->short_name.value();
  }
  if (mojom_state_ && mojom_state_->short_name.has_value()) {
    return mojom_state_->short_name.value();
  }
  return base::EmptyString();
}

bool AppUpdate::ShortNameChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(short_name)

  return mojom_delta_ && mojom_delta_->short_name.has_value() &&
         (!mojom_state_ ||
          (mojom_delta_->short_name != mojom_state_->short_name));
}

const std::string& AppUpdate::PublisherId() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(publisher_id, base::EmptyString())
  }

  if (mojom_delta_ && mojom_delta_->publisher_id.has_value()) {
    return mojom_delta_->publisher_id.value();
  }
  if (mojom_state_ && mojom_state_->publisher_id.has_value()) {
    return mojom_state_->publisher_id.value();
  }
  return base::EmptyString();
}

bool AppUpdate::PublisherIdChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(publisher_id)

  return mojom_delta_ && mojom_delta_->publisher_id.has_value() &&
         (!mojom_state_ ||
          (mojom_delta_->publisher_id != mojom_state_->publisher_id));
}

const std::string& AppUpdate::Description() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(description, base::EmptyString())
  }

  if (mojom_delta_ && mojom_delta_->description.has_value()) {
    return mojom_delta_->description.value();
  }
  if (mojom_state_ && mojom_state_->description.has_value()) {
    return mojom_state_->description.value();
  }
  return base::EmptyString();
}

bool AppUpdate::DescriptionChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(description)

  return mojom_delta_ && mojom_delta_->description.has_value() &&
         (!mojom_state_ ||
          (mojom_delta_->description != mojom_state_->description));
}

const std::string& AppUpdate::Version() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(version, base::EmptyString())
  }

  if (mojom_delta_ && mojom_delta_->version.has_value()) {
    return mojom_delta_->version.value();
  }
  if (mojom_state_ && mojom_state_->version.has_value()) {
    return mojom_state_->version.value();
  }
  return base::EmptyString();
}

bool AppUpdate::VersionChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(version)

  return mojom_delta_ && mojom_delta_->version.has_value() &&
         (!mojom_state_ || (mojom_delta_->version != mojom_state_->version));
}

std::vector<std::string> AppUpdate::AdditionalSearchTerms() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_CHECK_AND_DEFAULT_RETURN(additional_search_terms, empty,
                                            std::vector<std::string>{})
  }

  std::vector<std::string> additional_search_terms;

  if (mojom_delta_ && !mojom_delta_->additional_search_terms.empty()) {
    CloneStrings(mojom_delta_->additional_search_terms,
                 &additional_search_terms);
  } else if (mojom_state_ && !mojom_state_->additional_search_terms.empty()) {
    CloneStrings(mojom_state_->additional_search_terms,
                 &additional_search_terms);
  }

  return additional_search_terms;
}

bool AppUpdate::AdditionalSearchTermsChanged() const {
  if (ShouldUseNonMojom()) {
    IS_VALUE_CHANGED_WITH_CHECK(additional_search_terms, empty)
  }

  return mojom_delta_ && !mojom_delta_->additional_search_terms.empty() &&
         (!mojom_state_ || (mojom_delta_->additional_search_terms !=
                            mojom_state_->additional_search_terms));
}

absl::optional<apps::IconKey> AppUpdate::IconKey() const {
  apps::IconKey icon_key;
  if (ShouldUseNonMojom()) {
    if (delta_ && delta_->icon_key.has_value()) {
      icon_key = std::move(*delta_->icon_key->Clone());
      return icon_key;
    }
    if (state_ && state_->icon_key.has_value()) {
      icon_key = std::move(*state_->icon_key->Clone());
      return icon_key;
    }
    return absl::nullopt;
  }

  if (mojom_delta_ && !mojom_delta_->icon_key.is_null()) {
    icon_key = std::move(*ConvertMojomIconKeyToIconKey(mojom_delta_->icon_key));
    return icon_key;
  }
  if (mojom_state_ && !mojom_state_->icon_key.is_null()) {
    icon_key = std::move(*ConvertMojomIconKeyToIconKey(mojom_state_->icon_key));
    return icon_key;
  }
  return absl::nullopt;
}

bool AppUpdate::IconKeyChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(icon_key)

  return mojom_delta_ && !mojom_delta_->icon_key.is_null() &&
         (!mojom_state_ ||
          !mojom_delta_->icon_key.Equals(mojom_state_->icon_key));
}

base::Time AppUpdate::LastLaunchTime() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(last_launch_time, base::Time())
  }

  if (mojom_delta_ && mojom_delta_->last_launch_time.has_value()) {
    return mojom_delta_->last_launch_time.value();
  }
  if (mojom_state_ && mojom_state_->last_launch_time.has_value()) {
    return mojom_state_->last_launch_time.value();
  }
  return base::Time();
}

bool AppUpdate::LastLaunchTimeChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(last_launch_time)

  return mojom_delta_ && mojom_delta_->last_launch_time.has_value() &&
         (!mojom_state_ ||
          (mojom_delta_->last_launch_time != mojom_state_->last_launch_time));
}

base::Time AppUpdate::InstallTime() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(install_time, base::Time())
  }

  if (mojom_delta_ && mojom_delta_->install_time.has_value()) {
    return mojom_delta_->install_time.value();
  }
  if (mojom_state_ && mojom_state_->install_time.has_value()) {
    return mojom_state_->install_time.value();
  }
  return base::Time();
}

bool AppUpdate::InstallTimeChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(install_time)

  return mojom_delta_ && mojom_delta_->install_time.has_value() &&
         (!mojom_state_ ||
          (mojom_delta_->install_time != mojom_state_->install_time));
}

apps::Permissions AppUpdate::Permissions() const {
  if (ShouldUseNonMojom()) {
    if (delta_ && !delta_->permissions.empty()) {
      return ClonePermissions(delta_->permissions);
    } else if (state_ && !state_->permissions.empty()) {
      return ClonePermissions(state_->permissions);
    }
    return std::vector<PermissionPtr>{};
  }

  if (mojom_delta_ && !mojom_delta_->permissions.empty()) {
    return ::ConvertMojomPermissionsToPermissions(mojom_delta_->permissions);
  } else if (mojom_state_ && !mojom_state_->permissions.empty()) {
    return ::ConvertMojomPermissionsToPermissions(mojom_state_->permissions);
  }
  return std::vector<PermissionPtr>{};
}

bool AppUpdate::PermissionsChanged() const {
  if (ShouldUseNonMojom()) {
    return delta_ && !delta_->permissions.empty() &&
           (!state_ || !IsEqual(delta_->permissions, state_->permissions));
  }

  return mojom_delta_ && !mojom_delta_->permissions.empty() &&
         (!mojom_state_ ||
          (mojom_delta_->permissions != mojom_state_->permissions));
}

apps::InstallReason AppUpdate::InstallReason() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_DEFAULT_VALUE(install_reason, InstallReason::kUnknown)
  }

  if (mojom_delta_ &&
      (mojom_delta_->install_reason != apps::mojom::InstallReason::kUnknown)) {
    return ConvertMojomInstallReasonToInstallReason(
        mojom_delta_->install_reason);
  }
  if (mojom_state_) {
    return ConvertMojomInstallReasonToInstallReason(
        mojom_state_->install_reason);
  }
  return apps::InstallReason::kUnknown;
}

bool AppUpdate::InstallReasonChanged() const {
  if (ShouldUseNonMojom()) {
    IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(install_reason, InstallReason::kUnknown)
  }

  return mojom_delta_ &&
         (mojom_delta_->install_reason !=
          apps::mojom::InstallReason::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->install_reason != mojom_state_->install_reason));
}

apps::InstallSource AppUpdate::InstallSource() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_DEFAULT_VALUE(install_source, InstallSource::kUnknown)
  }

  if (mojom_delta_ &&
      (mojom_delta_->install_source != apps::mojom::InstallSource::kUnknown)) {
    return ConvertMojomInstallSourceToInstallSource(
        mojom_delta_->install_source);
  }
  if (mojom_state_) {
    return ConvertMojomInstallSourceToInstallSource(
        mojom_state_->install_source);
  }
  return apps::InstallSource::kUnknown;
}

bool AppUpdate::InstallSourceChanged() const {
  if (ShouldUseNonMojom()) {
    IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(install_source, InstallSource::kUnknown)
  }

  return mojom_delta_ &&
         (mojom_delta_->install_source !=
          apps::mojom::InstallSource::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->install_source != mojom_state_->install_source));
}

const std::string& AppUpdate::PolicyId() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(policy_id, base::EmptyString())
  }

  if (mojom_delta_ && mojom_delta_->policy_id.has_value()) {
    return mojom_delta_->policy_id.value();
  }
  if (mojom_state_ && mojom_state_->policy_id.has_value()) {
    return mojom_state_->policy_id.value();
  }
  return base::EmptyString();
}

bool AppUpdate::PolicyIdChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(policy_id)

  return mojom_delta_ && mojom_delta_->policy_id.has_value() &&
         (!mojom_state_ ||
          (mojom_delta_->policy_id != mojom_state_->policy_id));
}

bool AppUpdate::InstalledInternally() const {
  switch (InstallReason()) {
    case apps::InstallReason::kSystem:
    case apps::InstallReason::kPolicy:
    case apps::InstallReason::kOem:
    case apps::InstallReason::kDefault:
      return true;
    case apps::InstallReason::kUnknown:
    case apps::InstallReason::kSync:
    case apps::InstallReason::kUser:
    case apps::InstallReason::kSubApp:
      return false;
  }
}

absl::optional<bool> AppUpdate::IsPlatformApp() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(is_platform_app, absl::nullopt)
  }

  CONVERT_MOJOM_OPTIONALBOOL_TO_OPTIONAL_VALUE(is_platform_app)
}

bool AppUpdate::IsPlatformAppChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(is_platform_app)

  return mojom_delta_ &&
         (mojom_delta_->is_platform_app !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->is_platform_app != mojom_state_->is_platform_app));
}

absl::optional<bool> AppUpdate::Recommendable() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(recommendable, absl::nullopt)
  }

  CONVERT_MOJOM_OPTIONALBOOL_TO_OPTIONAL_VALUE(recommendable)
}

bool AppUpdate::RecommendableChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(recommendable)

  return mojom_delta_ &&
         (mojom_delta_->recommendable != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->recommendable != mojom_state_->recommendable));
}

absl::optional<bool> AppUpdate::Searchable() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(searchable, absl::nullopt)
  }

  CONVERT_MOJOM_OPTIONALBOOL_TO_OPTIONAL_VALUE(searchable)
}

bool AppUpdate::SearchableChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(searchable)

  return mojom_delta_ &&
         (mojom_delta_->searchable != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->searchable != mojom_state_->searchable));
}

absl::optional<bool> AppUpdate::ShowInLauncher() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(show_in_launcher, absl::nullopt)
  }

  CONVERT_MOJOM_OPTIONALBOOL_TO_OPTIONAL_VALUE(show_in_launcher)
}

bool AppUpdate::ShowInLauncherChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(show_in_launcher)

  return mojom_delta_ &&
         (mojom_delta_->show_in_launcher !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->show_in_launcher != mojom_state_->show_in_launcher));
}

absl::optional<bool> AppUpdate::ShowInShelf() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(show_in_shelf, absl::nullopt)
  }

  CONVERT_MOJOM_OPTIONALBOOL_TO_OPTIONAL_VALUE(show_in_shelf)
}

bool AppUpdate::ShowInShelfChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(show_in_shelf)

  return mojom_delta_ &&
         (mojom_delta_->show_in_shelf != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->show_in_shelf != mojom_state_->show_in_shelf));
}

absl::optional<bool> AppUpdate::ShowInSearch() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(show_in_search, absl::nullopt)
  }

  CONVERT_MOJOM_OPTIONALBOOL_TO_OPTIONAL_VALUE(show_in_search)
}

bool AppUpdate::ShowInSearchChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(show_in_search)

  return mojom_delta_ &&
         (mojom_delta_->show_in_search !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->show_in_search != mojom_state_->show_in_search));
}

absl::optional<bool> AppUpdate::ShowInManagement() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(show_in_management, absl::nullopt)
  }

  CONVERT_MOJOM_OPTIONALBOOL_TO_OPTIONAL_VALUE(show_in_management)
}

bool AppUpdate::ShowInManagementChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(show_in_management)

  return mojom_delta_ &&
         (mojom_delta_->show_in_management !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ || (mojom_delta_->show_in_management !=
                            mojom_state_->show_in_management));
}

absl::optional<bool> AppUpdate::HandlesIntents() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(handles_intents, absl::nullopt)
  }

  CONVERT_MOJOM_OPTIONALBOOL_TO_OPTIONAL_VALUE(handles_intents)
}

bool AppUpdate::HandlesIntentsChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(handles_intents)

  return mojom_delta_ &&
         (mojom_delta_->handles_intents !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->handles_intents != mojom_state_->handles_intents));
}

absl::optional<bool> AppUpdate::AllowUninstall() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(allow_uninstall, absl::nullopt)
  }

  CONVERT_MOJOM_OPTIONALBOOL_TO_OPTIONAL_VALUE(allow_uninstall)
}

bool AppUpdate::AllowUninstallChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(allow_uninstall)

  return mojom_delta_ &&
         (mojom_delta_->allow_uninstall !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->allow_uninstall != mojom_state_->allow_uninstall));
}

absl::optional<bool> AppUpdate::HasBadge() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(has_badge, absl::nullopt)
  }

  CONVERT_MOJOM_OPTIONALBOOL_TO_OPTIONAL_VALUE(has_badge)
}

bool AppUpdate::HasBadgeChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(has_badge)

  return mojom_delta_ &&
         (mojom_delta_->has_badge != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->has_badge != mojom_state_->has_badge));
}

absl::optional<bool> AppUpdate::Paused() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(paused, absl::nullopt);
  }

  CONVERT_MOJOM_OPTIONALBOOL_TO_OPTIONAL_VALUE(paused)
}

bool AppUpdate::PausedChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(paused)

  return mojom_delta_ &&
         (mojom_delta_->paused != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ || (mojom_delta_->paused != mojom_state_->paused));
}

apps::IntentFilters AppUpdate::IntentFilters() const {
  if (ShouldUseNonMojom()) {
    if (delta_ && !delta_->intent_filters.empty()) {
      return CloneIntentFilters(delta_->intent_filters);
    }
    if (state_ && !state_->intent_filters.empty()) {
      return CloneIntentFilters(state_->intent_filters);
    }
    return std::vector<IntentFilterPtr>{};
  }

  if (mojom_delta_ && !mojom_delta_->intent_filters.empty()) {
    return ::ConvertMojomIntentFiltersToIntentFilters(
        mojom_delta_->intent_filters);
  } else if (mojom_state_ && !mojom_state_->intent_filters.empty()) {
    return ::ConvertMojomIntentFiltersToIntentFilters(
        mojom_state_->intent_filters);
  }
  return std::vector<IntentFilterPtr>{};
}

bool AppUpdate::IntentFiltersChanged() const {
  if (ShouldUseNonMojom()) {
    return delta_ && !delta_->intent_filters.empty() &&
           (!state_ ||
            !IsEqual(delta_->intent_filters, state_->intent_filters));
  }

  return mojom_delta_ && !mojom_delta_->intent_filters.empty() &&
         (!mojom_state_ ||
          (mojom_delta_->intent_filters != mojom_state_->intent_filters));
}

absl::optional<bool> AppUpdate::ResizeLocked() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_FALLBACK(resize_locked, absl::nullopt);
  }

  CONVERT_MOJOM_OPTIONALBOOL_TO_OPTIONAL_VALUE(resize_locked)
}

bool AppUpdate::ResizeLockedChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(resize_locked)

  return mojom_delta_ &&
         (mojom_delta_->resize_locked != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->resize_locked != mojom_state_->resize_locked));
}

apps::WindowMode AppUpdate::WindowMode() const {
  if (ShouldUseNonMojom()) {
    GET_VALUE_WITH_DEFAULT_VALUE(window_mode, WindowMode::kUnknown)
  }

  if (mojom_delta_ &&
      (mojom_delta_->window_mode != apps::mojom::WindowMode::kUnknown)) {
    return ConvertMojomWindowModeToWindowMode(mojom_delta_->window_mode);
  }
  if (mojom_state_) {
    return ConvertMojomWindowModeToWindowMode(mojom_state_->window_mode);
  }
  return WindowMode::kUnknown;
}

bool AppUpdate::WindowModeChanged() const {
  if (ShouldUseNonMojom()) {
    IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(window_mode, WindowMode::kUnknown)
  }

  return mojom_delta_ &&
         (mojom_delta_->window_mode != apps::mojom::WindowMode::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->window_mode != mojom_state_->window_mode));
}

absl::optional<apps::RunOnOsLogin> AppUpdate::RunOnOsLogin() const {
  if (ShouldUseNonMojom()) {
    if (delta_ && delta_->run_on_os_login.has_value()) {
      return CloneRunOnOsLogin(delta_->run_on_os_login.value());
    }
    if (state_ && state_->run_on_os_login.has_value()) {
      return CloneRunOnOsLogin(state_->run_on_os_login.value());
    }
    return absl::nullopt;
  }

  if (mojom_delta_ && !mojom_delta_->run_on_os_login.is_null()) {
    apps::RunOnOsLogin run_os_login = std::move(
        *ConvertMojomRunOnOsLoginToRunOnOsLogin(mojom_delta_->run_on_os_login));
    return run_os_login;
  }
  if (mojom_state_ && !mojom_state_->run_on_os_login.is_null()) {
    apps::RunOnOsLogin run_os_login = std::move(
        *ConvertMojomRunOnOsLoginToRunOnOsLogin(mojom_state_->run_on_os_login));
    return run_os_login;
  }
  return absl::nullopt;
}

bool AppUpdate::RunOnOsLoginChanged() const {
  MAYBE_RETURN_OPTIONAL_VALUE_CHANGED(run_on_os_login)

  return mojom_delta_ && !mojom_delta_->run_on_os_login.is_null() &&
         (!mojom_state_ ||
          !mojom_delta_->run_on_os_login.Equals(mojom_state_->run_on_os_login));
}

apps::Shortcuts AppUpdate::Shortcuts() const {
  if (ShouldUseNonMojom()) {
    if (delta_ && !delta_->shortcuts.empty()) {
      return CloneShortcuts(delta_->shortcuts);
    } else if (state_ && !state_->shortcuts.empty()) {
      return CloneShortcuts(state_->shortcuts);
    }
  }
  return std::vector<ShortcutPtr>{};
}

bool AppUpdate::ShortcutsChanged() const {
  if (ShouldUseNonMojom()) {
    return delta_ && !delta_->shortcuts.empty() &&
           (!state_ || !IsEqual(delta_->shortcuts, state_->shortcuts));
  }

  // Shortcuts are not implemented in the Mojo interface of the app service.
  return false;
}

const ::AccountId& AppUpdate::AccountId() const {
  return account_id_;
}

bool AppUpdate::ShouldUseNonMojom() const {
  // `state_` or `delta_` being non-null means exclusively non-mojom updates are
  // being sent.
  return state_ || delta_;
}

std::ostream& operator<<(std::ostream& out, const AppUpdate& app) {
  out << "AppType: " << EnumToString(app.AppType()) << std::endl;
  out << "AppId: " << app.AppId() << std::endl;
  out << "Readiness: " << EnumToString(app.Readiness()) << std::endl;
  out << "Name: " << app.Name() << std::endl;
  out << "ShortName: " << app.ShortName() << std::endl;
  out << "PublisherId: " << app.PublisherId() << std::endl;
  out << "Description: " << app.Description() << std::endl;
  out << "Version: " << app.Version() << std::endl;

  out << "AdditionalSearchTerms: ";
  for (const std::string& term : app.AdditionalSearchTerms()) {
    out << term << ", ";
  }
  out << std::endl;

  out << "LastLaunchTime: " << app.LastLaunchTime() << std::endl;
  out << "InstallTime: " << app.InstallTime() << std::endl;

  out << "Permissions:" << std::endl;
  for (const auto& permission : app.Permissions()) {
    out << permission->ToString();
  }

  out << "InstallReason: " << EnumToString(app.InstallReason()) << std::endl;
  out << "InstallSource: " << EnumToString(app.InstallSource()) << std::endl;
  out << "PolicyId: " << app.PolicyId() << std::endl;
  out << "InstalledInternally: " << app.InstalledInternally() << std::endl;
  out << "IsPlatformApp: " << PRINT_OPTIONAL_VALUE(IsPlatformApp) << std::endl;
  out << "Recommendable: " << PRINT_OPTIONAL_VALUE(Recommendable) << std::endl;
  out << "Searchable: " << PRINT_OPTIONAL_VALUE(Searchable) << std::endl;
  out << "ShowInLauncher: " << PRINT_OPTIONAL_VALUE(ShowInLauncher)
      << std::endl;
  out << "ShowInShelf: " << PRINT_OPTIONAL_VALUE(ShowInShelf) << std::endl;
  out << "ShowInSearch: " << PRINT_OPTIONAL_VALUE(ShowInSearch) << std::endl;
  out << "ShowInManagement: " << PRINT_OPTIONAL_VALUE(ShowInManagement)
      << std::endl;
  out << "HandlesIntents: " << PRINT_OPTIONAL_VALUE(HandlesIntents)
      << std::endl;
  out << "AllowUninstall: " << PRINT_OPTIONAL_VALUE(AllowUninstall)
      << std::endl;
  out << "HasBadge: " << PRINT_OPTIONAL_VALUE(HasBadge) << std::endl;
  out << "Paused: " << PRINT_OPTIONAL_VALUE(Paused) << std::endl;

  out << "IntentFilters: " << std::endl;
  for (const auto& filter : app.IntentFilters()) {
    out << filter->ToString() << std::endl;
  }

  out << "ResizeLocked: " << PRINT_OPTIONAL_VALUE(ResizeLocked) << std::endl;
  out << "WindowMode: " << EnumToString(app.WindowMode()) << std::endl;
  if (app.RunOnOsLogin().has_value()) {
    out << "RunOnOsLoginMode: "
        << EnumToString(app.RunOnOsLogin().value().login_mode) << std::endl;
  }

  out << "Shortcuts: " << std::endl;
  for (const auto& shortcut : app.Shortcuts()) {
    out << shortcut->ToString() << std::endl;
  }

  return out;
}

}  // namespace apps
