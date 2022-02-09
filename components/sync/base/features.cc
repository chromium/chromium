// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/features.h"

namespace switches {

#if BUILDFLAG(IS_IOS)
bool IsSyncTrustedVaultPassphraseiOSRPCEnabled() {
  return base::FeatureList::IsEnabled(
             switches::kSyncTrustedVaultPassphraseRecovery) &&
         base::FeatureList::IsEnabled(
             switches::kSyncTrustedVaultPassphraseiOSRPC);
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace switches
