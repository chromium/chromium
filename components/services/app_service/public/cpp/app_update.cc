// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_update.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/macros.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "components/services/app_service/public/cpp/types_util.h"

namespace apps {

namespace {

// Represents an empty fallback vector. This comes in handy when both delta_ and
// state_ in AppUpdate are null, so instead of returning a prohibited temporary
// reference in AppUpdate::AdditionalSearchTerms / AppUpdate::PolicyIds() we
// respond with this object.
static const std::vector<std::string>& EmptyStringVector() {
  static const std::vector<std::string> g_empty_string_vector;
  return g_empty_string_vector;
}

absl::optional<apps::RunOnOsLogin> CloneRunOnOsLogin(
    const apps::RunOnOsLogin& login_data) {
  return apps::RunOnOsLogin(login_data.login_mode, login_data.is_managed);
}

std::string FormatBytes(absl::optional<uint64_t> bytes) {
  return bytes.has_value() ? base::NumberToString(bytes.value()) : "null";
}

// Merges `delta`'s `icon_key` to `new_delta`'s `icon_key`.
void MergeIconKeyDelta(App* new_delta, App* delta) {
  CHECK(new_delta);

  // `new_delta` should hold a bool icon version only.
  CHECK(!new_delta->icon_key.has_value() ||
        absl::holds_alternative<bool>(new_delta->icon_key->update_version));

  // `delta` should hold a bool icon version only.
  CHECK(!delta || !delta->icon_key.has_value() ||
        absl::holds_alternative<bool>(delta->icon_key->update_version));

  if (delta && delta->readiness != Readiness::kUnknown &&
      !apps_util::IsInstalled(delta->readiness)) {
    // When the app is uninstalled, reset `icon_key` to clear the icon key, to
    // refresh the icon for AppService clients, and reload the icon when the app
    // is installed back again.
    new_delta->icon_key.reset();
    return;
  }

  if (!delta || !delta->icon_key.has_value()) {
    return;
  }

  if (new_delta->icon_key.has_value()) {
    // If `new_delta`'s `update_version` is true, or `delta`'s `update_version`
    // is true, the new `update_version` should be true.
    delta->icon_key->update_version =
        absl::get<bool>(new_delta->icon_key->update_version) ||
        absl::get<bool>(delta->icon_key->update_version);
  }

  new_delta->icon_key = std::move(delta->icon_key);
  return;
}

// Merges `delta`'s `icon_key` to `state`'s `icon_key`, and  returns's the
// merge result.
//
// For `icon_key`, if `delta`'s `update_version` is true, increase `state`'s
// `update_version`.
absl::optional<apps::IconKey> MergeIconKey(const App* state, const App* delta) {
  //`state` should have int32_t `update_version` only.
  CHECK(!state || !state->icon_key.has_value() ||
        absl::holds_alternative<int32_t>(state->icon_key->update_version));

  // `delta` should hold a bool icon version only.
  CHECK(!delta || !delta->icon_key.has_value() ||
        absl::holds_alternative<bool>(delta->icon_key->update_version));

  if (delta && delta->readiness != Readiness::kUnknown &&
      !apps_util::IsInstalled(delta->readiness)) {
    // When the app is uninstalled, reset `icon_key` to clear the icon key, to
    // refresh the icon for AppService clients, and reload the icon when the app
    // is installed back again.
    IconKey icon_key;
    icon_key.update_version = IconKey::kInvalidVersion;
    return icon_key;
  }

  return MergeIconKey(
      state && state->icon_key.has_value() ? &state->icon_key.value() : nullptr,
      delta && delta->icon_key.has_value() ? &delta->icon_key.value()
                                           : nullptr);
}

bool MergeWithoutIconKey(App* state, const App* delta) {
  CHECK(state);

  if (!delta) {
    return false;
  }

  if ((delta->app_type != state->app_type) ||
      (delta->app_id != state->app_id)) {
    LOG(ERROR) << "inconsistent (app_type, app_id): ("
               << EnumToString(delta->app_type) << ", " << delta->app_id
               << ") vs (" << EnumToString(state->app_type) << ", "
               << state->app_id << ") ";
    return false;
  }

  // You can not merge removed states.
  DCHECK_NE(state->readiness, Readiness::kRemoved);
  DCHECK_NE(delta->readiness, Readiness::kRemoved);

  SET_ENUM_VALUE(readiness, Readiness::kUnknown);
  SET_OPTIONAL_VALUE(name)
  SET_OPTIONAL_VALUE(short_name)
  SET_OPTIONAL_VALUE(publisher_id)
  SET_OPTIONAL_VALUE(description)
  SET_OPTIONAL_VALUE(version)

  if (!delta->additional_search_terms.empty()) {
    state->additional_search_terms.clear();
    state->additional_search_terms = delta->additional_search_terms;
  }

  SET_OPTIONAL_VALUE(last_launch_time);
  SET_OPTIONAL_VALUE(install_time);

  if (!delta->permissions.empty()) {
    state->permissions.clear();
    state->permissions = ClonePermissions(delta->permissions);
  }

  SET_ENUM_VALUE(install_reason, InstallReason::kUnknown);
  SET_ENUM_VALUE(install_source, InstallSource::kUnknown);
  SET_OPTIONAL_VALUE(is_platform_app);

  if (!delta->policy_ids.empty()) {
    state->policy_ids.clear();
    state->policy_ids = delta->policy_ids;
  }

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
  SET_OPTIONAL_VALUE(allow_close);

  SET_OPTIONAL_VALUE(app_size_in_bytes);
  SET_OPTIONAL_VALUE(data_size_in_bytes);

  if (!delta->supported_locales.empty()) {
    state->supported_locales.clear();
    state->supported_locales = delta->supported_locales;
  }
  SET_OPTIONAL_VALUE(selected_locale);

  // When adding new fields to the App type, this function should also be
  // updated.

  return true;
}

}  // namespace

// static
void AppUpdate::MergeDelta(App* new_delta, App* delta) {
  if (!MergeWithoutIconKey(new_delta, delta)) {
    return;
  }

  MergeIconKeyDelta(new_delta, delta);
}

// static
void AppUpdate::Merge(App* state, const App* delta) {
  if (!MergeWithoutIconKey(state, delta)) {
    return;
  }

  state->icon_key = MergeIconKey(state, delta);
}

// static
bool AppUpdate::IsChanged(const App* state, const App* delta) {
  if (!delta) {
    return false;
  }

  if (!state) {
    return true;
  }

  CHECK_EQ(state->app_id, delta->app_id);

  if (state->app_type != delta->app_type) {
    return true;
  }

  ::AccountId acount_id;
  AppUpdate update(state, delta, acount_id);
  return update.ReadinessChanged() || update.NameChanged() ||
         update.ShortNameChanged() || update.PublisherIdChanged() ||
         update.DescriptionChanged() || update.VersionChanged() ||
         update.AdditionalSearchTermsChanged() || update.IconKeyChanged() ||
         update.LastLaunchTimeChanged() || update.InstallTimeChanged() ||
         update.PermissionsChanged() || update.InstallReasonChanged() ||
         update.InstallSourceChanged() || update.PolicyIdsChanged() ||
         update.IsPlatformAppChanged() || update.RecommendableChanged() ||
         update.SearchableChanged() || update.ShowInLauncherChanged() ||
         update.ShowInShelfChanged() || update.ShowInSearchChanged() ||
         update.ShowInManagementChanged() || update.HandlesIntentsChanged() ||
         update.AllowUninstallChanged() || update.HasBadgeChanged() ||
         update.PausedChanged() || update.IntentFiltersChanged() ||
         update.ResizeLockedChanged() || update.WindowModeChanged() ||
         update.RunOnOsLoginChanged() || update.AllowCloseChanged() ||
         update.AppSizeInBytesChanged() || update.DataSizeInBytesChanged() ||
         update.SupportedLocalesChanged() || update.SelectedLocaleChanged();
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
  return state_ == nullptr;
}

apps::AppType AppUpdate::AppType() const {
  return delta_ ? delta_->app_type : state_->app_type;
}

const std::string& AppUpdate::AppId() const {
  return delta_ ? delta_->app_id : state_->app_id;
}

apps::Readiness AppUpdate::Readiness() const {
  GET_VALUE_WITH_DEFAULT_VALUE(readiness, apps::Readiness::kUnknown);
}

apps::Readiness AppUpdate::PriorReadiness() const {
  return state_ ? state_->readiness : apps::Readiness::kUnknown;
}

bool AppUpdate::ReadinessChanged() const {
  IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(readiness, Readiness::kUnknown)
}

const std::string& AppUpdate::Name() const {
  GET_VALUE_WITH_FALLBACK(name, base::EmptyString())
}

bool AppUpdate::NameChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(name)
}

