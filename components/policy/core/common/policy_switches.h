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

POLICY_EXPORT extern const char kDeviceManagementUrl[];
POLICY_EXPORT extern const char kRealtimeReportingUrl[];
POLICY_EXPORT extern const char kEncryptedReportingUrl[];
POLICY_EXPORT extern const char kChromePolicy[];
POLICY_EXPORT extern const char kSecureConnectApiUrl[];
#if BUILDFLAG(IS_CHROMEOS_ASH)
POLICY_EXPORT extern const char kDisablePolicyKeyVerification[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace switches
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_SWITCHES_H_
