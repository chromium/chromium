// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_update.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"

namespace {

void ClonePermissions(const std::vector<apps::mojom::PermissionPtr>& clone_from,
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

void CloneIntentFilters(
    const std::vector<apps::mojom::IntentFilterPtr>& clone_from,
    std::vector<apps::mojom::IntentFilterPtr>* clone_to) {
  for (const auto& intent_filter : clone_from) {
    clone_to->push_back(intent_filter->Clone());
  }
}

absl::optional<apps::IconKey> CloneIconKey(const apps::IconKey& icon_key) {
  return apps::IconKey(icon_key.timeline, icon_key.resource_id,
                       icon_key.icon_effects);
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
    DCHECK(state->permissions.empty() ||
           (delta->permissions.size() == state->permissions.size()));
    state->permissions.clear();
    ClonePermissions(delta->permissions, &state->permissions);
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
    CloneIntentFilters(delta->intent_filters, &state->intent_filters);
  }
  if (delta->resize_locked != apps::mojom::OptionalBool::kUnknown) {
    state->resize_locked = delta->resize_locked;
  }
  if (delta->window_mode != apps::mojom::WindowMode::kUnknown) {
    state->window_mode = delta->window_mode;
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

  if (delta->readiness != apps::Readiness::kUnknown) {
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
  if (delta->icon_key.has_value()) {
    state->icon_key = CloneIconKey(delta->icon_key.value());
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
  return mojom_state_ == nullptr;
}

apps::mojom::AppType AppUpdate::AppType() const {
  return mojom_delta_ ? mojom_delta_->app_type : mojom_state_->app_type;
}

apps::AppType AppUpdate::GetAppType() const {
  return delta_ ? delta_->app_type : state_->app_type;
}

const std::string& AppUpdate::AppId() const {
  return mojom_delta_ ? mojom_delta_->app_id : mojom_state_->app_id;
}

const std::string& AppUpdate::GetAppId() const {
  return delta_ ? delta_->app_id : state_->app_id;
}

apps::mojom::Readiness AppUpdate::Readiness() const {
  if (mojom_delta_ &&
      (mojom_delta_->readiness != apps::mojom::Readiness::kUnknown)) {
    return mojom_delta_->readiness;
  }
  if (mojom_state_) {
    return mojom_state_->readiness;
  }
  return apps::mojom::Readiness::kUnknown;
}

apps::mojom::Readiness AppUpdate::PriorReadiness() const {
  return mojom_state_ ? mojom_state_->readiness
                      : apps::mojom::Readiness::kUnknown;
}

apps::Readiness AppUpdate::GetReadiness() const {
  if (delta_ && (delta_->readiness != apps::Readiness::kUnknown)) {
    return delta_->readiness;
  }
  if (state_) {
    return state_->readiness;
  }
  return apps::Readiness::kUnknown;
}

apps::Readiness AppUpdate::GetPriorReadiness() const {
  return state_ ? state_->readiness : apps::Readiness::kUnknown;
}

bool AppUpdate::ReadinessChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->readiness != apps::mojom::Readiness::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->readiness != mojom_state_->readiness));
}

const std::string& AppUpdate::Name() const {
  if (mojom_delta_ && mojom_delta_->name.has_value()) {
    return mojom_delta_->name.value();
  }
  if (mojom_state_ && mojom_state_->name.has_value()) {
    return mojom_state_->name.value();
  }
  return base::EmptyString();
}

const std::string& AppUpdate::GetName() const {
  if (delta_ && delta_->name.has_value()) {
    return delta_->name.value();
  }
  if (state_ && state_->name.has_value()) {
    return state_->name.value();
  }
  return base::EmptyString();
}

bool AppUpdate::NameChanged() const {
  return mojom_delta_ && mojom_delta_->name.has_value() &&
         (!mojom_state_ || (mojom_delta_->name != mojom_state_->name));
}

const std::string& AppUpdate::ShortName() const {
  if (mojom_delta_ && mojom_delta_->short_name.has_value()) {
    return mojom_delta_->short_name.value();
  }
  if (mojom_state_ && mojom_state_->short_name.has_value()) {
    return mojom_state_->short_name.value();
  }
  return base::EmptyString();
}

const std::string& AppUpdate::GetShortName() const {
  if (delta_ && delta_->short_name.has_value()) {
    return delta_->short_name.value();
  }
  if (state_ && state_->short_name.has_value()) {
    return state_->short_name.value();
  }
  return base::EmptyString();
}

bool AppUpdate::ShortNameChanged() const {
  return mojom_delta_ && mojom_delta_->short_name.has_value() &&
         (!mojom_state_ ||
          (mojom_delta_->short_name != mojom_state_->short_name));
}

const std::string& AppUpdate::PublisherId() const {
  if (mojom_delta_ && mojom_delta_->publisher_id.has_value()) {
    return mojom_delta_->publisher_id.value();
  }
  if (mojom_state_ && mojom_state_->publisher_id.has_value()) {
    return mojom_state_->publisher_id.value();
  }
  return base::EmptyString();
}

const std::string& AppUpdate::GetPublisherId() const {
  if (delta_ && delta_->publisher_id.has_value()) {
    return delta_->publisher_id.value();
  }
  if (state_ && state_->publisher_id.has_value()) {
    return state_->publisher_id.value();
  }
  return base::EmptyString();
}

bool AppUpdate::PublisherIdChanged() const {
  return mojom_delta_ && mojom_delta_->publisher_id.has_value() &&
         (!mojom_state_ ||
          (mojom_delta_->publisher_id != mojom_state_->publisher_id));
}

const std::string& AppUpdate::Description() const {
  if (mojom_delta_ && mojom_delta_->description.has_value()) {
    return mojom_delta_->description.value();
  }
  if (mojom_state_ && mojom_state_->description.has_value()) {
    return mojom_state_->description.value();
  }
  return base::EmptyString();
}

const std::string& AppUpdate::GetDescription() const {
  if (delta_ && delta_->description.has_value()) {
    return delta_->description.value();
  }
  if (state_ && state_->description.has_value()) {
    return state_->description.value();
  }
  return base::EmptyString();
}

bool AppUpdate::DescriptionChanged() const {
  return mojom_delta_ && mojom_delta_->description.has_value() &&
         (!mojom_state_ ||
          (mojom_delta_->description != mojom_state_->description));
}

const std::string& AppUpdate::Version() const {
  if (mojom_delta_ && mojom_delta_->version.has_value()) {
    return mojom_delta_->version.value();
  }
  if (mojom_state_ && mojom_state_->version.has_value()) {
    return mojom_state_->version.value();
  }
  return base::EmptyString();
}

const std::string& AppUpdate::GetVersion() const {
  if (delta_ && delta_->version.has_value()) {
    return delta_->version.value();
  }
  if (state_ && state_->version.has_value()) {
    return state_->version.value();
  }
  return base::EmptyString();
}

bool AppUpdate::VersionChanged() const {
  return mojom_delta_ && mojom_delta_->version.has_value() &&
         (!mojom_state_ || (mojom_delta_->version != mojom_state_->version));
}

std::vector<std::string> AppUpdate::AdditionalSearchTerms() const {
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
  return mojom_delta_ && !mojom_delta_->additional_search_terms.empty() &&
         (!mojom_state_ || (mojom_delta_->additional_search_terms !=
                            mojom_state_->additional_search_terms));
}

apps::mojom::IconKeyPtr AppUpdate::IconKey() const {
  if (mojom_delta_ && !mojom_delta_->icon_key.is_null()) {
    return mojom_delta_->icon_key.Clone();
  }
  if (mojom_state_ && !mojom_state_->icon_key.is_null()) {
    return mojom_state_->icon_key.Clone();
  }
  return apps::mojom::IconKeyPtr();
}

absl::optional<apps::IconKey> AppUpdate::GetIconKey() const {
  if (delta_ && delta_->icon_key.has_value()) {
    return CloneIconKey(delta_->icon_key.value());
  }
  if (state_ && state_->icon_key.has_value()) {
    return CloneIconKey(state_->icon_key.value());
  }
  return absl::nullopt;
}

bool AppUpdate::IconKeyChanged() const {
  return mojom_delta_ && !mojom_delta_->icon_key.is_null() &&
         (!mojom_state_ ||
          !mojom_delta_->icon_key.Equals(mojom_state_->icon_key));
}

base::Time AppUpdate::LastLaunchTime() const {
  if (mojom_delta_ && mojom_delta_->last_launch_time.has_value()) {
    return mojom_delta_->last_launch_time.value();
  }
  if (mojom_state_ && mojom_state_->last_launch_time.has_value()) {
    return mojom_state_->last_launch_time.value();
  }
  return base::Time();
}

bool AppUpdate::LastLaunchTimeChanged() const {
  return mojom_delta_ && mojom_delta_->last_launch_time.has_value() &&
         (!mojom_state_ ||
          (mojom_delta_->last_launch_time != mojom_state_->last_launch_time));
}

base::Time AppUpdate::InstallTime() const {
  if (mojom_delta_ && mojom_delta_->install_time.has_value()) {
    return mojom_delta_->install_time.value();
  }
  if (mojom_state_ && mojom_state_->install_time.has_value()) {
    return mojom_state_->install_time.value();
  }
  return base::Time();
}

bool AppUpdate::InstallTimeChanged() const {
  return mojom_delta_ && mojom_delta_->install_time.has_value() &&
         (!mojom_state_ ||
          (mojom_delta_->install_time != mojom_state_->install_time));
}

std::vector<apps::mojom::PermissionPtr> AppUpdate::Permissions() const {
  std::vector<apps::mojom::PermissionPtr> permissions;

  if (mojom_delta_ && !mojom_delta_->permissions.empty()) {
    ClonePermissions(mojom_delta_->permissions, &permissions);
  } else if (mojom_state_ && !mojom_state_->permissions.empty()) {
    ClonePermissions(mojom_state_->permissions, &permissions);
  }

  return permissions;
}

bool AppUpdate::PermissionsChanged() const {
  return mojom_delta_ && !mojom_delta_->permissions.empty() &&
         (!mojom_state_ ||
          (mojom_delta_->permissions != mojom_state_->permissions));
}

apps::mojom::InstallReason AppUpdate::InstallReason() const {
  if (mojom_delta_ &&
      (mojom_delta_->install_reason != apps::mojom::InstallReason::kUnknown)) {
    return mojom_delta_->install_reason;
  }
  if (mojom_state_) {
    return mojom_state_->install_reason;
  }
  return apps::mojom::InstallReason::kUnknown;
}

bool AppUpdate::InstallReasonChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->install_reason !=
          apps::mojom::InstallReason::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->install_reason != mojom_state_->install_reason));
}

