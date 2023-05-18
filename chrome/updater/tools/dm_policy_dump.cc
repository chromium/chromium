// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <string>
#

#include "base/base64.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/device_management/dm_cached_policy_info.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace updater {
namespace {

std::string UpdateValueAsString(
    ::wireless_android_enterprise_devicemanagement::UpdateValue value) {
  switch (value) {
    case ::wireless_android_enterprise_devicemanagement::UPDATES_DISABLED:
      return "Disabled";
    case ::wireless_android_enterprise_devicemanagement::MANUAL_UPDATES_ONLY:
      return "Manual Updates Only";
    case ::wireless_android_enterprise_devicemanagement::AUTOMATIC_UPDATES_ONLY:
      return "Automatic Updates Only";
    case ::wireless_android_enterprise_devicemanagement::UPDATES_ENABLED:
    default:
      return "Enabled";
  }
}

std::string InstallDefaultValueAsString(
    ::wireless_android_enterprise_devicemanagement::InstallDefaultValue value) {
  switch (value) {
    case ::wireless_android_enterprise_devicemanagement::
        INSTALL_DEFAULT_DISABLED:
      return "Disabled";
    case ::wireless_android_enterprise_devicemanagement::
        INSTALL_DEFAULT_ENABLED_MACHINE_ONLY:
      return "Enabled Machine Only";
    case ::wireless_android_enterprise_devicemanagement::
        INSTALL_DEFAULT_ENABLED:
    default:
      return "Enabled";
  }
}

std::string InstallValueAsString(
    ::wireless_android_enterprise_devicemanagement::InstallValue value) {
  switch (value) {
    case ::wireless_android_enterprise_devicemanagement::INSTALL_DISABLED:
      return "Disabled";
    case ::wireless_android_enterprise_devicemanagement::
        INSTALL_ENABLED_MACHINE_ONLY:
      return "Enabled Machine Only";
    case ::wireless_android_enterprise_devicemanagement::INSTALL_FORCED:
      return "Forced";
    case ::wireless_android_enterprise_devicemanagement::INSTALL_ENABLED:
    default:
      return "Enabled";
  }
}

void PrintPolicies() {
  scoped_refptr<DMStorage> storage = GetDefaultDMStorage();
  if (!storage) {
    std::cerr << "Failed to instantiate DM storage instance." << std::endl;
    return;
  }

  std::cout << "-------------------------------------------------" << std::endl;
  std::cout << "Device ID: " << storage->GetDeviceID() << std::endl;
  std::cout << "Enrollment token: " << storage->GetEnrollmentToken()
            << std::endl;
  std::cout << "DM token: " << storage->GetDmToken() << std::endl;
  std::cout << "-------------------------------------------------" << std::endl;

  std::unique_ptr<CachedPolicyInfo> cached_info =
      storage->GetCachedPolicyInfo();
  if (cached_info) {
    std::cout << "Cached policy info (for subsequent policy fetch):"
              << std::endl;
    if (cached_info->has_key_version()) {
      std::cout << "  Key version: " << cached_info->key_version() << std::endl;
      std::cout << "  Key size: " << cached_info->public_key().size()
                << std::endl;
    }
    std::cout << "  Timestamp: " << cached_info->timestamp() << std::endl;
  }
  std::cout << "-------------------------------------------------" << std::endl;

  std::unique_ptr<
      ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto>
      omaha_settings = storage->GetOmahaPolicySettings();
  if (omaha_settings) {
    bool has_global_policy = false;
    std::cout << "Global policies:" << std::endl;
    if (omaha_settings->has_install_default()) {
      std::cout << "  InstallDefault: "
                << InstallDefaultValueAsString(
                       omaha_settings->install_default())
                << "(" << omaha_settings->install_default() << ")" << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_update_default()) {
      std::cout << "  UpdateDefault: "
                << UpdateValueAsString(omaha_settings->update_default()) << "("
                << omaha_settings->update_default() << ")" << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_auto_update_check_period_minutes()) {
      std::cout << "  Auto-update check period minutes: "
                << omaha_settings->auto_update_check_period_minutes()
                << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_updates_suppressed()) {
      std::cout << "  Update suppressed: " << std::endl
                << "      Start Hour: "
                << omaha_settings->updates_suppressed().start_hour()
                << std::endl
                << "      Start Minute: "
                << omaha_settings->updates_suppressed().start_minute()
                << std::endl
                << "      Durantion Minute: "
                << omaha_settings->updates_suppressed().duration_min()
                << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_proxy_mode()) {
      std::cout << "  Proxy Mode: " << omaha_settings->proxy_mode()
                << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_proxy_pac_url()) {
      std::cout << "  Proxy PacURL: " << omaha_settings->proxy_pac_url()
                << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_proxy_server()) {
      std::cout << "  Proxy Server: " << omaha_settings->proxy_server()
                << std::endl;
      has_global_policy = true;
    }
    if (omaha_settings->has_download_preference()) {
      std::cout << "  DownloadPreference: "
                << omaha_settings->download_preference() << std::endl;
      has_global_policy = true;
    }
    if (!has_global_policy) {
      std::cout << "  (No policy)" << std::endl;
    }

    for (const auto& app_settings : omaha_settings->application_settings()) {
      bool has_policy = false;
      if (app_settings.has_app_guid()) {
        std::cout << "App : " << app_settings.app_guid();
        if (app_settings.has_bundle_identifier()) {
          std::cout << "(" << app_settings.bundle_identifier() << ")";
        }
        std::cout << std::endl;
      }
      if (app_settings.has_install()) {
        std::cout << "  Install : "
                  << InstallValueAsString(app_settings.install()) << "("
                  << app_settings.install() << ")" << std::endl;
        has_policy = true;
      }
      if (app_settings.has_update()) {
        std::cout << "  Update : " << UpdateValueAsString(app_settings.update())
                  << "(" << app_settings.update() << ")" << std::endl;
        has_policy = true;
      }
      if (app_settings.has_rollback_to_target_version()) {
        std::cout << "  RollbackToTargetVersionAllowed : "
                  << app_settings.rollback_to_target_version() << std::endl;
        has_policy = true;
      }
      if (app_settings.has_target_version_prefix()) {
        std::cout << "  TargetVersionPrefix : "
                  << app_settings.target_version_prefix() << std::endl;
        has_policy = true;
      }
      if (app_settings.has_target_channel()) {
        std::cout << "  TargetChannel : " << app_settings.target_channel()
                  << std::endl;
        has_policy = true;
      }
      if (app_settings.has_gcpw_application_settings()) {
        std::cout << "  DomainsAllowedToLogin: ";
        for (const auto& domain : app_settings.gcpw_application_settings()
                                      .domains_allowed_to_login()) {
          std::cout << domain << ", ";
          has_policy = true;
        }
        std::cout << std::endl;
      }
      if (!has_policy) {
        std::cout << "  (No policy)" << std::endl;
      }
    }
  }
  std::cout << "-------------------------------------------------" << std::endl;
  base::FileEnumerator e(storage->policy_cache_folder(), false,
                         base::FileEnumerator::DIRECTORIES);
  std::cout << "Downloaded policy types:" << std::endl;
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    std::string policy_type;
    if (base::Base64Decode(name.BaseName().MaybeAsASCII(), &policy_type)) {
      std::cout << "  " << policy_type << std::endl;
    }
  }
  std::cout << std::endl;
}

}  // namespace
}  // namespace updater

int main(int argc, char* argv[]) {
  updater::PrintPolicies();
}
