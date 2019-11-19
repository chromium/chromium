// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/external_install_options.h"

#include <ostream>
#include <tuple>

#include "base/strings/string_util.h"

namespace web_app {

ExternalInstallOptions::ExternalInstallOptions(
    const GURL& url,
    DisplayMode user_display_mode,
    ExternalInstallSource install_source)
    : url(url),
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
  return std::tie(url, user_display_mode, install_source,
                  add_to_applications_menu, add_to_desktop,
                  add_to_quick_launch_bar, override_previous_user_uninstall,
                  bypass_service_worker_check, require_manifest,
                  force_reinstall, wait_for_windows_closed, install_placeholder,
                  reinstall_placeholder, uninstall_and_replace) ==
         std::tie(other.url, other.user_display_mode, other.install_source,
                  other.add_to_applications_menu, other.add_to_desktop,
                  other.add_to_quick_launch_bar,
                  other.override_previous_user_uninstall,
                  other.bypass_service_worker_check, other.require_manifest,
                  other.force_reinstall, other.wait_for_windows_closed,
                  other.install_placeholder, other.reinstall_placeholder,
                  other.uninstall_and_replace);
}

std::ostream& operator<<(std::ostream& out,
                         const ExternalInstallOptions& install_options) {
  return out << "url: " << install_options.url << "\n user_display_mode: "
             << static_cast<int32_t>(install_options.user_display_mode)
             << "\n install_source: "
             << static_cast<int32_t>(install_options.install_source)
             << "\n add_to_applications_menu: "
             << install_options.add_to_applications_menu
             << "\n add_to_desktop: " << install_options.add_to_desktop
             << "\n add_to_quick_launch_bar: "
             << install_options.add_to_quick_launch_bar
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
             << "\n uninstall_and_replace:\n  "
             << base::JoinString(install_options.uninstall_and_replace, "\n  ");
}

InstallManager::InstallParams ConvertExternalInstallOptionsToParams(
    const ExternalInstallOptions& install_options) {
  InstallManager::InstallParams params;

  params.user_display_mode = install_options.user_display_mode;

  params.fallback_start_url = install_options.url;

  params.add_to_applications_menu = install_options.add_to_applications_menu;
  params.add_to_desktop = install_options.add_to_desktop;
  params.add_to_quick_launch_bar = install_options.add_to_quick_launch_bar;

  params.bypass_service_worker_check =
      install_options.bypass_service_worker_check;
  params.require_manifest = install_options.require_manifest;

  return params;
}

}  // namespace web_app