apps::mojom::InstallSource AppUpdate::InstallSource() const {
  if (mojom_delta_ &&
      (mojom_delta_->install_source != apps::mojom::InstallSource::kUnknown)) {
    return mojom_delta_->install_source;
  }
  if (mojom_state_) {
    return mojom_state_->install_source;
  }
  return apps::mojom::InstallSource::kUnknown;
}

bool AppUpdate::InstallSourceChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->install_source !=
          apps::mojom::InstallSource::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->install_source != mojom_state_->install_source));
}

const std::string& AppUpdate::PolicyId() const {
  if (mojom_delta_ && mojom_delta_->policy_id.has_value()) {
    return mojom_delta_->policy_id.value();
  }
  if (mojom_state_ && mojom_state_->policy_id.has_value()) {
    return mojom_state_->policy_id.value();
  }
  return base::EmptyString();
}

bool AppUpdate::PolicyIdChanged() const {
  return mojom_delta_ && mojom_delta_->policy_id.has_value() &&
         (!mojom_state_ ||
          (mojom_delta_->policy_id != mojom_state_->policy_id));
}

apps::mojom::OptionalBool AppUpdate::InstalledInternally() const {
  switch (InstallReason()) {
    case apps::mojom::InstallReason::kUnknown:
      return apps::mojom::OptionalBool::kUnknown;
    case apps::mojom::InstallReason::kSystem:
    case apps::mojom::InstallReason::kPolicy:
    case apps::mojom::InstallReason::kOem:
    case apps::mojom::InstallReason::kDefault:
      return apps::mojom::OptionalBool::kTrue;
    default:
      return apps::mojom::OptionalBool::kFalse;
  }
}

