// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/mdm_utils.h"

#include <windows.h>

#include <lm.h>  // Needed for PNTSTATUS
#include <winternl.h>

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <MDMRegistration.h>  // For RegisterDeviceWithManagement()
#include <ntsecapi.h>         // For LsaQueryInformationPolicy()

#include <atlconv.h>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/scoped_native_library.h"
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

bool IsEnrolledWithGoogleMdm(const std::wstring& mdm_url) {
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
                     GURL(base::AsStringPiece16(mdm_url)) ==
                         GURL(base::AsStringPiece16(info->pszMDMServiceUri));

  if (SUCCEEDED(hr))
    ::HeapFree(::GetProcessHeap(), 0, info);
  return is_enrolled;
}

HRESULT ExtractRegistrationData(const base::Value::Dict& registration_data,
                                std::wstring* out_email,
                                std::wstring* out_id_token,
                                std::wstring* out_access_token,
                                std::wstring* out_sid,
                                std::wstring* out_username,
                                std::wstring* out_domain,
                                std::wstring* out_is_ad_user_joined) {
  DCHECK(out_email);
  DCHECK(out_id_token);
  DCHECK(out_access_token);
  DCHECK(out_sid);
  DCHECK(out_username);
  DCHECK(out_domain);
  DCHECK(out_is_ad_user_joined);

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

HRESULT RegisterWithGoogleDeviceManagement(
    const std::wstring& mdm_url,
    const base::Value::Dict& properties) {
  // Make sure all the needed data is present in the dictionary.
  std::wstring email;
  std::wstring id_token;
  std::wstring access_token;
  std::wstring sid;
  std::wstring username;
  std::wstring domain;
  std::wstring is_ad_joined_user;

  HRESULT hr =
      ExtractRegistrationData(properties, &email, &id_token, &access_token,
                              &sid, &username, &domain, &is_ad_joined_user);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "ExtractRegistrationData hr=" << putHR(hr);
    return E_INVALIDARG;
  }

  LOGFN(INFO) << "MDM_URL=" << mdm_url
              << " token=" << std::wstring(id_token.c_str(), 10);

  // Add the serial number to the registration data dictionary.
  std::wstring serial_number = GetSerialNumber();

  if (serial_number.empty()) {
    LOGFN(ERROR) << "Failed to get serial number.";
    return E_FAIL;
  }

  // Add machine_guid to the registration data dictionary.
  std::wstring machine_guid;
  hr = GetMachineGuid(&machine_guid);

  if (FAILED(hr) || machine_guid.empty()) {
    LOGFN(ERROR) << "Failed to get machine guid.";
    return FAILED(hr) ? hr : E_FAIL;
  }

  // Need localized local user group name for Administrators group
  // for supporting account elevation scenarios.
  std::wstring local_administrators_group_name;
  hr = LookupLocalizedNameForWellKnownSid(WinBuiltinAdministratorsSid,
                                          &local_administrators_group_name);
  if (FAILED(hr)) {
    LOGFN(WARNING) << "Failed to fetch name for administrators group";
  }

  std::wstring builtin_administrator_name;
  hr = GetLocalizedNameBuiltinAdministratorAccount(&builtin_administrator_name);
  if (FAILED(hr)) {
    LOGFN(WARNING) << "Failed to fetch name for builtin administrator account";
  }

  // Build the json data needed by the server.
  auto registration_data =
      base::Value::Dict()
          .Set("id_token", base::WideToUTF8(id_token))
          .Set("access_token", base::WideToUTF8(access_token))
          .Set("sid", base::WideToUTF8(sid))
          .Set("username", base::WideToUTF8(username))
          .Set("domain", base::WideToUTF8(domain))
          .Set("serial_number", base::WideToUTF8(serial_number))
          .Set("machine_guid", base::WideToUTF8(machine_guid))
          .Set("admin_local_user_group_name",
               base::WideToUTF8(local_administrators_group_name))
          .Set("builtin_administrator_name",
               base::WideToUTF8(builtin_administrator_name))
          .Set(kKeyIsAdJoinedUser, base::WideToUTF8(is_ad_joined_user));

  // Send device resource ID if available as part of the enrollment payload.
  // Enrollment backend should not assume that this will always be available.
  std::wstring user_device_resource_id = GetUserDeviceResourceId(sid);
  if (!user_device_resource_id.empty()) {
    registration_data.Set("resource_id",
                          base::WideToUTF8(user_device_resource_id));
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

  std::string data_encoded = base::Base64Encode(registration_data_str);

  // This register call is blocking.  It won't return until the machine is
  // properly registered with the MDM server.
  return register_device_with_management_function(
      email.c_str(), mdm_url.c_str(), base::UTF8ToWide(data_encoded).c_str());
}

bool IsUserAllowedToEnrollWithMdm(const std::wstring& sid) {
  UserPolicies policies;
  UserPoliciesManager::Get()->GetUserPolicies(sid, &policies);
  return policies.enable_dm_enrollment;
}

}  // namespace

