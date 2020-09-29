// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/external_install_options.h"

#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "base/strings/string_util.h"

namespace web_app {

ExternalInstallOptions::ExternalInstallOptions(
    const GURL& install_url,
    DisplayMode user_display_mode,
    ExternalInstallSource install_source)
    : install_url(install_url),
      user_display_mode(user_display_mode),
      install_source(install_source) {}

ExternalInstallOptions::~ExternalInstallOptions() = default;

ExternalInstallOptions::ExternalInstallOptions(
    const ExternalInstallOptions& other) = default;

ExternalInstallOptions::ExternalInstallOptions(ExternalInstallOptions&& other) =
    default;

ExternalInstallOptions& ExternalInstallOptions::operator=(
    const ExternalInstallOptions& other) = default;

bool ExternalInstallOptions::operator==(
    const ExternalInstallOptions& other) const {
  return std::tie(install_url, user_display_mode, install_source,
                  add_to_applications_menu, add_to_desktop,
                  add_to_quick_launch_bar, add_to_search, add_to_management,
                  is_disabled, override_previous_user_uninstall,
                  bypass_service_worker_check, require_manifest,
                  force_reinstall, wait_for_windows_closed, install_placeholder,
                  reinstall_placeholder,
                  load_and_await_service_worker_registration,
                  service_worker_registration_url, uninstall_and_replace,
                  additional_search_terms, only_use_app_info_factory) ==
         std::tie(other.install_url, other.user_display_mode,
                  other.install_source, other.add_to_applications_menu,
                  other.add_to_desktop, other.add_to_quick_launch_bar,
                  other.add_to_search, other.add_to_management,
                  other.is_disabled, other.override_previous_user_uninstall,
                  other.bypass_service_worker_check, other.require_manifest,
                  other.force_reinstall, other.wait_for_windows_closed,
                  other.install_placeholder, other.reinstall_placeholder,
                  other.load_and_await_service_worker_registration,
                  other.service_worker_registration_url,
                  other.uninstall_and_replace, other.additional_search_terms,
                  other.only_use_app_info_factory);
}

std::ostream& operator<<(std::ostream& out,
                         const ExternalInstallOptions& install_options) {
  return out << "install_url: " << install_options.install_url
             << "\n user_display_mode: "
             << static_cast<int32_t>(install_options.user_display_mode)
             << "\n install_source: "
             << static_cast<int32_t>(install_options.install_source)
             << "\n add_to_applications_menu: "
             << install_options.add_to_applications_menu
             << "\n add_to_desktop: " << install_options.add_to_desktop
             << "\n add_to_quick_launch_bar: "
             << install_options.add_to_quick_launch_bar
             << "\n add_to_search: " << install_options.add_to_search
             << "\n add_to_management: " << install_options.add_to_management
             << "\n is_disabled: " << install_options.is_disabled
             << "\n override_previous_user_uninstall: "
             << install_options.override_previous_user_uninstall
             << "\n bypass_service_worker_check: "
             << install_options.bypass_service_worker_check
             << "\n require_manifest: " << install_options.require_manifest
             << "\n force_reinstall: " << install_options.force_reinstall
             << "\n wait_for_windows_closed: "
             << install_options.wait_for_windows_closed
             << "\n install_placeholder: "
             << install_options.install_placeholder
             << "\n reinstall_placeholder: "
             << install_options.reinstall_placeholder
             << "\n load_and_await_service_worker_registration: "
             << install_options.load_and_await_service_worker_registration
             << "\n service_worker_registration_url: "
             << install_options.service_worker_registration_url.value_or(GURL())
             << "\n uninstall_and_replace:\n  "
             << base::JoinString(install_options.uninstall_and_replace, "\n  ")
             << "\n only_use_app_info_factory:\n "
             << install_options.only_use_app_info_factory
             << "\n additional_search_terms:\n "
             << base::JoinString(install_options.additional_search_terms,
                                 "\n ");
}

InstallManager::InstallParams ConvertExternalInstallOptionsToParams(
    const ExternalInstallOptions& install_options) {
  InstallManager::InstallParams params;

  params.user_display_mode = install_options.user_display_mode;

  params.fallback_start_url = install_options.install_url;

  params.add_to_applications_menu = install_options.add_to_applications_menu;
  params.add_to_desktop = install_options.add_to_desktop;
  params.add_to_quick_launch_bar = install_options.add_to_quick_launch_bar;
  params.run_on_os_login = install_options.run_on_os_login;
  params.add_to_search = install_options.add_to_search;
  params.add_to_management = install_options.add_to_management;
  params.is_disabled = install_options.is_disabled;

  params.bypass_service_worker_check =
      install_options.bypass_service_worker_check;
  params.require_manifest = install_options.require_manifest;

  params.additional_search_terms = install_options.additional_search_terms;

  params.launch_query_params = install_options.launch_query_params;

  return params;
}

}  // namespace web_app
