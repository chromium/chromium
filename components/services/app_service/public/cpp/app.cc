// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app.h"

namespace apps {

App::App(AppType app_type, const std::string& app_id)
    : app_type(app_type), app_id(app_id) {}

App::~App() = default;

bool App::operator==(const App& other) const {
  if (this->app_id != other.app_id || this->app_type != other.app_type ||
      this->readiness != other.readiness ||
      this->install_reason != other.install_reason ||
      this->install_source != other.install_source ||
      this->window_mode != other.window_mode) {
    return false;
  }

  if (this->name != other.name) {
    return false;
  }
  if (this->short_name != other.short_name) {
    return false;
  }
  if (this->publisher_id != other.publisher_id) {
    return false;
  }
  if (this->installer_package_id != other.installer_package_id) {
    return false;
  }
  if (this->description != other.description) {
    return false;
  }
  if (this->version != other.version) {
    return false;
  }
  if (this->icon_key != other.icon_key) {
    return false;
  }
  if (this->last_launch_time != other.last_launch_time) {
    return false;
  }
  if (this->install_time != other.install_time) {
    return false;
  }
  if (this->is_platform_app != other.is_platform_app) {
    return false;
  }
  if (this->recommendable != other.recommendable) {
    return false;
  }
  if (this->searchable != other.searchable) {
    return false;
  }
  if (this->show_in_launcher != other.show_in_launcher) {
    return false;
  }
  if (this->show_in_shelf != other.show_in_shelf) {
    return false;
  }
  if (this->show_in_search != other.show_in_search) {
    return false;
  }
  if (this->show_in_management != other.show_in_management) {
    return false;
  }
  if (this->handles_intents != other.handles_intents) {
    return false;
  }
  if (this->allow_uninstall != other.allow_uninstall) {
    return false;
  }
  if (this->has_badge != other.has_badge) {
    return false;
  }
  if (this->paused != other.paused) {
    return false;
  }
  if (this->resize_locked != other.resize_locked) {
    return false;
  }
  if (this->run_on_os_login != other.run_on_os_login) {
    return false;
  }
  if (this->allow_close != other.allow_close) {
    return false;
  }
  if (this->allow_window_mode_selection != other.allow_window_mode_selection) {
    return false;
  }
  if (this->app_size_in_bytes != other.app_size_in_bytes) {
    return false;
  }
  if (this->data_size_in_bytes != other.data_size_in_bytes) {
    return false;
  }

  IS_VECTOR_VALUE_EQUAL(additional_search_terms);
  IS_VECTOR_VALUE_EQUAL(policy_ids);

  if (!IsEqual(this->permissions, other.permissions)) {
    return false;
  }

  if (!IsEqual(this->intent_filters, other.intent_filters)) {
    return false;
  }

  IS_VECTOR_VALUE_EQUAL(supported_locales);
  if (this->selected_locale != other.selected_locale) {
    return false;
  }

  if (this->extra != other.extra) {
    return false;
  }

  return true;
}

bool App::operator!=(const App& other) const {
  return !(*this == other);
}

AppPtr App::Clone() const {
  auto app = std::make_unique<App>(app_type, app_id);

  app->readiness = readiness;
  app->name = name;
  app->short_name = short_name;
  app->publisher_id = publisher_id;
  app->description = description;
  app->version = version;
  app->additional_search_terms = additional_search_terms;

  if (icon_key.has_value()) {
    app->icon_key = std::move(*icon_key->Clone());
  }

  app->last_launch_time = last_launch_time;
  app->install_time = install_time;
  app->permissions = ClonePermissions(permissions);
  app->install_reason = install_reason;
  app->install_source = install_source;
  app->policy_ids = policy_ids;
  app->installer_package_id = installer_package_id;
  app->is_platform_app = is_platform_app;
  app->recommendable = recommendable;
  app->searchable = searchable;
  app->show_in_launcher = show_in_launcher;
  app->show_in_shelf = show_in_shelf;
  app->show_in_search = show_in_search;
  app->show_in_management = show_in_management;
  app->handles_intents = handles_intents;
  app->allow_uninstall = allow_uninstall;
  app->has_badge = has_badge;
  app->paused = paused;
  app->intent_filters = CloneIntentFilters(intent_filters);
  app->resize_locked = resize_locked;
  app->window_mode = window_mode;
  app->allow_window_mode_selection = allow_window_mode_selection;

  if (run_on_os_login.has_value()) {
    app->run_on_os_login = apps::RunOnOsLogin(run_on_os_login->login_mode,
                                              run_on_os_login->is_managed);
  }
  app->allow_close = allow_close;

  app->app_size_in_bytes = app_size_in_bytes;
  app->data_size_in_bytes = data_size_in_bytes;

  app->supported_locales = supported_locales;
  app->selected_locale = selected_locale;

  if (extra.has_value()) {
    app->extra = extra->Clone();
  }

  return app;
}

bool IsEqual(const std::vector<AppPtr>& source,
             const std::vector<AppPtr>& target) {
  if (source.size() != target.size()) {
    return false;
  }

  for (size_t i = 0; i < source.size(); i++) {
    if (*source[i] != *target[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace apps
