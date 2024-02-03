// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/device_policies_manager.h"

#include <limits>

#include "base/base64.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
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
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

using ::enterprise_management::PolicyData;
using ::enterprise_management::PolicyFetchResponse;
using ::wireless_android_enterprise_devicemanagement::ApplicationSettings;
using ::wireless_android_enterprise_devicemanagement::
    GcpwSpecificApplicationSettings;
using ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto;

namespace credential_provider {

namespace {
// Character used to separate the update channel and version components in the
// update tracks value.
const wchar_t kChannelAndVersionSeparator[] = L"-";

// Name of the file used to store the policies on disk by Omaha that it receives
// from the DMServer.
constexpr char kOmahaPolicyFileName[] = "PolicyFetchResponse";

// Directory name where Omaha policies are stored.
constexpr char kOmahaPoliciesDirName[] = "Policies";

// The policy type for Omaha policy settings.
constexpr char kOmahaPolicyType[] = "google/machine-level-omaha";

// Gets the file path where Omaha policies for the device are stored.
base::FilePath GetOmahaPolicyFilePath() {
  base::FilePath policy_dir;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILESX86, &policy_dir)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "PathService::Get(DIR_PROGRAM_FILESX86) hr=" << putHR(hr);
    return base::FilePath();
  }

  std::string encoded_omaha_policy_type = base::Base64Encode(kOmahaPolicyType);
  policy_dir = policy_dir.Append(GetInstallParentDirectoryName())
                   .AppendASCII(kOmahaPoliciesDirName)
                   .AppendASCII(encoded_omaha_policy_type);

  base::FilePath policy_file_path =
      policy_dir.AppendASCII(kOmahaPolicyFileName);
  return policy_file_path;
}

// Returns a parsed proto of the Omaha policy fetch response stored on disk from
// the last time Omaha synced the policies from the backend server.
std::unique_ptr<PolicyFetchResponse> ReadOmahaPolicyFromDisk() {
  LOGFN(VERBOSE);

  base::FilePath policy_file_path = GetOmahaPolicyFilePath();
  std::string policy_blob;

  if (!base::PathExists(policy_file_path) ||
      !base::ReadFileToString(policy_file_path, &policy_blob)) {
    LOGFN(WARNING) << "Omaha policy not found in " << policy_file_path;
    return nullptr;
  }

  auto policy_fetch_response = std::make_unique<PolicyFetchResponse>();
  if (policy_blob.empty() ||
      !policy_fetch_response->ParseFromString(policy_blob)) {
    LOGFN(ERROR) << "Omaha policy corrupted!";
    return nullptr;
  }

  return policy_fetch_response;
}

