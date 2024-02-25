// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_SWITCHES_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_SWITCHES_H_

#include "components/policy/policy_export.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace policy {
namespace switches {

extern const char kDeviceManagementUrl[];
extern const char kRealtimeReportingUrl[];
extern const char kEncryptedReportingUrl[];
extern const char kChromePolicy[];
extern const char kSecureConnectApiUrl[];
extern const char kFileStorageServerUploadUrl[];
extern const char kPolicyVerificationKey[];
#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kDisablePolicyKeyVerification[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace switches
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_SWITCHES_H_