apps::mojom::OptionalBool AppUpdate::IsPlatformApp() const {
  if (mojom_delta_ &&
      (mojom_delta_->is_platform_app != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->is_platform_app;
  }
  if (mojom_state_) {
    return mojom_state_->is_platform_app;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::IsPlatformAppChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->is_platform_app !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->is_platform_app != mojom_state_->is_platform_app));
}

apps::mojom::OptionalBool AppUpdate::Recommendable() const {
  if (mojom_delta_ &&
      (mojom_delta_->recommendable != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->recommendable;
  }
  if (mojom_state_) {
    return mojom_state_->recommendable;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::RecommendableChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->recommendable != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->recommendable != mojom_state_->recommendable));
}

apps::mojom::OptionalBool AppUpdate::Searchable() const {
  if (mojom_delta_ &&
      (mojom_delta_->searchable != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->searchable;
  }
  if (mojom_state_) {
    return mojom_state_->searchable;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::SearchableChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->searchable != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->searchable != mojom_state_->searchable));
}

apps::mojom::OptionalBool AppUpdate::ShowInLauncher() const {
  if (mojom_delta_ &&
      (mojom_delta_->show_in_launcher != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->show_in_launcher;
  }
  if (mojom_state_) {
    return mojom_state_->show_in_launcher;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::ShowInLauncherChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->show_in_launcher !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->show_in_launcher != mojom_state_->show_in_launcher));
}