const std::string& AppUpdate::ShortName() const {
  GET_VALUE_WITH_FALLBACK(short_name, base::EmptyString())
}

bool AppUpdate::ShortNameChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(short_name)
}

const std::string& AppUpdate::PublisherId() const {
  GET_VALUE_WITH_FALLBACK(publisher_id, base::EmptyString())
}

bool AppUpdate::PublisherIdChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(publisher_id)
}

const std::string& AppUpdate::Description() const {
  GET_VALUE_WITH_FALLBACK(description, base::EmptyString())
}

bool AppUpdate::DescriptionChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(description)
}

const std::string& AppUpdate::Version() const {
  GET_VALUE_WITH_FALLBACK(version, base::EmptyString())
}

bool AppUpdate::VersionChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(version);
}

const std::vector<std::string>& AppUpdate::AdditionalSearchTerms() const {
  GET_VALUE_WITH_CHECK_AND_DEFAULT_RETURN(additional_search_terms, empty,
                                          EmptyStringVector());
}

bool AppUpdate::AdditionalSearchTermsChanged() const {
  IS_VALUE_CHANGED_WITH_CHECK(additional_search_terms, empty);
}

absl::optional<apps::IconKey> AppUpdate::IconKey() const {
  return MergeIconKey(state_, delta_);
}

