// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/device_policies_manager.h"

#include <limits>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/user_policies_manager.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace {
// Character used to separate the update channel and version components in the
// update tracks value.
const wchar_t kChannelAndVersionSeparator[] = L"-";
}  // namespace

namespace credential_provider {

// static
DevicePoliciesManager* DevicePoliciesManager::Get() {
  return *GetInstanceStorage();
}

// static
DevicePoliciesManager** DevicePoliciesManager::GetInstanceStorage() {
  static DevicePoliciesManager instance;
  static DevicePoliciesManager* instance_storage = &instance;
  return &instance_storage;
}

DevicePoliciesManager::DevicePoliciesManager() {}

DevicePoliciesManager::~DevicePoliciesManager() = default;

bool DevicePoliciesManager::CloudPoliciesEnabled() const {
  return UserPoliciesManager::Get()->CloudPoliciesEnabled();
}

void DevicePoliciesManager::GetDevicePolicies(DevicePolicies* device_policies) {
  bool found_existing_user = false;
  UserPoliciesManager* user_policies_manager = UserPoliciesManager::Get();
  base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, kGcpUsersRootKeyName);
  for (; iter.Valid(); ++iter) {
    base::string16 sid(iter.Name());
    // Check if this account with this sid exists on device.
    HRESULT hr = OSUserManager::Get()->FindUserBySID(sid.c_str(), nullptr, 0,
                                                     nullptr, 0);
    if (hr != S_OK) {
      if (hr != HRESULT_FROM_WIN32(ERROR_NONE_MAPPED)) {
        LOGFN(ERROR) << "FindUserBySID hr=" << putHR(hr);
      } else {
        LOGFN(WARNING) << sid << " is not a valid sid";
      }
      continue;
    }

    UserPolicies user_policies;
    if (!user_policies_manager->GetUserPolicies(sid, &user_policies)) {
      LOGFN(ERROR) << "Failed to read user policies for " << sid;
      continue;
    }

    // We need to first find a single active user whose policies we start with
    // and then merge with the policies of other users.
    if (!found_existing_user) {
      *device_policies = DevicePolicies::FromUserPolicies(user_policies);
      found_existing_user = true;
    } else {
      DevicePolicies other_policies =
          DevicePolicies::FromUserPolicies(user_policies);
      device_policies->MergeWith(other_policies);
    }
  }
}

void DevicePoliciesManager::EnforceGcpwUpdatePolicy() {
  // Apply the Omaha update policy.
  DevicePolicies device_policies;
  GetDevicePolicies(&device_policies);

  base::win::RegKey key;
  LONG status = key.Create(HKEY_LOCAL_MACHINE, kRegUpdaterClientStateAppPath,
                           KEY_READ | KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (status != ERROR_SUCCESS) {
    LOGFN(ERROR) << "Unable to open omaha key=" << kRegUpdaterClientStateAppPath
                 << " status=" << status;
    return;
  }

  base::string16 update_channel;  // Empty value indicates the stable channel.
  std::wstring ap_value;
  LONG ap_status = key.ReadValue(kRegUpdateTracksName, &ap_value);
  GcpwVersion current_pinned_version(base::UTF16ToUTF8(ap_value));

  if (ap_status == ERROR_SUCCESS && !current_pinned_version.IsValid()) {
    std::vector<base::string16> ap_components =
        base::SplitString(ap_value, kChannelAndVersionSeparator,
                          base::WhitespaceHandling::TRIM_WHITESPACE,
                          base::SplitResult::SPLIT_WANT_NONEMPTY);
    if (ap_components.size() > 0)
      update_channel = ap_components[0];
  }

  if (device_policies.enable_gcpw_auto_update &&
      !device_policies.gcpw_pinned_version.IsValid()) {
    // Auto update enabled with no pinning so if installation was previously
    // pinned to a version, remove the registry entry if device was on the
    // stable channel or restore the previous channel.

    if (ap_status == ERROR_SUCCESS && update_channel.empty()) {
      status = key.DeleteValue(kRegUpdateTracksName);
      if (status != ERROR_SUCCESS) {
        LOGFN(ERROR) << "Unable to delete " << kRegUpdateTracksName
                     << " value status=" << status;
      }
    } else if (update_channel != ap_value) {
      status = key.WriteValue(kRegUpdateTracksName, update_channel.c_str());
      if (status != ERROR_SUCCESS) {
        LOGFN(ERROR) << "Unable to reset " << kRegUpdateTracksName
                     << " value to " << update_channel << ". status=" << status;
      }
    }
  } else {
    base::string16 gcpw_version;
    if (device_policies.enable_gcpw_auto_update &&
        device_policies.gcpw_pinned_version.IsValid()) {
      // Auto update enabled with pinning so set it to the pinned track.
      gcpw_version =
          base::UTF8ToUTF16(device_policies.gcpw_pinned_version.ToString());
    } else {
      // Auto update is disabled so make sure we stay on the installed
      // version.
      GcpwVersion version = GcpwVersion::GetCurrentVersion();
      if (!version.IsValid()) {
        LOGFN(ERROR) << "Could not read currently installed version";
        return;
      }
      gcpw_version = base::UTF8ToUTF16(version.ToString());
    }

    base::string16 ap_value = gcpw_version;
    if (!update_channel.empty())
      ap_value = update_channel + kChannelAndVersionSeparator + gcpw_version;

    status = key.WriteValue(kRegUpdateTracksName, ap_value.c_str());
    if (status != ERROR_SUCCESS) {
      LOGFN(ERROR) << "Unable to write " << kRegUpdateTracksName
                   << " value status=" << status;
    }
  }
}

}  // namespace credential_provider