// Get the list of domains allowed to login as defined by the cloud policy
// fetched by Omaha.
bool GetAllowedDomainsToLoginFromCloudPolicy(
    std::vector<std::wstring>* domains) {
  DCHECK(domains);

  std::unique_ptr<PolicyFetchResponse> policy_fetch_response =
      ReadOmahaPolicyFromDisk();
  if (!policy_fetch_response) {
    return false;
  }

  PolicyData policy_data;
  if (!policy_fetch_response->has_policy_data() ||
      !policy_data.ParseFromString(policy_fetch_response->policy_data()) ||
      !policy_data.has_policy_value()) {
    LOGFN(ERROR) << "Cloud policy data not found!";
    return false;
  }

  OmahaSettingsClientProto omaha_settings;
  omaha_settings.ParseFromString(policy_data.policy_value());

  bool found_gcpw_settings = false;
  for (const ApplicationSettings& app_setting :
       omaha_settings.application_settings()) {
    if (app_setting.app_guid() == base::WideToUTF8(kGcpwUpdateClientGuid) &&
        app_setting.has_gcpw_application_settings()) {
      found_gcpw_settings = true;
      const GcpwSpecificApplicationSettings& gcpw_settings =
          app_setting.gcpw_application_settings();

      domains->clear();
      for (const std::string& domain :
           gcpw_settings.domains_allowed_to_login()) {
        domains->push_back(base::UTF8ToWide(domain));
      }
      LOGFN(VERBOSE) << "Cloud policy domains: "
                     << base::JoinString(*domains, L",");

      break;
    }
  }

  return found_gcpw_settings;
}
}  // namespace

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
    std::wstring sid(iter.Name());
    wchar_t found_username[kWindowsUsernameBufferLength];
    wchar_t found_domain[kWindowsDomainBufferLength];

    // Check if this account with this sid exists on device.
    HRESULT hr = OSUserManager::Get()->FindUserBySidWithFallback(
        sid.c_str(), found_username, std::size(found_username), found_domain,
        std::size(found_domain));
    if (hr != S_OK) {
      if (hr != HRESULT_FROM_WIN32(ERROR_NONE_MAPPED)) {
        LOGFN(ERROR) << "FindUserBySidWithRegistryFallback hr=" << putHR(hr);
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

  // Read allowed domains cloud policy.
  std::vector<std::wstring> domains_from_policy;
  if (GetAllowedDomainsToLoginFromCloudPolicy(&domains_from_policy)) {
    if (!domains_from_policy.empty()) {
      device_policies->domains_allowed_to_login = domains_from_policy;
    } else {
      LOGFN(VERBOSE) << "Allowed domains cloud policy is empty. Falling back "
                        "to device registry settings.";
    }
  } else {
    LOGFN(VERBOSE) << "Allowed domains cloud policy not found";
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

  std::wstring update_channel;  // Empty value indicates the stable channel.
  std::wstring ap_value;
  LONG ap_status = key.ReadValue(kRegUpdateTracksName, &ap_value);
  GcpwVersion current_pinned_version(base::WideToUTF8(ap_value));

  if (ap_status == ERROR_SUCCESS && !current_pinned_version.IsValid()) {
    std::vector<std::wstring> ap_components =
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
    std::wstring gcpw_version;
    if (device_policies.enable_gcpw_auto_update &&
        device_policies.gcpw_pinned_version.IsValid()) {
      // Auto update enabled with pinning so set it to the pinned track.
      gcpw_version =
          base::UTF8ToWide(device_policies.gcpw_pinned_version.ToString());
    } else {
      // Auto update is disabled so make sure we stay on the installed
      // version.
      GcpwVersion version = GcpwVersion::GetCurrentVersion();
      if (!version.IsValid()) {
        LOGFN(ERROR) << "Could not read currently installed version";
        return;
      }
      gcpw_version = base::UTF8ToWide(version.ToString());
    }

    ap_value = gcpw_version;
    if (!update_channel.empty())
      ap_value = update_channel + kChannelAndVersionSeparator + gcpw_version;

    status = key.WriteValue(kRegUpdateTracksName, ap_value.c_str());
    if (status != ERROR_SUCCESS) {
      LOGFN(ERROR) << "Unable to write " << kRegUpdateTracksName
                   << " value status=" << status;
    }
  }
}

bool DevicePoliciesManager::SetAllowedDomainsOmahaPolicyForTesting(
    const std::vector<std::wstring>& domains) {
  OmahaSettingsClientProto omaha_settings;
  ApplicationSettings* app_settings = omaha_settings.add_application_settings();
  app_settings->set_app_guid(base::WideToUTF8(kGcpwUpdateClientGuid));
  GcpwSpecificApplicationSettings* gcpw_settings =
      app_settings->mutable_gcpw_application_settings();

  for (const std::wstring& domain : domains) {
    gcpw_settings->add_domains_allowed_to_login(base::WideToUTF8(domain));
  }

  PolicyData policy_data;
  std::string policy_value = omaha_settings.SerializeAsString();
  policy_data.set_policy_value(policy_value);

  PolicyFetchResponse policy_fetch_response;
  policy_fetch_response.set_error_code(200);
  std::string policy_data_str = policy_data.SerializeAsString();
  policy_fetch_response.set_policy_data(policy_data_str);

  base::FilePath policy_file_path = GetOmahaPolicyFilePath();
  std::string policy_response_str = policy_fetch_response.SerializeAsString();
  base::CreateDirectory(policy_file_path.DirName());
  return base::WriteFile(policy_file_path, policy_response_str);
}

}  // namespace credential_provider