apps::mojom::OptionalBool AppUpdate::ShowInShelf() const {
  if (mojom_delta_ &&
      (mojom_delta_->show_in_shelf != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->show_in_shelf;
  }
  if (mojom_state_) {
    return mojom_state_->show_in_shelf;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::ShowInShelfChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->show_in_shelf != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->show_in_shelf != mojom_state_->show_in_shelf));
}

apps::mojom::OptionalBool AppUpdate::ShowInSearch() const {
  if (mojom_delta_ &&
      (mojom_delta_->show_in_search != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->show_in_search;
  }
  if (mojom_state_) {
    return mojom_state_->show_in_search;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::ShowInSearchChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->show_in_search !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->show_in_search != mojom_state_->show_in_search));
}

apps::mojom::OptionalBool AppUpdate::ShowInManagement() const {
  if (mojom_delta_ && (mojom_delta_->show_in_management !=
                       apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->show_in_management;
  }
  if (mojom_state_) {
    return mojom_state_->show_in_management;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::ShowInManagementChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->show_in_management !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ || (mojom_delta_->show_in_management !=
                            mojom_state_->show_in_management));
}

apps::mojom::OptionalBool AppUpdate::HandlesIntents() const {
  if (mojom_delta_ &&
      (mojom_delta_->handles_intents != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->handles_intents;
  }
  if (mojom_state_) {
    return mojom_state_->handles_intents;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::HandlesIntentsChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->handles_intents !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->handles_intents != mojom_state_->handles_intents));
}

apps::mojom::OptionalBool AppUpdate::AllowUninstall() const {
  if (mojom_delta_ &&
      (mojom_delta_->allow_uninstall != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->allow_uninstall;
  }
  if (mojom_state_) {
    return mojom_state_->allow_uninstall;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::AllowUninstallChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->allow_uninstall !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->allow_uninstall != mojom_state_->allow_uninstall));
}

apps::mojom::OptionalBool AppUpdate::HasBadge() const {
  if (mojom_delta_ &&
      (mojom_delta_->has_badge != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->has_badge;
  }
  if (mojom_state_) {
    return mojom_state_->has_badge;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::HasBadgeChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->has_badge != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->has_badge != mojom_state_->has_badge));
}

apps::mojom::OptionalBool AppUpdate::Paused() const {
  if (mojom_delta_ &&
      (mojom_delta_->paused != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->paused;
  }
  if (mojom_state_) {
    return mojom_state_->paused;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::PausedChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->paused != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ || (mojom_delta_->paused != mojom_state_->paused));
}

std::vector<apps::mojom::IntentFilterPtr> AppUpdate::IntentFilters() const {
  std::vector<apps::mojom::IntentFilterPtr> intent_filters;

  if (mojom_delta_ && !mojom_delta_->intent_filters.empty()) {
    CloneIntentFilters(mojom_delta_->intent_filters, &intent_filters);
  } else if (mojom_state_ && !mojom_state_->intent_filters.empty()) {
    CloneIntentFilters(mojom_state_->intent_filters, &intent_filters);
  }

  return intent_filters;
}

bool AppUpdate::IntentFiltersChanged() const {
  return mojom_delta_ && !mojom_delta_->intent_filters.empty() &&
         (!mojom_state_ ||
          (mojom_delta_->intent_filters != mojom_state_->intent_filters));
}

apps::mojom::OptionalBool AppUpdate::ResizeLocked() const {
  if (mojom_delta_ &&
      (mojom_delta_->resize_locked != apps::mojom::OptionalBool::kUnknown)) {
    return mojom_delta_->resize_locked;
  }
  if (mojom_state_)
    return mojom_state_->resize_locked;
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::ResizeLockedChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->resize_locked != apps::mojom::OptionalBool::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->resize_locked != mojom_state_->resize_locked));
}

