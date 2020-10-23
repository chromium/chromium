// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gem_device_details_manager.h"

#include <windows.h>
#include <winternl.h>

#include <lm.h>  // Needed for LSA_UNICODE_STRING
#include <process.h>

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <ntsecapi.h>  // For POLICY_ALL_ACCESS types

#include "base/containers/span.h"
#include "base/stl_util.h"
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
        base::TimeDelta::FromMilliseconds(12000);

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

// Maximum number of retries if a HTTP call to the backend fails.
constexpr unsigned int kMaxNumHttpRetries = 3;
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

GemDeviceDetailsManager::GemDeviceDetailsManager(
    base::TimeDelta upload_device_details_request_timeout)
    : upload_device_details_request_timeout_(
          upload_device_details_request_timeout) {}

GemDeviceDetailsManager::~GemDeviceDetailsManager() = default;

GURL GemDeviceDetailsManager::GetGemServiceUploadDeviceDetailsUrl() {
  GURL gem_service_url = GetGcpwServiceUrl();

  return gem_service_url.Resolve(kGemServiceUploadDeviceDetailsPath);
}

// Uploads the device details into GEM database using |access_token|
// for authentication and authorization. The GEM service would use
// |serial_number| and |machine_guid| for identifying the device
// entry in GEM database.
HRESULT GemDeviceDetailsManager::UploadDeviceDetails(
    const std::string& access_token,
    const base::string16& sid,
    const base::string16& username,
    const base::string16& domain) {
  base::string16 serial_number = GetSerialNumber();
  base::string16 machine_guid;
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
  base::string16 admin_group_name = L"";
  hr = LookupLocalizedNameForWellKnownSid(WinBuiltinAdministratorsSid,
                                          &admin_group_name);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "LookupLocalizedNameForWellKnownSid  hr=" << putHR(hr);
    hr = S_OK;
  }

  base::string16 built_in_admin_name = L"";
  hr = GetLocalizedNameBuiltinAdministratorAccount(&built_in_admin_name);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetLocalizedNameBuiltinAdministratorAccount  hr="
                 << putHR(hr);
    hr = S_OK;
  }

  base::Value mac_address_value_list(base::Value::Type::LIST);
  for (const std::string& mac_address : mac_addresses)
    mac_address_value_list.Append(base::Value(mac_address));

  base::string16 dm_token;
  hr = GetGCPWDmToken(sid, &dm_token);
  if (FAILED(hr)) {
    LOGFN(WARNING) << "DM token is required to execute periodic tasks hr="
                 << putHR(hr);
    hr = S_OK;
  }

  request_dict_.reset(new base::Value(base::Value::Type::DICTIONARY));
  request_dict_->SetStringKey(
      kUploadDeviceDetailsRequestSerialNumberParameterName,
      base::UTF16ToUTF8(serial_number));
  request_dict_->SetStringKey(
      kUploadDeviceDetailsRequestMachineGuidParameterName,
      base::UTF16ToUTF8(machine_guid));
  request_dict_->SetStringKey(kUploadDeviceDetailsRequestUserSidParameterName,
                              base::UTF16ToUTF8(sid));
  request_dict_->SetStringKey(kUploadDeviceDetailsRequestUsernameParameterName,
                              base::UTF16ToUTF8(username));
  request_dict_->SetStringKey(kUploadDeviceDetailsRequestDomainParameterName,
                              base::UTF16ToUTF8(domain));
  request_dict_->SetBoolKey(kIsAdJoinedUserParameterName,
                            OSUserManager::Get()->IsUserDomainJoined(sid));
  request_dict_->SetKey(kMacAddressParameterName,
                        std::move(mac_address_value_list));
  request_dict_->SetStringKey(kOsVersion, version);
  request_dict_->SetStringKey(kBuiltInAdminNameParameterName,
                              built_in_admin_name);
  request_dict_->SetStringKey(kAdminGroupNameParameterName, admin_group_name);
  request_dict_->SetStringKey(kDmToken, base::UTF16ToUTF8(dm_token));

  base::string16 known_resource_id = GetUserDeviceResourceId(sid);
  if (!known_resource_id.empty()) {
    request_dict_->SetStringKey(
        kUploadDeviceDetailsRequestDeviceResourceIdParameterName,
        base::UTF16ToUTF8(known_resource_id));
  }

  base::Optional<base::Value> request_result;

  hr = WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService(
      GemDeviceDetailsManager::Get()->GetGemServiceUploadDeviceDetailsUrl(),
      access_token, {}, *request_dict_, upload_device_details_request_timeout_,
      kMaxNumHttpRetries, &request_result);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromHttpService hr="
                 << putHR(hr);
    return E_FAIL;
  }

  std::string* resource_id = request_result->FindStringKey(
      kUploadDeviceDetailsResponseDeviceResourceIdParameterName);
  if (resource_id) {
    hr = SetUserProperty(sid, kRegUserDeviceResourceId,
                         base::UTF8ToUTF16(*resource_id));
  } else {
    LOGFN(ERROR) << "Server response does not contain "
                 << kUploadDeviceDetailsResponseDeviceResourceIdParameterName;
    hr = E_FAIL;
  }

  return hr;
}

}  // namespace credential_provider
