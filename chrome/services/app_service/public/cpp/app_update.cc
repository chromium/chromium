// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/app_update.h"

#include "base/logging.h"
#include "base/time/time.h"

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
  if (delta->install_source != apps::mojom::InstallSource::kUnknown) {
    state->install_source = delta->install_source;
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
  if (delta->show_in_search != apps::mojom::OptionalBool::kUnknown) {
    state->show_in_search = delta->show_in_search;
  }
  if (delta->show_in_management != apps::mojom::OptionalBool::kUnknown) {
    state->show_in_management = delta->show_in_management;
  }
  if (delta->paused != apps::mojom::OptionalBool::kUnknown) {
    state->paused = delta->paused;
  }

  if (!delta->intent_filters.empty()) {
    state->intent_filters.clear();
    CloneIntentFilters(delta->intent_filters, &state->intent_filters);
  }

  // When adding new fields to the App Mojo type, this function should also be
  // updated.
}

AppUpdate::AppUpdate(const apps::mojom::App* state,
                     const apps::mojom::App* delta)
    : state_(state), delta_(delta) {
  DCHECK(state_ || delta_);
  if (state_ && delta_) {
    DCHECK(state_->app_type == delta->app_type);
    DCHECK(state_->app_id == delta->app_id);
  }
}

bool AppUpdate::StateIsNull() const {
  return state_ == nullptr;
}

apps::mojom::AppType AppUpdate::AppType() const {
  return delta_ ? delta_->app_type : state_->app_type;
}

const std::string& AppUpdate::AppId() const {
  return delta_ ? delta_->app_id : state_->app_id;
}

apps::mojom::Readiness AppUpdate::Readiness() const {
  if (delta_ && (delta_->readiness != apps::mojom::Readiness::kUnknown)) {
    return delta_->readiness;
  }
  if (state_) {
    return state_->readiness;
  }
  return apps::mojom::Readiness::kUnknown;
}

bool AppUpdate::ReadinessChanged() const {
  return delta_ && (delta_->readiness != apps::mojom::Readiness::kUnknown) &&
         (!state_ || (delta_->readiness != state_->readiness));
}

const std::string& AppUpdate::Name() const {
  if (delta_ && delta_->name.has_value()) {
    return delta_->name.value();
  }
  if (state_ && state_->name.has_value()) {
    return state_->name.value();
  }
  return base::EmptyString();
}

bool AppUpdate::NameChanged() const {
  return delta_ && delta_->name.has_value() &&
         (!state_ || (delta_->name != state_->name));
}

const std::string& AppUpdate::ShortName() const {
  if (delta_ && delta_->short_name.has_value()) {
    return delta_->short_name.value();
  }
  if (state_ && state_->short_name.has_value()) {
    return state_->short_name.value();
  }
  return base::EmptyString();
}

bool AppUpdate::ShortNameChanged() const {
  return delta_ && delta_->short_name.has_value() &&
         (!state_ || (delta_->short_name != state_->short_name));
}

const std::string& AppUpdate::PublisherId() const {
  if (delta_ && delta_->publisher_id.has_value()) {
    return delta_->publisher_id.value();
  }
  if (state_ && state_->publisher_id.has_value()) {
    return state_->publisher_id.value();
  }
  return base::EmptyString();
}

bool AppUpdate::PublisherIdChanged() const {
  return delta_ && delta_->publisher_id.has_value() &&
         (!state_ || (delta_->publisher_id != state_->publisher_id));
}

const std::string& AppUpdate::Description() const {
  if (delta_ && delta_->description.has_value()) {
    return delta_->description.value();
  }
  if (state_ && state_->description.has_value()) {
    return state_->description.value();
  }
  return base::EmptyString();
}

bool AppUpdate::DescriptionChanged() const {
  return delta_ && delta_->description.has_value() &&
         (!state_ || (delta_->description != state_->description));
}

const std::string& AppUpdate::Version() const {
  if (delta_ && delta_->version.has_value()) {
    return delta_->version.value();
  }
  if (state_ && state_->version.has_value()) {
    return state_->version.value();
  }
  return base::EmptyString();
}

bool AppUpdate::VersionChanged() const {
  return delta_ && delta_->version.has_value() &&
         (!state_ || (delta_->version != state_->version));
}

std::vector<std::string> AppUpdate::AdditionalSearchTerms() const {
  std::vector<std::string> additional_search_terms;

  if (delta_ && !delta_->additional_search_terms.empty()) {
    CloneStrings(delta_->additional_search_terms, &additional_search_terms);
  } else if (state_ && !state_->additional_search_terms.empty()) {
    CloneStrings(state_->additional_search_terms, &additional_search_terms);
  }

  return additional_search_terms;
}

bool AppUpdate::AdditionalSearchTermsChanged() const {
  return delta_ && !delta_->additional_search_terms.empty() &&
         (!state_ ||
          (delta_->additional_search_terms != state_->additional_search_terms));
}

apps::mojom::IconKeyPtr AppUpdate::IconKey() const {
  if (delta_ && !delta_->icon_key.is_null()) {
    return delta_->icon_key.Clone();
  }
  if (state_ && !state_->icon_key.is_null()) {
    return state_->icon_key.Clone();
  }
  return apps::mojom::IconKeyPtr();
}

bool AppUpdate::IconKeyChanged() const {
  return delta_ && !delta_->icon_key.is_null() &&
         (!state_ || !delta_->icon_key.Equals(state_->icon_key));
}

base::Time AppUpdate::LastLaunchTime() const {
  if (delta_ && delta_->last_launch_time.has_value()) {
    return delta_->last_launch_time.value();
  }
  if (state_ && state_->last_launch_time.has_value()) {
    return state_->last_launch_time.value();
  }
  return base::Time();
}