bool AppUpdate::IconKeyChanged() const {
  return delta_ && delta_->icon_key.has_value() &&
         (!state_ || (MergeIconKey(state_, delta_) != state_->icon_key));
}

base::Time AppUpdate::LastLaunchTime() const {
  GET_VALUE_WITH_FALLBACK(last_launch_time, base::Time())
}

bool AppUpdate::LastLaunchTimeChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(last_launch_time);
}

base::Time AppUpdate::InstallTime() const {
  GET_VALUE_WITH_FALLBACK(install_time, base::Time())
}

bool AppUpdate::InstallTimeChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(install_time);
}

apps::Permissions AppUpdate::Permissions() const {
  if (delta_ && !delta_->permissions.empty()) {
    return ClonePermissions(delta_->permissions);
  } else if (state_ && !state_->permissions.empty()) {
    return ClonePermissions(state_->permissions);
  }
  return std::vector<PermissionPtr>{};
}

bool AppUpdate::PermissionsChanged() const {
  return delta_ && !delta_->permissions.empty() &&
         (!state_ || !IsEqual(delta_->permissions, state_->permissions));
}

apps::InstallReason AppUpdate::InstallReason() const {
  GET_VALUE_WITH_DEFAULT_VALUE(install_reason, InstallReason::kUnknown)
}

bool AppUpdate::InstallReasonChanged() const {
  IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(install_reason, InstallReason::kUnknown);
}

apps::InstallSource AppUpdate::InstallSource() const {
  GET_VALUE_WITH_DEFAULT_VALUE(install_source, InstallSource::kUnknown)
}

bool AppUpdate::InstallSourceChanged() const {
  IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(install_source, InstallSource::kUnknown);
}

const std::vector<std::string>& AppUpdate::PolicyIds() const {
  GET_VALUE_WITH_CHECK_AND_DEFAULT_RETURN(policy_ids, empty,
                                          EmptyStringVector());
}

bool AppUpdate::PolicyIdsChanged() const {
  IS_VALUE_CHANGED_WITH_CHECK(policy_ids, empty);
}

bool AppUpdate::InstalledInternally() const {
  switch (InstallReason()) {
    case apps::InstallReason::kSystem:
    case apps::InstallReason::kPolicy:
    case apps::InstallReason::kOem:
    case apps::InstallReason::kDefault:
    case apps::InstallReason::kKiosk:
      return true;
    case apps::InstallReason::kUnknown:
    case apps::InstallReason::kSync:
    case apps::InstallReason::kUser:
    case apps::InstallReason::kSubApp:
    case apps::InstallReason::kCommandLine:
      return false;
  }
}

absl::optional<bool> AppUpdate::IsPlatformApp() const {
  GET_VALUE_WITH_FALLBACK(is_platform_app, absl::nullopt)
}

bool AppUpdate::IsPlatformAppChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(is_platform_app);
}

absl::optional<bool> AppUpdate::Recommendable() const {
  GET_VALUE_WITH_FALLBACK(recommendable, absl::nullopt)
}

bool AppUpdate::RecommendableChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(recommendable);
}

absl::optional<bool> AppUpdate::Searchable() const {
  GET_VALUE_WITH_FALLBACK(searchable, absl::nullopt)
}

