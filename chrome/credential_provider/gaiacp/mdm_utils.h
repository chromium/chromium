// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_MDM_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_MDM_UTILS_H_

#include <string>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/win/windows_types.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "url/gurl.h"

namespace credential_provider {

// Mdm registry value key name.

// Enables verbose logging in GCPW.
extern const wchar_t kRegEnableVerboseLogging[];

// Determines if crash reporting is initialized for credential provider DLL.
extern const wchar_t kRegInitializeCrashReporting[];

// The url used to register the machine to MDM. If specified and non-empty
// additional user access restrictions will be applied to users associated
// to GCPW that have invalid token handles.
extern const wchar_t kRegMdmUrl[];

// The registry entry is used to control whether to enable enrollment
// Google device management solution.
extern const wchar_t kRegEnableDmEnrollment[];

// Disables password escrowing feature in GCPW.
extern const wchar_t kRegDisablePasswordSync[];

// Determines if multiple users can be added to a system managed by MDM.
extern const wchar_t kRegMdmSupportsMultiUser[];

// Allow sign in using normal consumer accounts.
extern const wchar_t kRegMdmAllowConsumerAccounts[];

// Enables force password reset option in forgot password flow.
extern const wchar_t kRegMdmEnableForcePasswordReset[];

// Password lsa store key prefix.
extern const wchar_t kUserPasswordLsaStoreKeyPrefix[];

// Error key name that is likely to be present in HTTP responses.
extern const char kErrorKeyInRequestResult[];

// Upload status for device details.
extern const wchar_t kRegDeviceDetailsUploadStatus[];

// Number of consecutive failures encountered when uploading device details.
extern const wchar_t kRegDeviceDetailsUploadFailures[];

// Specifies custom Chrome path to use for GLS.
extern const wchar_t kRegGlsPath[];

// Registry key where user device resource ID is stored.
extern const wchar_t kRegUserDeviceResourceId[];

// Maximum number of consecutive Upload device details failures for which we do
// enforce auth.
extern const int kMaxNumConsecutiveUploadDeviceFailures;

// The URL part that is used when constructing the developer complete URL. When
// it is empty, developer mode isn't enabled.
extern const wchar_t kRegDeveloperMode[];

// Enables updating credentials on login UI when the enforcement of any GCPW
// associated account changes.
extern const wchar_t kRegUpdateCredentialsOnChange[];

// Maximum allowed time delta after which user policies should be refreshed
// again.
extern const base::TimeDelta kMaxTimeDeltaSinceLastUserPolicyRefresh;

// Registry key that indicates account name for an unassociated Windows account
// should be in shorter form.
extern const wchar_t kRegUseShorterAccountName[];

// Class used in tests to force either a successful on unsuccessful enrollment
// to google MDM.
class GoogleMdmEnrollmentStatusForTesting {
 public:
  explicit GoogleMdmEnrollmentStatusForTesting(bool success);
  ~GoogleMdmEnrollmentStatusForTesting();
};

// Class used in tests to force enrolled status to google MDM.
class GoogleMdmEnrolledStatusForTesting {
 public:
  explicit GoogleMdmEnrolledStatusForTesting(bool success);
  ~GoogleMdmEnrolledStatusForTesting();
};

// Class used in tests to force upload device details needed.
class GoogleUploadDeviceDetailsNeededForTesting {
 public:
  explicit GoogleUploadDeviceDetailsNeededForTesting(bool success);
  ~GoogleUploadDeviceDetailsNeededForTesting();
};

// This function returns true if the user identified by |sid| is allowed to
// enroll with MDM and the device is not currently enrolled with the MDM server
// specified in |kGlobalMdmUrlRegKey|.
bool NeedsToEnrollWithMdm(const base::string16& sid);

// Checks user properties to determine whether last upload device details
// attempt succeeded for the given user.
bool UploadDeviceDetailsNeeded(const base::string16& sid);

// Checks whether the |kRegMdmUrl| is set on this machine and points
// to a valid URL. Returns false otherwise.
bool MdmEnrollmentEnabled();

// Get the URL used to enroll with MDM.
base::string16 GetMdmUrl();

// Checks whether the |kRegEscrowServiceServerUrl| is not empty on this
// machine.
bool PasswordRecoveryEnabled();

// Returns true if the |kKeyEnableGemFeatures| is set to 1.
bool IsGemEnabled();

// Checks if online login is enforced. Returns true if
// |kRegMdmEnforceOnlineLogin| is set to true at global or user level.
bool IsOnlineLoginEnforced(const base::string16& sid);

// Gets the escrow service URL unless password sync is disabled. Otherwise an
// empty url is returned.
GURL EscrowServiceUrl();

// Gets the gcpw service URL.
GURL GetGcpwServiceUrl();

// Enrolls the machine to with the Google MDM server if not already.
HRESULT EnrollToGoogleMdmIfNeeded(const base::Value& properties);

// Constructs the password lsa store key for the given |sid|.
base::string16 GetUserPasswordLsaStoreKey(const base::string16& sid);

// Get device resource ID for the user with given |sid|. Returns an empty string
// if one has not been set for the user.
base::string16 GetUserDeviceResourceId(const base::string16& sid);

// Converts the |url| in the form of http://xxxxx.googleapis.com/...
// to a form that points to a development URL as specified with |dev|
// environment. Final url will be in the form
// https://{dev}-xxxxx.sandbox.googleapis.com/...
base::string16 GetDevelopmentUrl(const base::string16& url,
                                 const base::string16& dev);

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_MDM_UTILS_H_
