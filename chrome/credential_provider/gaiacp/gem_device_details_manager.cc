// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gem_device_details_manager.h"

#include <windows.h>

#include <lm.h>  // Needed for LSA_UNICODE_STRING
#include <process.h>
#include <winternl.h>

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <ntsecapi.h>  // For POLICY_ALL_ACCESS types

#include <algorithm>
#include <memory>

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {

const base::TimeDelta
    GemDeviceDetailsManager::kDefaultUploadDeviceDetailsRequestTimeout =
        base::Milliseconds(12000);

namespace {

// Constants used for contacting the gem service.
const char kGemServiceUploadDeviceDetailsPath[] = "/v1/uploadDeviceDetails";
const char kUploadDeviceDetailsRequestSerialNumberParameterName[] =
    "device_serial_number";
const char kUploadDeviceDetailsRequestMachineGuidParameterName[] =
    "machine_guid";
const char kUploadDeviceDetailsRequestDeviceResourceIdParameterName[] =
    "device_resource_id";
const char kUploadDeviceDetailsRequestUserSidParameterName[] = "user_sid";
const char kUploadDeviceDetailsRequestUsernameParameterName[] =
    "account_username";
const char kUploadDeviceDetailsRequestDomainParameterName[] = "device_domain";
const char kIsAdJoinedUserParameterName[] = "is_ad_joined_user";
const char kMacAddressParameterName[] = "wlan_mac_addr";
const char kUploadDeviceDetailsResponseDeviceResourceIdParameterName[] =
    "deviceResourceId";
const char kOsVersion[] = "os_edition";
const char kBuiltInAdminNameParameterName[] = "built_in_admin_name";
const char kAdminGroupNameParameterName[] = "admin_group_name";
const char kDmToken[] = "dm_token";
const char kObfuscatedGaiaId[] = "obfuscated_gaia_id";

// Registry key to control whether upload device details from ESA feature is
// enabled.
const wchar_t kUploadDeviceDetailsFromEsaEnabledRegKey[] =
    L"upload_device_details_from_esa";

// The period of uploading device details to the backend.
const base::TimeDelta kUploadDeviceDetailsExecutionPeriod = base::Hours(3);

// True when upload device details from ESA feature  is enabled.
bool g_upload_device_details_from_esa_enabled = false;

// Maximum number of retries if a HTTP call to the backend fails.
constexpr unsigned int kMaxNumHttpRetries = 3;

// Defines a task that is called by the ESA to upload device details.
class UploadDeviceDetailsTask : public extension::Task {
 public:
  static std::unique_ptr<extension::Task> Create() {
    std::unique_ptr<extension::Task> esa_task(new UploadDeviceDetailsTask());
    return esa_task;
  }

  // ESA calls this to retrieve a configuration for the task execution. Return
  // 3 hours period for uploading device details.
  extension::Config GetConfig() final {
    extension::Config config;
    config.execution_period = kUploadDeviceDetailsExecutionPeriod;
    return config;
  }

  // ESA calls this to set all the user-device contexts for the execution of the
  // task.
  HRESULT SetContext(const std::vector<extension::UserDeviceContext>& c) final {
    context_ = c;
    return S_OK;
  }

  // ESA calls execute function to perform the actual task.
  HRESULT Execute() final {
    HRESULT task_status = S_OK;
    for (const auto& c : context_) {
      HRESULT hr = GemDeviceDetailsManager::Get()->UploadDeviceDetails(c);
      if (FAILED(hr)) {
        LOGFN(ERROR) << "Failed uploading device details for " << c.user_sid
                     << ". hr=" << putHR(hr);
        task_status = hr;
      }
    }
    return task_status;
  }

 private:
  std::vector<extension::UserDeviceContext> context_;
};
}  // namespace

// static
GemDeviceDetailsManager* GemDeviceDetailsManager::Get() {
  return *GetInstanceStorage();
}

// static
GemDeviceDetailsManager** GemDeviceDetailsManager::GetInstanceStorage() {
  static GemDeviceDetailsManager instance(
      kDefaultUploadDeviceDetailsRequestTimeout);
  static GemDeviceDetailsManager* instance_storage = &instance;
  return &instance_storage;
}

// static
extension::TaskCreator
GemDeviceDetailsManager::UploadDeviceDetailsTaskCreator() {
  return base::BindRepeating(&UploadDeviceDetailsTask::Create);
}

GemDeviceDetailsManager::GemDeviceDetailsManager(
    base::TimeDelta upload_device_details_request_timeout)
    : upload_device_details_request_timeout_(
          upload_device_details_request_timeout) {
  g_upload_device_details_from_esa_enabled =
      GetGlobalFlagOrDefault(kUploadDeviceDetailsFromEsaEnabledRegKey, 1) == 1;
}

GemDeviceDetailsManager::~GemDeviceDetailsManager() = default;

GURL GemDeviceDetailsManager::GetGemServiceUploadDeviceDetailsUrl() {
  GURL gem_service_url = GetGcpwServiceUrl();
  return gem_service_url.Resolve(kGemServiceUploadDeviceDetailsPath);
}

bool GemDeviceDetailsManager::UploadDeviceDetailsFromEsaFeatureEnabled() const {
  return g_upload_device_details_from_esa_enabled;
}

// Uploads the device details into GEM database using |dm_token|
// for authentication and authorization. The GEM service would use
// |serial_number| and |machine_guid| for identifying the device
// entry in GEM database.
HRESULT GemDeviceDetailsManager::UploadDeviceDetails(
    const extension::UserDeviceContext& context) {
  std::wstring obfuscated_user_id;
  HRESULT status = GetIdFromSid(context.user_sid.c_str(), &obfuscated_user_id);
  if (FAILED(status)) {
    LOGFN(ERROR) << "Could not get user id from sid " << context.user_sid;
    return status;
  }

  wchar_t found_username[kWindowsUsernameBufferLength] = {};
  wchar_t found_domain[kWindowsDomainBufferLength] = {};

  status = OSUserManager::Get()->FindUserBySidWithFallback(
      context.user_sid.c_str(), found_username, std::size(found_username),
      found_domain, std::size(found_domain));
  if (FAILED(status)) {
    LOGFN(ERROR) << "Could not get username and domain from sid "
                 << context.user_sid;
  }

  return UploadDeviceDetailsInternal(
      /* access_token= */ std::string(), obfuscated_user_id, context.dm_token,
      context.user_sid, context.device_resource_id, found_username,
      found_domain);
}

// Uploads the device details into GEM database using |access_token|
// for authentication and authorization. The GEM service would use
// |serial_number| and |machine_guid| for identifying the device
// entry in GEM database.
HRESULT GemDeviceDetailsManager::UploadDeviceDetails(
    const std::string& access_token,
    const std::wstring& sid,
    const std::wstring& username,
    const std::wstring& domain) {
  return UploadDeviceDetailsInternal(access_token,
                                     /* obfuscated_user_id= */ L"",
                                     /* dm_token= */ L"", sid,
                                     /* device_resource_id= */ L"", username,
                                     domain);
}

HRESULT GemDeviceDetailsManager::UploadDeviceDetailsInternal(
    const std::string access_token,
    const std::wstring obfuscated_user_id,
    const std::wstring dm_token,
    const std::wstring sid,
    const std::wstring device_resource_id,
    const std::wstring username,
    const std::wstring domain) {
  std::wstring serial_number = GetSerialNumber();
  std::wstring machine_guid;
  HRESULT hr = GetMachineGuid(&machine_guid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "Failed fetching machine guid. hr=" << putHR(hr);
    return hr;
  }
  std::vector<std::string> mac_addresses = GetMacAddresses();

  // Get OS version of the windows device.
  std::string version;
  GetOsVersion(&version);

  // Extract built-in administrator and administrator group name
  // in device locale.
  std::wstring admin_group_name = L"";
  hr = LookupLocalizedNameForWellKnownSid(WinBuiltinAdministratorsSid,
                                          &admin_group_name);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "LookupLocalizedNameForWellKnownSid  hr=" << putHR(hr);
    hr = S_OK;
  }

  std::wstring built_in_admin_name = L"";
  hr = GetLocalizedNameBuiltinAdministratorAccount(&built_in_admin_name);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetLocalizedNameBuiltinAdministratorAccount  hr="
                 << putHR(hr);
    hr = S_OK;
  }

  base::Value::List mac_address_value_list;
  for (const std::string& mac_address : mac_addresses)
    mac_address_value_list.Append(mac_address);

  std::wstring dm_token_value = dm_token;
  if (dm_token_value.empty()) {
    hr = GetGCPWDmToken(sid, &dm_token_value);
    if (FAILED(hr)) {
      LOGFN(WARNING) << "Failed to fetch DmToken hr=" << putHR(hr);
      hr = S_OK;
    }
  }

  request_dict_ = std::make_unique<base::Value::Dict>();
  request_dict_->Set(kUploadDeviceDetailsRequestSerialNumberParameterName,
                     base::WideToUTF8(serial_number));
  request_dict_->Set(kUploadDeviceDetailsRequestMachineGuidParameterName,
                     base::WideToUTF8(machine_guid));
  request_dict_->Set(kUploadDeviceDetailsRequestUserSidParameterName,
                     base::WideToUTF8(sid));

  if (!username.empty()) {
    request_dict_->Set(kUploadDeviceDetailsRequestUsernameParameterName,
                       base::WideToUTF8(username));
  }

  if (!domain.empty()) {
    request_dict_->Set(kUploadDeviceDetailsRequestDomainParameterName,
                       base::WideToUTF8(domain));
  }

  request_dict_->Set(kIsAdJoinedUserParameterName,
                     OSUserManager::Get()->IsUserDomainJoined(sid));
  request_dict_->Set(kMacAddressParameterName,
                     std::move(mac_address_value_list));
  request_dict_->Set(kOsVersion, version);
  request_dict_->Set(kBuiltInAdminNameParameterName,
                     base::WideToUTF8(built_in_admin_name));
  request_dict_->Set(kAdminGroupNameParameterName,
                     base::WideToUTF8(admin_group_name));
  request_dict_->Set(kDmToken, base::WideToUTF8(dm_token_value));

  if (!obfuscated_user_id.empty()) {
    request_dict_->Set(kObfuscatedGaiaId, base::WideToUTF8(obfuscated_user_id));
  }

  std::wstring known_resource_id = device_resource_id.empty()
                                       ? GetUserDeviceResourceId(sid)
                                       : device_resource_id;
  if (!known_resource_id.empty()) {
    request_dict_->Set(kUploadDeviceDetailsRequestDeviceResourceIdParameterName,
                       base::WideToUTF8(known_resource_id));
  }

  std::optional<base::Value> request_result;

  hr = WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService(
      GemDeviceDetailsManager::Get()->GetGemServiceUploadDeviceDetailsUrl(),
      access_token, {}, *request_dict_, upload_device_details_request_timeout_,
      kMaxNumHttpRetries, &request_result);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromHttpService hr="
                 << putHR(hr);
    return E_FAIL;
  }

  auto* resource_id = request_result->GetDict().FindString(
      kUploadDeviceDetailsResponseDeviceResourceIdParameterName);
  if (resource_id) {
    hr = SetUserProperty(sid, kRegUserDeviceResourceId,
                         base::UTF8ToWide(*resource_id));
  } else {
    LOGFN(ERROR) << "Server response does not contain "
                 << kUploadDeviceDetailsResponseDeviceResourceIdParameterName;
    hr = E_FAIL;
  }

  return hr;
}

void GemDeviceDetailsManager::
    SetUploadDeviceDetailsFromEsaFeatureEnabledForTesting(bool value) {
  g_upload_device_details_from_esa_enabled = value;
}

}  // namespace credential_provider