apps::mojom::WindowMode AppUpdate::WindowMode() const {
  if (mojom_delta_ &&
      (mojom_delta_->window_mode != apps::mojom::WindowMode::kUnknown)) {
    return mojom_delta_->window_mode;
  }
  if (mojom_state_) {
    return mojom_state_->window_mode;
  }
  return apps::mojom::WindowMode::kUnknown;
}

bool AppUpdate::WindowModeChanged() const {
  return mojom_delta_ &&
         (mojom_delta_->window_mode != apps::mojom::WindowMode::kUnknown) &&
         (!mojom_state_ ||
          (mojom_delta_->window_mode != mojom_state_->window_mode));
}

const ::AccountId& AppUpdate::AccountId() const {
  return account_id_;
}

std::ostream& operator<<(std::ostream& out, const AppUpdate& app) {
  out << "AppType: " << app.AppType() << std::endl;
  out << "AppId: " << app.AppId() << std::endl;
  out << "Readiness: " << app.Readiness() << std::endl;
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
    out << "  ID: " << permission->permission_type;
    out << " value: " << std::endl;
    if (permission->value->is_bool_value()) {
      out << " bool_value: " << permission->value->get_bool_value();
    }
    if (permission->value->is_tristate_value()) {
      out << " tristate_value: " << permission->value->get_tristate_value();
    }
    out << " is_managed: " << permission->is_managed << std::endl;
  }

  out << "InstallReason: " << app.InstallReason() << std::endl;
  out << "PolicyId: " << app.PolicyId() << std::endl;
  out << "InstalledInternally: " << app.InstalledInternally() << std::endl;
  out << "IsPlatformApp: " << app.IsPlatformApp() << std::endl;
  out << "Recommendable: " << app.Recommendable() << std::endl;
  out << "Searchable: " << app.Searchable() << std::endl;
  out << "ShowInLauncher: " << app.ShowInLauncher() << std::endl;
  out << "ShowInShelf: " << app.ShowInShelf() << std::endl;
  out << "ShowInSearch: " << app.ShowInSearch() << std::endl;
  out << "ShowInManagement: " << app.ShowInManagement() << std::endl;
  out << "HandlesIntents: " << app.HandlesIntents() << std::endl;
  out << "AllowUninstall: " << app.AllowUninstall() << std::endl;
  out << "HasBadge: " << app.HasBadge() << std::endl;
  out << "Paused: " << app.Paused() << std::endl;
  out << "IntentFilters: " << std::endl;
  for (const auto& filter : app.IntentFilters()) {
    out << filter << std::endl;
  }

  out << "ResizeLocked: " << app.ResizeLocked() << std::endl;
  out << "WindowMode: " << app.WindowMode() << std::endl;

  return out;
}

}  // namespace apps
