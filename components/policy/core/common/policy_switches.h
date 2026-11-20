// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_SWITCHES_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_SWITCHES_H_

#include "build/build_config.h"
#include "components/policy/buildflags/buildflags.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace switches {

extern const char kDeviceManagementUrl[];
extern const char kRealtimeReportingUrl[];
extern const char kEncryptedReportingUrl[];
extern const char kChromePolicy[];
extern const char kSecureConnectApiUrl[];
extern const char kFileStorageServerUploadUrl[];
extern const char kPolicyVerificationKey[];
#if BUILDFLAG(IS_CHROMEOS)
extern const char kDisablePolicyKeyVerification[];
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
extern const char kForceDeviceOwnership[];
#endif

#if BUILDFLAG(ALLOW_FAKE_DMSERVER)
extern const char kUseFakeDmserver[];
#endif

}  // namespace switches

// Returns true if this build allows fake_dmserver, and --use-fake-dmserver is
// on the command line.
bool IsFakeDmserverEnabled();

// Returns true if `IsFakeDmserverEnabled()`, and the switch requests automatic
// enrollment (--use-fake-dmserver=always-enroll).
bool FakeDmserverAlwaysEnrolls();

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_SWITCHES_H_