bool AppUpdate::SearchableChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(searchable);
}

absl::optional<bool> AppUpdate::ShowInLauncher() const {
  GET_VALUE_WITH_FALLBACK(show_in_launcher, absl::nullopt)
}

bool AppUpdate::ShowInLauncherChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(show_in_launcher);
}

absl::optional<bool> AppUpdate::ShowInShelf() const {
  GET_VALUE_WITH_FALLBACK(show_in_shelf, absl::nullopt)
}

bool AppUpdate::ShowInShelfChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(show_in_shelf);
}

absl::optional<bool> AppUpdate::ShowInSearch() const {
  GET_VALUE_WITH_FALLBACK(show_in_search, absl::nullopt)
}

bool AppUpdate::ShowInSearchChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(show_in_search);
}

absl::optional<bool> AppUpdate::ShowInManagement() const {
  GET_VALUE_WITH_FALLBACK(show_in_management, absl::nullopt)
}

bool AppUpdate::ShowInManagementChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(show_in_management);
}

absl::optional<bool> AppUpdate::HandlesIntents() const {
  GET_VALUE_WITH_FALLBACK(handles_intents, absl::nullopt)
}

bool AppUpdate::HandlesIntentsChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(handles_intents);
}

absl::optional<bool> AppUpdate::AllowUninstall() const {
  GET_VALUE_WITH_FALLBACK(allow_uninstall, absl::nullopt)
}

bool AppUpdate::AllowUninstallChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(allow_uninstall);
}

absl::optional<bool> AppUpdate::HasBadge() const {
  GET_VALUE_WITH_FALLBACK(has_badge, absl::nullopt)
}

bool AppUpdate::HasBadgeChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(has_badge);
}

absl::optional<bool> AppUpdate::Paused() const {
  GET_VALUE_WITH_FALLBACK(paused, absl::nullopt);
}

bool AppUpdate::PausedChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(paused);
}

apps::IntentFilters AppUpdate::IntentFilters() const {
  if (delta_ && !delta_->intent_filters.empty()) {
    return CloneIntentFilters(delta_->intent_filters);
  }
  if (state_ && !state_->intent_filters.empty()) {
    return CloneIntentFilters(state_->intent_filters);
  }
  return std::vector<IntentFilterPtr>{};
}

bool AppUpdate::IntentFiltersChanged() const {
  return delta_ && !delta_->intent_filters.empty() &&
         (!state_ || !IsEqual(delta_->intent_filters, state_->intent_filters));
}

absl::optional<bool> AppUpdate::ResizeLocked() const {
  GET_VALUE_WITH_FALLBACK(resize_locked, absl::nullopt);
}

bool AppUpdate::ResizeLockedChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(resize_locked);
}

apps::WindowMode AppUpdate::WindowMode() const {
  GET_VALUE_WITH_DEFAULT_VALUE(window_mode, WindowMode::kUnknown)
}

bool AppUpdate::WindowModeChanged() const {
  IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(window_mode, WindowMode::kUnknown);
}

absl::optional<apps::RunOnOsLogin> AppUpdate::RunOnOsLogin() const {
  if (delta_ && delta_->run_on_os_login.has_value()) {
    return CloneRunOnOsLogin(delta_->run_on_os_login.value());
  }
  if (state_ && state_->run_on_os_login.has_value()) {
    return CloneRunOnOsLogin(state_->run_on_os_login.value());
  }
  return absl::nullopt;
}

bool AppUpdate::RunOnOsLoginChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(run_on_os_login);
}

absl::optional<bool> AppUpdate::AllowClose() const {
  GET_VALUE_WITH_FALLBACK(allow_close, absl::nullopt)
}

bool AppUpdate::AllowCloseChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(allow_close);
}

const ::AccountId& AppUpdate::AccountId() const {
  return *account_id_;
}

absl::optional<uint64_t> AppUpdate::AppSizeInBytes() const {
  GET_VALUE_WITH_FALLBACK(app_size_in_bytes, absl::nullopt);
}

bool AppUpdate::AppSizeInBytesChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(app_size_in_bytes);
}

absl::optional<uint64_t> AppUpdate::DataSizeInBytes() const {
  GET_VALUE_WITH_FALLBACK(data_size_in_bytes, absl::nullopt);
}

bool AppUpdate::DataSizeInBytesChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(data_size_in_bytes);
}