bool AppUpdate::LastLaunchTimeChanged() const {
  return delta_ && delta_->last_launch_time.has_value() &&
         (!state_ || (delta_->last_launch_time != state_->last_launch_time));
}

base::Time AppUpdate::InstallTime() const {
  if (delta_ && delta_->install_time.has_value()) {
    return delta_->install_time.value();
  }
  if (state_ && state_->install_time.has_value()) {
    return state_->install_time.value();
  }
  return base::Time();
}

bool AppUpdate::InstallTimeChanged() const {
  return delta_ && delta_->install_time.has_value() &&
         (!state_ || (delta_->install_time != state_->install_time));
}

std::vector<apps::mojom::PermissionPtr> AppUpdate::Permissions() const {
  std::vector<apps::mojom::PermissionPtr> permissions;

  if (delta_ && !delta_->permissions.empty()) {
    ClonePermissions(delta_->permissions, &permissions);
  } else if (state_ && !state_->permissions.empty()) {
    ClonePermissions(state_->permissions, &permissions);
  }

  return permissions;
}

bool AppUpdate::PermissionsChanged() const {
  return delta_ && !delta_->permissions.empty() &&
         (!state_ || (delta_->permissions != state_->permissions));
}

apps::mojom::InstallSource AppUpdate::InstallSource() const {
  if (delta_ &&
      (delta_->install_source != apps::mojom::InstallSource::kUnknown)) {
    return delta_->install_source;
  }
  if (state_) {
    return state_->install_source;
  }
  return apps::mojom::InstallSource::kUnknown;
}

bool AppUpdate::InstallSourceChanged() const {
  return delta_ &&
         (delta_->install_source != apps::mojom::InstallSource::kUnknown) &&
         (!state_ || (delta_->install_source != state_->install_source));
}

apps::mojom::OptionalBool AppUpdate::InstalledInternally() const {
  switch (InstallSource()) {
    case apps::mojom::InstallSource::kUnknown:
      return apps::mojom::OptionalBool::kUnknown;
    case apps::mojom::InstallSource::kSystem:
    case apps::mojom::InstallSource::kPolicy:
    case apps::mojom::InstallSource::kOem:
    case apps::mojom::InstallSource::kDefault:
      return apps::mojom::OptionalBool::kTrue;
    default:
      return apps::mojom::OptionalBool::kFalse;
  }
}

apps::mojom::OptionalBool AppUpdate::IsPlatformApp() const {
  if (delta_ &&
      (delta_->is_platform_app != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->is_platform_app;
  }
  if (state_) {
    return state_->is_platform_app;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::IsPlatformAppChanged() const {
  return delta_ &&
         (delta_->is_platform_app != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ || (delta_->is_platform_app != state_->is_platform_app));
}

apps::mojom::OptionalBool AppUpdate::Recommendable() const {
  if (delta_ &&
      (delta_->recommendable != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->recommendable;
  }
  if (state_) {
    return state_->recommendable;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::RecommendableChanged() const {
  return delta_ &&
         (delta_->recommendable != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ || (delta_->recommendable != state_->recommendable));
}

apps::mojom::OptionalBool AppUpdate::Searchable() const {
  if (delta_ && (delta_->searchable != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->searchable;
  }
  if (state_) {
    return state_->searchable;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::SearchableChanged() const {
  return delta_ &&
         (delta_->searchable != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ || (delta_->searchable != state_->searchable));
}

apps::mojom::OptionalBool AppUpdate::ShowInLauncher() const {
  if (delta_ &&
      (delta_->show_in_launcher != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->show_in_launcher;
  }
  if (state_) {
    return state_->show_in_launcher;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::ShowInLauncherChanged() const {
  return delta_ &&
         (delta_->show_in_launcher != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ || (delta_->show_in_launcher != state_->show_in_launcher));
}

apps::mojom::OptionalBool AppUpdate::ShowInSearch() const {
  if (delta_ &&
      (delta_->show_in_search != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->show_in_search;
  }
  if (state_) {
    return state_->show_in_search;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::ShowInSearchChanged() const {
  return delta_ &&
         (delta_->show_in_search != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ || (delta_->show_in_search != state_->show_in_search));
}

apps::mojom::OptionalBool AppUpdate::ShowInManagement() const {
  if (delta_ &&
      (delta_->show_in_management != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->show_in_management;
  }
  if (state_) {
    return state_->show_in_management;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::ShowInManagementChanged() const {
  return delta_ &&
         (delta_->show_in_management != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ ||
          (delta_->show_in_management != state_->show_in_management));
}

apps::mojom::OptionalBool AppUpdate::Paused() const {
  if (delta_ && (delta_->paused != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->paused;
  }
  if (state_) {
    return state_->paused;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::PausedChanged() const {
  return delta_ && (delta_->paused != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ || (delta_->paused != state_->paused));
}

std::vector<apps::mojom::IntentFilterPtr> AppUpdate::IntentFilters() const {
  std::vector<apps::mojom::IntentFilterPtr> intent_filters;

  if (delta_ && !delta_->intent_filters.empty()) {
    CloneIntentFilters(delta_->intent_filters, &intent_filters);
  } else if (state_ && !state_->intent_filters.empty()) {
    CloneIntentFilters(state_->intent_filters, &intent_filters);
  }

  return intent_filters;
}

bool AppUpdate::IntentFiltersChanged() const {
  return delta_ && !delta_->intent_filters.empty() &&
         (!state_ || (delta_->intent_filters != state_->intent_filters));
}

}  // namespace apps
