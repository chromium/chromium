// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_switches.h"

namespace policy {
namespace switches {

// Specifies the URL at which to communicate with the device management backend
// to fetch configuration policies and perform other device tasks.
const char kDeviceManagementUrl[] = "device-management-url";

// Specifies the URL at which to upload real-time reports.
const char kRealtimeReportingUrl[] = "realtime-reporting-url";

// Specifies the URL at which to upload encrypted reports.
const char kEncryptedReportingUrl[] = "encrypted-reporting-url";

// Set policy value by command line.
const char kChromePolicy[] = "policy";

// Specifies the URL at which to communicate with File Storage Server
// (go/crosman-file-storage-server) to upload log and support packet files.
const char kFileStorageServerUploadUrl[] = "file-storage-server-upload-url";

// Replace the original verification_key with the one provided by the command
// line flag. Can be used only for unit tests or browser tests.
const char kPolicyVerificationKey[] = "policy-verification-key";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Disables the verification of policy signing keys. It just works on Chrome OS
// test images and crashes otherwise.
// TODO(crbug.com/1225054): This flag might introduce security risks. Find a
// better solution to enable policy tast test for Family Link account.
const char kDisablePolicyKeyVerification[] = "disable-policy-key-verification";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Specifies the base URL to contact the secure connect Api.
const char kSecureConnectApiUrl[] = "secure-connect-api-url";
}  // namespace switches
}  // namespace policy
