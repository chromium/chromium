// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_MDM_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_MDM_UTILS_H_

#include <string>

#include "base/values.h"
#include "base/win/windows_types.h"
#include "url/gurl.h"

namespace credential_provider {

// Password lsa store key prefix.
extern const wchar_t kUserPasswordLsaStoreKeyPrefix[];

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
bool NeedsToEnrollWithMdm(const std::wstring& sid);

// Checks user properties to determine whether last upload device details
// attempt succeeded for the given user.
bool UploadDeviceDetailsNeeded(const std::wstring& sid);

// Checks whether the |kRegMdmUrl| is set on this machine and points
// to a valid URL. Returns false otherwise.
bool MdmEnrollmentEnabled();

// Get the URL used to enroll with MDM.
std::wstring GetMdmUrl();

// Checks whether the |kRegEscrowServiceServerUrl| is not empty on this
// machine.
bool PasswordRecoveryEnabled();

// Returns true if the |kKeyEnableGemFeatures| is set to 1.
bool IsGemEnabled();

// Checks if online login is enforced. Returns true if
// |kRegMdmEnforceOnlineLogin| is set to true at global or user level.
bool IsOnlineLoginEnforced(const std::wstring& sid);

// Gets the escrow service URL unless password sync is disabled. Otherwise an
// empty url is returned.
GURL EscrowServiceUrl();

// Enrolls the machine to with the Google MDM server if not already.
HRESULT EnrollToGoogleMdmIfNeeded(const base::Value::Dict& properties);

// Constructs the password lsa store key for the given |sid|.
std::wstring GetUserPasswordLsaStoreKey(const std::wstring& sid);

// Returns true if the device is enrolled with Google MDM.
bool IsEnrolledWithGoogleMdm();
}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_MDM_UTILS_H_
