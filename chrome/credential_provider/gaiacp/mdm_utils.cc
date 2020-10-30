// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/mdm_utils.h"

#include <windows.h>
#include <winternl.h>
#include <lm.h>  // Needed for PNTSTATUS

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <MDMRegistration.h>  // For RegisterDeviceWithManagement()
#include <ntsecapi.h>         // For LsaQueryInformationPolicy()

#include <atlconv.h>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "base/win/wmi.h"
#include "build/branding_buildflags.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/device_policies_manager.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/user_policies_manager.h"

namespace credential_provider {

constexpr wchar_t kRegMdmEnforceOnlineLogin[] = L"enforce_online_login";
constexpr wchar_t kUserPasswordLsaStoreKeyPrefix[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    L"Chrome-GCPW-";
#else
    L"Chromium-GCPW-";
#endif

// Overridden in tests to force the MDM enrollment to either succeed or fail.
enum class EnrollmentStatus {
  kForceSuccess,
  kForceFailure,
  kDontForce,
};
EnrollmentStatus g_enrollment_status = EnrollmentStatus::kDontForce;

// Overridden in tests to force the MDM enrollment check to either return true
// or false.
enum class EnrolledStatus {
  kForceTrue,
  kForceFalse,
  kDontForce,
};
EnrolledStatus g_enrolled_status = EnrolledStatus::kDontForce;

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
enum class EscrowServiceStatus {
  kDisabled,
  kEnabled,
};

EscrowServiceStatus g_escrow_service_enabled = EscrowServiceStatus::kDisabled;
#endif

enum class DeviceDetailsUploadNeeded {
  kForceTrue,
  kForceFalse,
  kDontForce,
};
DeviceDetailsUploadNeeded g_device_details_upload_needed =
    DeviceDetailsUploadNeeded::kDontForce;

namespace {

constexpr wchar_t kDefaultEscrowServiceServerUrl[] =
    L"https://devicepasswordescrowforwindows-pa.googleapis.com";

template <typename T>
T GetMdmFunctionPointer(const base::ScopedNativeLibrary& library,
                        const char* function_name) {
  if (!library.is_valid())
    return nullptr;

  return reinterpret_cast<T>(library.GetFunctionPointer(function_name));
}

#define GET_MDM_FUNCTION_POINTER(library, name) \
  GetMdmFunctionPointer<decltype(&::name)>(library, #name)

bool IsEnrolledWithGoogleMdm(const base::string16& mdm_url) {
  switch (g_enrolled_status) {
    case EnrolledStatus::kForceTrue:
      return true;
    case EnrolledStatus::kForceFalse:
      return false;
    case EnrolledStatus::kDontForce:
      break;
  }

  base::ScopedNativeLibrary library(
      base::FilePath(FILE_PATH_LITERAL("MDMRegistration.dll")));
  auto get_device_registration_info_function =
      GET_MDM_FUNCTION_POINTER(library, GetDeviceRegistrationInfo);
  if (!get_device_registration_info_function) {
    // On Windows < 1803 the function GetDeviceRegistrationInfo does not exist
    // in MDMRegistration.dll so we have to fallback to the less accurate
    // IsDeviceRegisteredWithManagement. This can return false positives if the
    // machine is registered to MDM but to a different server.
    LOGFN(ERROR) << "GET_MDM_FUNCTION_POINTER(GetDeviceRegistrationInfo)";
    auto is_device_registered_with_management_function =
        GET_MDM_FUNCTION_POINTER(library, IsDeviceRegisteredWithManagement);
    if (!is_device_registered_with_management_function) {
      LOGFN(ERROR)
          << "GET_MDM_FUNCTION_POINTER(IsDeviceRegisteredWithManagement)";
      return false;
    } else {
      BOOL is_managed = FALSE;
      HRESULT hr = is_device_registered_with_management_function(&is_managed, 0,
                                                                 nullptr);
      return SUCCEEDED(hr) && is_managed;
    }
  }

  MANAGEMENT_REGISTRATION_INFO* info;
  HRESULT hr = get_device_registration_info_function(
      DeviceRegistrationBasicInfo, reinterpret_cast<void**>(&info));

  bool is_enrolled = SUCCEEDED(hr) && info->fDeviceRegisteredWithManagement &&
                     GURL(mdm_url) == GURL(info->pszMDMServiceUri);

  if (SUCCEEDED(hr))
    ::HeapFree(::GetProcessHeap(), 0, info);
  return is_enrolled;
}

HRESULT ExtractRegistrationData(const base::Value& registration_data,
                                base::string16* out_email,
                                base::string16* out_id_token,
                                base::string16* out_access_token,
                                base::string16* out_sid,
                                base::string16* out_username,
                                base::string16* out_domain,
                                base::string16* out_is_ad_user_joined) {
  DCHECK(out_email);
  DCHECK(out_id_token);
  DCHECK(out_access_token);
  DCHECK(out_sid);
  DCHECK(out_username);
  DCHECK(out_domain);
  DCHECK(out_is_ad_user_joined);
  if (!registration_data.is_dict()) {
    LOGFN(ERROR) << "Registration data is not a dictionary";
    return E_INVALIDARG;
  }

  *out_email = GetDictString(registration_data, kKeyEmail);
  *out_id_token = GetDictString(registration_data, kKeyMdmIdToken);
  *out_access_token = GetDictString(registration_data, kKeyAccessToken);
  *out_sid = GetDictString(registration_data, kKeySID);
  *out_username = GetDictString(registration_data, kKeyUsername);
  *out_domain = GetDictString(registration_data, kKeyDomain);
  *out_is_ad_user_joined = GetDictString(registration_data, kKeyIsAdJoinedUser);

  if (out_email->empty()) {
    LOGFN(ERROR) << "Email is empty";
    return E_INVALIDARG;
  }

  if (out_id_token->empty()) {
    LOGFN(ERROR) << "MDM id token is empty";
    return E_INVALIDARG;
  }

  if (out_access_token->empty()) {
    LOGFN(ERROR) << "Access token is empty";
    return E_INVALIDARG;
  }

  if (out_sid->empty()) {
    LOGFN(ERROR) << "SID is empty";
    return E_INVALIDARG;
  }

  if (out_username->empty()) {
    LOGFN(ERROR) << "username is empty";
    return E_INVALIDARG;
  }

  if (out_domain->empty()) {
    LOGFN(ERROR) << "domain is empty";
    return E_INVALIDARG;
  }

  if (out_is_ad_user_joined->empty()) {
    LOGFN(ERROR) << "is_ad_user_joined is empty";
    return E_INVALIDARG;
  }

  return S_OK;
}

HRESULT RegisterWithGoogleDeviceManagement(const base::string16& mdm_url,
                                           const base::Value& properties) {
  // Make sure all the needed data is present in the dictionary.
  base::string16 email;
  base::string16 id_token;
  base::string16 access_token;
  base::string16 sid;
  base::string16 username;
  base::string16 domain;
  base::string16 is_ad_joined_user;

  HRESULT hr =
      ExtractRegistrationData(properties, &email, &id_token, &access_token,
                              &sid, &username, &domain, &is_ad_joined_user);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "ExtractRegistrationData hr=" << putHR(hr);
    return E_INVALIDARG;
  }

  LOGFN(INFO) << "MDM_URL=" << mdm_url
              << " token=" << base::string16(id_token.c_str(), 10);

  // Add the serial number to the registration data dictionary.
  base::string16 serial_number = GetSerialNumber();

  if (serial_number.empty()) {
    LOGFN(ERROR) << "Failed to get serial number.";
    return E_FAIL;
  }

  // Add machine_guid to the registration data dictionary.
  base::string16 machine_guid;
  hr = GetMachineGuid(&machine_guid);

  if (FAILED(hr) || machine_guid.empty()) {
    LOGFN(ERROR) << "Failed to get machine guid.";
    return FAILED(hr) ? hr : E_FAIL;
  }

  // Need localized local user group name for Administrators group
  // for supporting account elevation scenarios.
  base::string16 local_administrators_group_name;
  hr = LookupLocalizedNameForWellKnownSid(WinBuiltinAdministratorsSid,
                                          &local_administrators_group_name);
  if (FAILED(hr)) {
    LOGFN(WARNING) << "Failed to fetch name for administrators group";
  }

  base::string16 builtin_administrator_name;
  hr = GetLocalizedNameBuiltinAdministratorAccount(&builtin_administrator_name);
  if (FAILED(hr)) {
    LOGFN(WARNING) << "Failed to fetch name for builtin administrator account";
  }

  // Build the json data needed by the server.
  base::Value registration_data(base::Value::Type::DICTIONARY);
  registration_data.SetStringKey("id_token", id_token);
  registration_data.SetStringKey("access_token", access_token);
  registration_data.SetStringKey("sid", sid);
  registration_data.SetStringKey("username", username);
  registration_data.SetStringKey("domain", domain);
  registration_data.SetStringKey("serial_number", serial_number);
  registration_data.SetStringKey("machine_guid", machine_guid);
  registration_data.SetStringKey("admin_local_user_group_name",
                                 local_administrators_group_name);
  registration_data.SetStringKey("builtin_administrator_name",
                                 builtin_administrator_name);
  registration_data.SetStringKey(kKeyIsAdJoinedUser, is_ad_joined_user);

  // Send device resource ID if available as part of the enrollment payload.
  // Enrollment backend should not assume that this will always be available.
  base::string16 user_device_resource_id = GetUserDeviceResourceId(sid);
  if (!user_device_resource_id.empty()) {
    registration_data.SetStringKey("resource_id", user_device_resource_id);
  }

  std::string registration_data_str;
  if (!base::JSONWriter::Write(registration_data, &registration_data_str)) {
    LOGFN(ERROR) << "JSONWriter::Write(registration_data)";
    return E_FAIL;
  }

  switch (g_enrollment_status) {
    case EnrollmentStatus::kForceSuccess:
      return S_OK;
    case EnrollmentStatus::kForceFailure:
      return E_FAIL;
    case EnrollmentStatus::kDontForce:
      break;
  }

  base::ScopedNativeLibrary library(
      base::FilePath(FILE_PATH_LITERAL("MDMRegistration.dll")));
  auto register_device_with_management_function =
      GET_MDM_FUNCTION_POINTER(library, RegisterDeviceWithManagement);
  if (!register_device_with_management_function) {
    LOGFN(ERROR) << "GET_MDM_FUNCTION_POINTER(RegisterDeviceWithManagement)";
    return false;
  }

  std::string data_encoded;
  base::Base64Encode(registration_data_str, &data_encoded);

  // This register call is blocking.  It won't return until the machine is
  // properly registered with the MDM server.
  return register_device_with_management_function(
      email.c_str(), mdm_url.c_str(), base::UTF8ToWide(data_encoded).c_str());
}

bool IsUserAllowedToEnrollWithMdm(const base::string16& sid) {
  UserPolicies policies;
  UserPoliciesManager::Get()->GetUserPolicies(sid, &policies);
  return policies.enable_dm_enrollment;
}

}  // namespace

bool NeedsToEnrollWithMdm(const base::string16& sid) {
  if (UserPoliciesManager::Get()->CloudPoliciesEnabled()) {
    if (!IsUserAllowedToEnrollWithMdm(sid))
      return false;
  }

  base::string16 mdm_url = GetMdmUrl();
  return !mdm_url.empty() && !IsEnrolledWithGoogleMdm(mdm_url);
}

bool UploadDeviceDetailsNeeded(const base::string16& sid) {
  switch (g_device_details_upload_needed) {
    case DeviceDetailsUploadNeeded::kForceTrue:
      return true;
    case DeviceDetailsUploadNeeded::kForceFalse:
      return false;
    case DeviceDetailsUploadNeeded::kDontForce:
      break;
  }

  DWORD status = 0;
  GetUserProperty(sid, kRegDeviceDetailsUploadStatus, &status);

  // GCPW token is required for ESA to communicate with the GEM backends. So
  // enforce upload if this token is missing.
  base::string16 gcpw_token;
  HRESULT hr = GetGCPWDmToken(sid, &gcpw_token);
  bool gcpw_token_upload_required = false;
  if (UserPoliciesManager::Get()->CloudPoliciesEnabled() && FAILED(hr)) {
    gcpw_token_upload_required = true;
  }

  if (status != 1 || gcpw_token_upload_required) {
    DWORD device_upload_failures = 1;
    GetUserProperty(sid, kRegDeviceDetailsUploadFailures,
                    &device_upload_failures);
    if (device_upload_failures >
        DWORD(kMaxNumConsecutiveUploadDeviceFailures)) {
      LOGFN(WARNING) << "Reauth not enforced due to upload device details "
                        "failures exceeding threshhold.";
      return false;
    }
    return true;
  }
  return false;
}

bool MdmEnrollmentEnabled() {
  if (DevicePoliciesManager::Get()->CloudPoliciesEnabled()) {
    DevicePolicies policies;
    DevicePoliciesManager::Get()->GetDevicePolicies(&policies);
    return policies.enable_dm_enrollment;
  }

  base::string16 mdm_url = GetMdmUrl();
  return !mdm_url.empty();
}

base::string16 GetMdmUrl() {
  base::string16 enrollment_url = L"";

  if (UserPoliciesManager::Get()->CloudPoliciesEnabled()) {
    enrollment_url = GetGlobalFlagOrDefault(kRegMdmUrl, kDefaultMdmUrl);
  } else {
    DWORD enable_dm_enrollment;
    HRESULT hr = GetGlobalFlag(kRegEnableDmEnrollment, &enable_dm_enrollment);
    if (SUCCEEDED(hr)) {
      if (enable_dm_enrollment)
        enrollment_url = kDefaultMdmUrl;
    } else {
      // Fallback to using the older flag to control mdm url.
      enrollment_url = GetGlobalFlagOrDefault(kRegMdmUrl, kDefaultMdmUrl);
    }
  }

  base::string16 dev = GetGlobalFlagOrDefault(kRegDeveloperMode, L"");
  if (!dev.empty())
    enrollment_url = GetDevelopmentUrl(enrollment_url, dev);

  return enrollment_url;
}

GURL EscrowServiceUrl() {
  DWORD disable_password_sync =
      GetGlobalFlagOrDefault(kRegDisablePasswordSync, 0);
  if (disable_password_sync)
    return GURL();

  base::string16 dev = GetGlobalFlagOrDefault(kRegDeveloperMode, L"");

  if (!dev.empty())
    return GURL(GetDevelopmentUrl(kDefaultEscrowServiceServerUrl, dev));

  // By default, the password recovery feature should be enabled.
  return GURL(base::UTF16ToUTF8(kDefaultEscrowServiceServerUrl));
}

bool PasswordRecoveryEnabled() {
  return !EscrowServiceUrl().is_empty();
}

bool IsGemEnabled() {
  // The gem features are enabled by default.
  return GetGlobalFlagOrDefault(kKeyEnableGemFeatures, 1);
}

bool IsOnlineLoginEnforced(const base::string16& sid) {
  DWORD global_flag = GetGlobalFlagOrDefault(kRegMdmEnforceOnlineLogin, 0);

  // Return true if global flag is set. If it is not set check for
  // the user flag.
  if (global_flag)
    return true;


  DWORD is_online_login_enforced_for_user = 0;
  HRESULT hr = GetUserProperty(sid, kRegMdmEnforceOnlineLogin,
                       &is_online_login_enforced_for_user);

  if (FAILED(hr)) {
    LOGFN(VERBOSE) << "GetUserProperty for " << kRegMdmEnforceOnlineLogin
                << " failed. hr=" << putHR(hr);
    // Fallback to the less obstructive option to not enforce login via google
    // when fetching the registry entry fails.
    return false;
  }

  return is_online_login_enforced_for_user;
}

HRESULT EnrollToGoogleMdmIfNeeded(const base::Value& properties) {
  LOGFN(VERBOSE);

  if (UserPoliciesManager::Get()->CloudPoliciesEnabled()) {
    base::string16 sid = GetDictString(properties, kKeySID);
    if (!IsUserAllowedToEnrollWithMdm(sid))
      return S_OK;
  }

  // Only enroll with MDM if configured.
  base::string16 mdm_url = GetMdmUrl();
  if (mdm_url.empty())
    return S_OK;

  // TODO(crbug.com/935577): Check if machine is already enrolled because
  // attempting to enroll when already enrolled causes a crash.
  if (IsEnrolledWithGoogleMdm(mdm_url)) {
    LOGFN(VERBOSE) << "Already enrolled to Google MDM";
    return S_OK;
  }

  HRESULT hr = RegisterWithGoogleDeviceManagement(mdm_url, properties);
  if (FAILED(hr))
    LOGFN(ERROR) << "RegisterWithGoogleDeviceManagement hr=" << putHR(hr);
  return hr;
}

base::string16 GetUserPasswordLsaStoreKey(const base::string16& sid) {
  DCHECK(sid.size());

  return kUserPasswordLsaStoreKeyPrefix + sid;
}

// GoogleMdmEnrollmentStatusForTesting ////////////////////////////////////////

GoogleMdmEnrollmentStatusForTesting::GoogleMdmEnrollmentStatusForTesting(
    bool success) {
  g_enrollment_status = success ? EnrollmentStatus::kForceSuccess
                                : EnrollmentStatus::kForceFailure;
}

GoogleMdmEnrollmentStatusForTesting::~GoogleMdmEnrollmentStatusForTesting() {
  g_enrollment_status = EnrollmentStatus::kDontForce;
}

// GoogleMdmEnrolledStatusForTesting //////////////////////////////////////////

GoogleMdmEnrolledStatusForTesting::GoogleMdmEnrolledStatusForTesting(
    bool success) {
  g_enrolled_status =
      success ? EnrolledStatus::kForceTrue : EnrolledStatus::kForceFalse;
}

GoogleMdmEnrolledStatusForTesting::~GoogleMdmEnrolledStatusForTesting() {
  g_enrolled_status = EnrolledStatus::kDontForce;
}

// GoogleMdmEnrolledStatusForTesting //////////////////////////////////////////

// GoogleUploadDeviceDetailsNeededForTesting //////////////////////////////////

GoogleUploadDeviceDetailsNeededForTesting::
    GoogleUploadDeviceDetailsNeededForTesting(bool success) {
  g_device_details_upload_needed = success
                                       ? DeviceDetailsUploadNeeded::kForceTrue
                                       : DeviceDetailsUploadNeeded::kForceFalse;
}

GoogleUploadDeviceDetailsNeededForTesting::
    ~GoogleUploadDeviceDetailsNeededForTesting() {
  g_device_details_upload_needed = DeviceDetailsUploadNeeded::kDontForce;
}

// GoogleUploadDeviceDetailsNeededForTesting //////////////////////////////////
}  // namespace credential_provider
