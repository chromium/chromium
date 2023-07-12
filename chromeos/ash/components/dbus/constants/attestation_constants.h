// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CONSTANTS_ATTESTATION_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CONSTANTS_ATTESTATION_CONSTANTS_H_

#include "base/component_export.h"

namespace ash::attestation {

enum VerifiedAccessType {
  DEFAULT_VA,  // The default Verified Access server.
  TEST_VA,     // The test Verified Access server.
};

// Key types supported by the Chrome OS attestation subsystem.
enum AttestationKeyType {
  // The key will be associated with the device itself and will be available
  // regardless of which user is signed-in.
  KEY_DEVICE,
  // The key will be associated with the current user and will only be available
  // when that user is signed-in.
  KEY_USER,
};

// Options available for customizing an attestation challenge response.
enum AttestationChallengeOptions {
  CHALLENGE_OPTION_NONE = 0,
  // Indicates that a SignedPublicKeyAndChallenge should be embedded in the
  // challenge response.
  CHALLENGE_INCLUDE_SIGNED_PUBLIC_KEY = 1,
};

// Available attestation certificate profiles. These values are sent straight
// to cryptohomed and therefore match the values of CertificateProfile in
// platform2/cryptohome/attestation.proto for the right certificates to be
// returned.
enum AttestationCertificateProfile {
  // Uses the following certificate options:
  //   CERTIFICATE_INCLUDE_STABLE_ID
  //   CERTIFICATE_INCLUDE_DEVICE_STATE
  PROFILE_ENTERPRISE_MACHINE_CERTIFICATE = 0,
  // Uses the following certificate options:
  //   CERTIFICATE_INCLUDE_DEVICE_STATE
  PROFILE_ENTERPRISE_USER_CERTIFICATE = 1,
  // A profile for certificates intended for protected content providers.
  PROFILE_CONTENT_PROTECTION_CERTIFICATE = 2,
  // A profile for certificates intended for enterprise registration.
  PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE = 7,
  // A profile for certificates intended for local authorities which are
  // used to bind software keys.
  PROFILE_SOFT_BIND_CERTIFICATE = 10,
  // A profile for certificates intended for setting up ChromeOS devices using
  // other authenticated devices.
  PROFILE_DEVICE_SETUP_CERTIFICATE = 11,
  // A profile for certificates intended for using the Device Trust Connector on
  // unmanaged devices.
  PROFILE_DEVICE_TRUST_USER_CERTIFICATE = 14,
};

// Status for operations involving an attestation server.
enum AttestationStatus {
  // Call successful
  ATTESTATION_SUCCESS,
  // Failure, no specific reason
  ATTESTATION_UNSPECIFIED_FAILURE,
  // Failure, sending a bad request to an attestation server
  ATTESTATION_SERVER_BAD_REQUEST_FAILURE,
  // Failure, attestation is not supported on this device
  ATTESTATION_NOT_AVAILABLE
};

enum PrivacyCAType {
  DEFAULT_PCA,  // The Google-operated Privacy CA.
  TEST_PCA,     // The test version of the Google-operated Privacy CA.
};

// A key name for the Enterprise Machine Key.  This key should always be stored
// as a DEVICE_KEY.
COMPONENT_EXPORT(ASH_DBUS_CONSTANTS)
extern const char kEnterpriseMachineKey[];

// A key name for the Enterprise Enrollmnent Key.  This key should always be
// stored as a DEVICE_KEY.
COMPONENT_EXPORT(ASH_DBUS_CONSTANTS)
extern const char kEnterpriseEnrollmentKey[];

// A key name for the Enterprise User Key.  This key should always be stored as
// a USER_KEY.
COMPONENT_EXPORT(ASH_DBUS_CONSTANTS)
extern const char kEnterpriseUserKey[];

// The key name prefix for content protection keys.  This prefix must be
// appended with an origin-specific identifier to form the final key name.
COMPONENT_EXPORT(ASH_DBUS_CONSTANTS)
extern const char kContentProtectionKeyPrefix[];

// The key name for the soft bind key. This key should always be stored as a
// USER_KEY.
COMPONENT_EXPORT(ASH_DBUS_CONSTANTS)
extern const char kSoftBindKey[];

// The key name for the device setup certificate. This key should always be
// stored as a DEVICE_KEY.
COMPONENT_EXPORT(ASH_DBUS_CONSTANTS)
extern const char kDeviceSetupKey[];

// The key name prefix for the Device Trust Connector Key. This prefix must be
// appended with an user identifier to form the final key name. This key should
// always be stored as a DEVICE_KEY.
COMPONENT_EXPORT(ASH_DBUS_CONSTANTS)
extern const char kDeviceTrustConnectorKeyPrefix[];

}  // namespace ash::attestation

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CONSTANTS_ATTESTATION_CONSTANTS_H_
