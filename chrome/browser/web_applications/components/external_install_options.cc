// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/external_install_options.h"

#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"

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
  auto AsTuple = [](const ExternalInstallOptions& options) {
    // Keep in order declared in external_install_options.h.
    return std::tie(
        // clang-format off
        options.install_url,
        options.user_display_mode,
        options.install_source,
        options.fallback_app_name,
        options.add_to_applications_menu,
        options.add_to_desktop,
        options.add_to_quick_launch_bar,
        options.add_to_search,
        options.add_to_management,
        options.run_on_os_login,
        options.is_disabled,
        options.override_previous_user_uninstall,
        options.only_for_new_users,
        options.user_type_allowlist,
        options.gate_on_feature,
#if BUILDFLAG(IS_CHROMEOS_ASH)
        options.disable_if_arc_supported,
        options.disable_if_tablet_form_factor,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
        options.bypass_service_worker_check,
        options.require_manifest,
        options.force_reinstall,
        options.force_reinstall_for_milestone,
        options.wait_for_windows_closed,
        options.install_placeholder,
        options.reinstall_placeholder,
        options.launch_query_params,
        options.load_and_await_service_worker_registration,
        options.service_worker_registration_url,
        options.uninstall_and_replace,
        options.additional_search_terms,
        options.only_use_app_info_factory,
        options.app_info_factory,
        options.system_app_type,
        options.oem_installed
        // clang-format on
    );
  };
  return AsTuple(*this) == AsTuple(other);
}

namespace {

template <typename T>
std::ostream& operator<<(std::ostream& out, const base::Optional<T>& optional) {
  if (optional)
    out << *optional;
  else
    out << "nullopt";
  return out;
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& list) {
  out << '[';
  for (size_t i = 0; i < list.size(); ++i) {
    if (i > 0)
      out << ", ";
    out << list[i];
  }
  out << ']';
  return out;
}

}  // namespace

std::ostream& operator<<(std::ostream& out,
                         const ExternalInstallOptions& install_options) {
  return out
         << "install_url: " << install_options.install_url
         << "\n user_display_mode: " << install_options.user_display_mode
         << "\n install_source: "
         << static_cast<int32_t>(install_options.install_source)
         << "\n fallback_app_name: "
         << install_options.fallback_app_name.value_or("")
         << "\n add_to_applications_menu: "
         << install_options.add_to_applications_menu
         << "\n add_to_desktop: " << install_options.add_to_desktop
         << "\n add_to_quick_launch_bar: "
         << install_options.add_to_quick_launch_bar
         << "\n add_to_search: " << install_options.add_to_search
         << "\n add_to_management: " << install_options.add_to_management
         << "\n run_on_os_login: " << install_options.run_on_os_login
         << "\n is_disabled: " << install_options.is_disabled
         << "\n override_previous_user_uninstall: "
         << install_options.override_previous_user_uninstall
         << "\n only_for_new_users: " << install_options.only_for_new_users
         << "\n user_type_allowlist: " << install_options.user_type_allowlist
         << "\n gate_on_feature: " << install_options.gate_on_feature
#if BUILDFLAG(IS_CHROMEOS_ASH)
         << "\n disable_if_arc_supported: "
         << install_options.disable_if_arc_supported
         << "\n disable_if_tablet_form_factor: "
         << install_options.disable_if_tablet_form_factor
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
         << "\n bypass_service_worker_check: "
         << install_options.bypass_service_worker_check
         << "\n require_manifest: " << install_options.require_manifest
         << "\n force_reinstall: " << install_options.force_reinstall
         << "\n force_reinstall_for_milestone: "
         << install_options.force_reinstall_for_milestone
         << "\n wait_for_windows_closed: "
         << install_options.wait_for_windows_closed
         << "\n install_placeholder: " << install_options.install_placeholder
         << "\n reinstall_placeholder: "
         << install_options.reinstall_placeholder
         << "\n launch_query_params: " << install_options.launch_query_params
         << "\n load_and_await_service_worker_registration: "
         << install_options.load_and_await_service_worker_registration
         << "\n service_worker_registration_url: "
         << install_options.service_worker_registration_url.value_or(GURL())
         << "\n uninstall_and_replace:\n   "
         << base::JoinString(install_options.uninstall_and_replace, "\n   ")
         << "\n additional_search_terms:\n   "
         << base::JoinString(install_options.additional_search_terms, "\n   ")
         << "\n only_use_app_info_factory: "
         << install_options.only_use_app_info_factory << "\n app_info_factory: "
         << !install_options.app_info_factory.is_null()
         << "\n system_app_type: "
         << (install_options.system_app_type.has_value()
                 ? static_cast<int32_t>(install_options.system_app_type.value())
                 : -1)
         << "\n oem_installed: " << install_options.oem_installed;
}

InstallManager::InstallParams ConvertExternalInstallOptionsToParams(
    const ExternalInstallOptions& install_options) {
  InstallManager::InstallParams params;

  params.user_display_mode = install_options.user_display_mode;

  if (install_options.fallback_app_name.has_value()) {
    params.fallback_app_name =
        base::UTF8ToUTF16(install_options.fallback_app_name.value());
  }

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

  params.system_app_type = install_options.system_app_type;

  params.oem_installed = install_options.oem_installed;

  return params;
}

}  // namespace web_app