bool NeedsToEnrollWithMdm(const std::wstring& sid) {
  if (UserPoliciesManager::Get()->CloudPoliciesEnabled()) {
    if (!IsUserAllowedToEnrollWithMdm(sid))
      return false;
  }

  std::wstring mdm_url = GetMdmUrl();
  return !mdm_url.empty() && !IsEnrolledWithGoogleMdm(mdm_url);
}

bool UploadDeviceDetailsNeeded(const std::wstring& sid) {
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
  std::wstring gcpw_token;
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

  std::wstring mdm_url = GetMdmUrl();
  return !mdm_url.empty();
}

std::wstring GetMdmUrl() {
  std::wstring enrollment_url = L"";

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

  std::wstring dev = GetGlobalFlagOrDefault(kRegDeveloperMode, L"");
  if (!dev.empty())
    enrollment_url = GetDevelopmentUrl(enrollment_url, dev);

  return enrollment_url;
}

GURL EscrowServiceUrl() {
  DWORD disable_password_sync =
      GetGlobalFlagOrDefault(kRegDisablePasswordSync, 0);
  if (disable_password_sync)
    return GURL();

  std::wstring dev = GetGlobalFlagOrDefault(kRegDeveloperMode, L"");

  if (!dev.empty())
    return GURL(base::AsStringPiece16(
        GetDevelopmentUrl(kDefaultEscrowServiceServerUrl, dev)));

  // By default, the password recovery feature should be enabled.
  return GURL(base::WideToUTF8(kDefaultEscrowServiceServerUrl));
}

bool PasswordRecoveryEnabled() {
  return !EscrowServiceUrl().is_empty();
}

bool IsGemEnabled() {
  // The gem features are enabled by default.
  return GetGlobalFlagOrDefault(kKeyEnableGemFeatures, 1);
}

bool IsOnlineLoginEnforced(const std::wstring& sid) {
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

HRESULT EnrollToGoogleMdmIfNeeded(const base::Value::Dict& properties) {
  LOGFN(VERBOSE);

  if (UserPoliciesManager::Get()->CloudPoliciesEnabled()) {
    std::wstring sid = GetDictString(properties, kKeySID);
    if (!IsUserAllowedToEnrollWithMdm(sid))
      return S_OK;
  }

  // Only enroll with MDM if configured.
  std::wstring mdm_url = GetMdmUrl();
  if (mdm_url.empty())
    return S_OK;

  // TODO(crbug.com/41443432): Check if machine is already enrolled because
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

bool IsEnrolledWithGoogleMdm() {
  std::wstring mdm_url = GetMdmUrl();
  return !mdm_url.empty() && IsEnrolledWithGoogleMdm(mdm_url);
}

std::wstring GetUserPasswordLsaStoreKey(const std::wstring& sid) {
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