const std::vector<std::string>& AppUpdate::SupportedLocales() const {
  GET_VALUE_WITH_CHECK_AND_DEFAULT_RETURN(supported_locales, empty,
                                          EmptyStringVector());
}

bool AppUpdate::SupportedLocalesChanged() const {
  IS_VALUE_CHANGED_WITH_CHECK(supported_locales, empty);
}

absl::optional<std::string> AppUpdate::SelectedLocale() const {
  GET_VALUE_WITH_FALLBACK(selected_locale, base::EmptyString())
}

bool AppUpdate::SelectedLocaleChanged() const {
    RETURN_OPTIONAL_VALUE_CHANGED(selected_locale)}

std::ostream&
operator<<(std::ostream& out, const AppUpdate& app) {
  out << "AppType: " << EnumToString(app.AppType()) << std::endl;
  out << "AppId: " << app.AppId() << std::endl;
  out << "Readiness: " << EnumToString(app.Readiness()) << std::endl;
  out << "Name: " << app.Name() << std::endl;
  out << "ShortName: " << app.ShortName() << std::endl;
  out << "PublisherId: " << app.PublisherId() << std::endl;
  out << "Description: " << app.Description() << std::endl;
  out << "Version: " << app.Version() << std::endl;

  out << "AdditionalSearchTerms: "
      << base::JoinString(app.AdditionalSearchTerms(), ", ") << std::endl;

  out << "LastLaunchTime: " << app.LastLaunchTime() << std::endl;
  out << "InstallTime: " << app.InstallTime() << std::endl;

  out << "Permissions:" << std::endl;
  for (const auto& permission : app.Permissions()) {
    out << permission->ToString() << std::endl;
  }

  out << "InstallReason: " << EnumToString(app.InstallReason()) << std::endl;
  out << "InstallSource: " << EnumToString(app.InstallSource()) << std::endl;

  out << "PolicyId: " << base::JoinString(app.PolicyIds(), ", ") << std::endl;

  out << "InstalledInternally: " << app.InstalledInternally() << std::endl;
  out << "IsPlatformApp: " << PRINT_OPTIONAL_BOOL(app.IsPlatformApp())
      << std::endl;
  out << "Recommendable: " << PRINT_OPTIONAL_BOOL(app.Recommendable())
      << std::endl;
  out << "Searchable: " << PRINT_OPTIONAL_BOOL(app.Searchable()) << std::endl;
  out << "ShowInLauncher: " << PRINT_OPTIONAL_BOOL(app.ShowInLauncher())
      << std::endl;
  out << "ShowInShelf: " << PRINT_OPTIONAL_BOOL(app.ShowInShelf()) << std::endl;
  out << "ShowInSearch: " << PRINT_OPTIONAL_BOOL(app.ShowInSearch())
      << std::endl;
  out << "ShowInManagement: " << PRINT_OPTIONAL_BOOL(app.ShowInManagement())
      << std::endl;
  out << "HandlesIntents: " << PRINT_OPTIONAL_BOOL(app.HandlesIntents())
      << std::endl;
  out << "AllowUninstall: " << PRINT_OPTIONAL_BOOL(app.AllowUninstall())
      << std::endl;
  out << "HasBadge: " << PRINT_OPTIONAL_BOOL(app.HasBadge()) << std::endl;
  out << "Paused: " << PRINT_OPTIONAL_BOOL(app.Paused()) << std::endl;

  out << "IntentFilters: " << std::endl;
  for (const auto& filter : app.IntentFilters()) {
    out << filter->ToString() << std::endl;
  }

  out << "ResizeLocked: " << PRINT_OPTIONAL_BOOL(app.ResizeLocked())
      << std::endl;
  out << "WindowMode: " << EnumToString(app.WindowMode()) << std::endl;
  if (app.RunOnOsLogin().has_value()) {
    out << "RunOnOsLoginMode: "
        << EnumToString(app.RunOnOsLogin().value().login_mode) << std::endl;
  }
  out << "Allow Close: " << PRINT_OPTIONAL_BOOL(app.AllowClose()) << std::endl;

  out << "App Size: " << FormatBytes(app.AppSizeInBytes()) << std::endl;
  out << "Data Size: " << FormatBytes(app.DataSizeInBytes()) << std::endl;

  out << "Supported locales: " << base::JoinString(app.SupportedLocales(), ", ")
      << std::endl;
  out << "Selected locale: "
      << (app.SelectedLocale().has_value() ? app.SelectedLocale().value()
                                           : "No selected_locale")
      << std::endl;

  return out;
}

}  // namespace apps
