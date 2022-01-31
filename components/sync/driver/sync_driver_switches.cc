// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_driver_switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace switches {

bool IsSyncAllowedByFlag() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSync);
}

#if BUILDFLAG(IS_IOS)
bool IsSyncTrustedVaultPassphraseiOSRPCEnabled() {
  return base::FeatureList::IsEnabled(
             switches::kSyncTrustedVaultPassphraseRecovery) &&
         base::FeatureList::IsEnabled(
             switches::kSyncTrustedVaultPassphraseiOSRPC);
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace switches
