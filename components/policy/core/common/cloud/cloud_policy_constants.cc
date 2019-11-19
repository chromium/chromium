// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_constants.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_switches.h"

namespace policy {

// Constants related to the device management protocol.
namespace dm_protocol {

// Name constants for URL query parameters.
const char kParamAgent[] = "agent";
const char kParamAppType[] = "apptype";
const char kParamCritical[] = "critical";
const char kParamDeviceID[] = "deviceid";
const char kParamDeviceType[] = "devicetype";
const char kParamLastError[] = "lasterror";
const char kParamOAuthToken[] = "oauth_token";
const char kParamPlatform[] = "platform";
const char kParamRequest[] = "request";
const char kParamRetry[] = "retry";

// Policy constants used in authorization header.
const char kAuthHeader[] = "Authorization";
const char kServiceTokenAuthHeaderPrefix[] = "GoogleLogin auth=";
const char kDMTokenAuthHeaderPrefix[] = "GoogleDMToken token=";
const char kEnrollmentTokenAuthHeaderPrefix[] = "GoogleEnrollmentToken token=";

// String constants for the device and app type we report to the server.
const char kValueAppType[] = "Chrome";
const char kValueDeviceType[] = "2";
const char kValueRequestAutoEnrollment[] = "enterprise_check";
const char kValueRequestPolicy[] = "policy";
const char kValueRequestRegister[] = "register";
const char kValueRequestApiAuthorization[] = "api_authorization";
const char kValueRequestUnregister[] = "unregister";
const char kValueRequestUploadCertificate[] = "cert_upload";
const char kValueRequestDeviceStateRetrieval[] = "device_state_retrieval";
const char kValueRequestUploadStatus[] = "status_upload";
const char kValueRequestRemoteCommands[] = "remote_commands";
const char kValueRequestDeviceAttributeUpdatePermission[] =
    "device_attribute_update_permission";
const char kValueRequestDeviceAttributeUpdate[] = "device_attribute_update";
const char kValueRequestGcmIdUpdate[] = "gcm_id_update";
const char kValueRequestCheckAndroidManagement[] = "check_android_management";
const char kValueRequestCertBasedRegister[] = "certificate_based_register";
const char kValueRequestActiveDirectoryEnrollPlayUser[] =
    "active_directory_enroll_play_user";
const char kValueRequestActiveDirectoryPlayActivity[] =
    "active_directory_play_activity";
const char kValueRequestCheckDeviceLicense[] = "check_device_license";
const char kValueRequestAppInstallReport[] = "app_install_report";
const char kValueRequestTokenEnrollment[] = "register_browser";
const char kValueRequestChromeDesktopReport[] = "chrome_desktop_report";
const char kValueRequestChromeOsUserReport[] = "chrome_os_user_report";
const char kValueRequestInitialEnrollmentStateRetrieval[] =
    "device_initial_enrollment_state";
const char kValueRequestUploadPolicyValidationReport[] =
    "policy_validation_report";
const char kValueRequestPublicSamlUser[] = "public_saml_user_request";

const char kChromeDevicePolicyType[] = "google/chromeos/device";
#if defined(OS_CHROMEOS)
const char kChromeUserPolicyType[] = "google/chromeos/user";
#elif defined(OS_ANDROID)
const char kChromeUserPolicyType[] = "google/android/user";
#elif defined(OS_IOS)
const char kChromeUserPolicyType[] = "google/ios/user";
#else
const char kChromeUserPolicyType[] = "google/chrome/user";
#endif
const char kChromePublicAccountPolicyType[] = "google/chromeos/publicaccount";
const char kChromeExtensionPolicyType[] = "google/chrome/extension";
const char kChromeSigninExtensionPolicyType[] =
    "google/chromeos/signinextension";
const char kChromeMachineLevelUserCloudPolicyType[] =
    "google/chrome/machine-level-user";
const char kChromeMachineLevelExtensionCloudPolicyType[] =
    "google/chrome/machine-level-extension";
const char kChromeRemoteCommandPolicyType[] = "google/chromeos/remotecommand";

}  // namespace dm_protocol

const uint8_t kPolicyVerificationKey[] = {
    0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86,
    0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00,
    0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xA7, 0xB3, 0xF9,
    0x0D, 0xC7, 0xC7, 0x8D, 0x84, 0x3D, 0x4B, 0x80, 0xDD, 0x9A, 0x2F, 0xF8,
    0x69, 0xD4, 0xD1, 0x14, 0x5A, 0xCA, 0x04, 0x4B, 0x1C, 0xBC, 0x28, 0xEB,
    0x5E, 0x10, 0x01, 0x36, 0xFD, 0x81, 0xEB, 0xE4, 0x3C, 0x16, 0x40, 0xA5,
    0x8A, 0xE6, 0x08, 0xEE, 0xEF, 0x39, 0x1F, 0x6B, 0x10, 0x29, 0x50, 0x84,
    0xCE, 0xEE, 0x33, 0x5C, 0x48, 0x4A, 0x33, 0xB0, 0xC8, 0x8A, 0x66, 0x0D,
    0x10, 0x11, 0x9D, 0x6B, 0x55, 0x4C, 0x9A, 0x62, 0x40, 0x9A, 0xE2, 0xCA,
    0x21, 0x01, 0x1F, 0x10, 0x1E, 0x7B, 0xC6, 0x89, 0x94, 0xDA, 0x39, 0x69,
    0xBE, 0x27, 0x28, 0x50, 0x5E, 0xA2, 0x55, 0xB9, 0x12, 0x3C, 0x79, 0x6E,
    0xDF, 0x24, 0xBF, 0x34, 0x88, 0xF2, 0x5E, 0xD0, 0xC4, 0x06, 0xEE, 0x95,
    0x6D, 0xC2, 0x14, 0xBF, 0x51, 0x7E, 0x3F, 0x55, 0x10, 0x85, 0xCE, 0x33,
    0x8F, 0x02, 0x87, 0xFC, 0xD2, 0xDD, 0x42, 0xAF, 0x59, 0xBB, 0x69, 0x3D,
    0xBC, 0x77, 0x4B, 0x3F, 0xC7, 0x22, 0x0D, 0x5F, 0x72, 0xC7, 0x36, 0xB6,
    0x98, 0x3D, 0x03, 0xCD, 0x2F, 0x68, 0x61, 0xEE, 0xF4, 0x5A, 0xF5, 0x07,
    0xAE, 0xAE, 0x79, 0xD1, 0x1A, 0xB2, 0x38, 0xE0, 0xAB, 0x60, 0x5C, 0x0C,
    0x14, 0xFE, 0x44, 0x67, 0x2C, 0x8A, 0x08, 0x51, 0x9C, 0xCD, 0x3D, 0xDB,
    0x13, 0x04, 0x57, 0xC5, 0x85, 0xB6, 0x2A, 0x0F, 0x02, 0x46, 0x0D, 0x2D,
    0xCA, 0xE3, 0x3F, 0x84, 0x9E, 0x8B, 0x8A, 0x5F, 0xFC, 0x4D, 0xAA, 0xBE,
    0xBD, 0xE6, 0x64, 0x9F, 0x26, 0x9A, 0x2B, 0x97, 0x69, 0xA9, 0xBA, 0x0B,
    0xBD, 0x48, 0xE4, 0x81, 0x6B, 0xD4, 0x4B, 0x78, 0xE6, 0xAF, 0x95, 0x66,
    0xC1, 0x23, 0xDA, 0x23, 0x45, 0x36, 0x6E, 0x25, 0xF3, 0xC7, 0xC0, 0x61,
    0xFC, 0xEC, 0x66, 0x9D, 0x31, 0xD4, 0xD6, 0xB6, 0x36, 0xE3, 0x7F, 0x81,
    0x87, 0x02, 0x03, 0x01, 0x00, 0x01};

const char kPolicyVerificationKeyHash[] = "1:356l7w";

const char kDemoModeDomain[] = "cros-demo-mode.com";

std::string GetPolicyVerificationKey() {
  return std::string(reinterpret_cast<const char*>(kPolicyVerificationKey),
                     sizeof(kPolicyVerificationKey));
}

const char kPolicyFCMInvalidationSenderID[] = "1013309121859";

}  // namespace policy
