// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_MDM_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_MDM_UTILS_H_

#include <string>

#include "base/strings/string16.h"
#include "base/values.h"
#include "base/win/windows_types.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "url/gurl.h"

namespace credential_provider {

// Mdm registry value key name.

// The url used to register the machine to MDM. If specified and non-empty
// additional user access restrictions will be applied to users associated
// to GCPW that have invalid token handles.
extern const wchar_t kRegMdmUrl[];

// Base server url for the password recovery escrow service.
extern const wchar_t kRegEscrowServiceServerUrl[];

// Determines if multiple users can be added to a system managed by MDM.
extern const wchar_t kRegMdmSupportsMultiUser[];

// Allow sign in using normal consumer accounts.
extern const wchar_t kRegMdmAllowConsumerAccounts[];

// Enables force password reset option in forgot password flow.
extern const wchar_t kRegMdmEnableForcePasswordReset[];

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

// Class used in tests to set registration data for testing.
class GoogleRegistrationDataForTesting {
 public:
  explicit GoogleRegistrationDataForTesting(base::string16 serial_number);
  ~GoogleRegistrationDataForTesting();
};

// Class used in tests to force password escrow service availability when not
// in a Google Chrome build (where the service is disabled).
class GoogleMdmEscrowServiceEnablerForTesting {
 public:
  GoogleMdmEscrowServiceEnablerForTesting();
  ~GoogleMdmEscrowServiceEnablerForTesting();
};

// If MdmEnrollmentEnabled returns true, this function verifies that the machine
// is enrolled to MDM AND that the server to which it is enrolled is the same
// as the one specified in |kGlobalMdmUrlRegKey|, otherwise returns false.
bool NeedsToEnrollWithMdm();

// Checks whether the |kRegMdmUrl| is set on this machine and points
// to a valid URL. Returns false otherwise.
bool MdmEnrollmentEnabled();

// Checks whether the |kRegEscrowServiceServerUrl| is not empty on this
// machine.
bool PasswordRecoveryEnabled();

// Gets the escrow service URL as defined in the registry or a default value if
// nothing is set.
GURL EscrowServiceUrl();

// Enrolls the machine to with the Google MDM server if not already.
HRESULT EnrollToGoogleMdmIfNeeded(const base::Value& properties);

// Constructs the password lsa store key for the given |sid|.
base::string16 GetUserPasswordLsaStoreKey(const base::string16& sid);

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_MDM_UTILS_H_
