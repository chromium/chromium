// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/features.h"

namespace syncer {

#if BUILDFLAG(IS_IOS)
bool IsSyncTrustedVaultPassphraseiOSRPCEnabled() {
  return base::FeatureList::IsEnabled(kSyncTrustedVaultPassphraseRecovery) &&
         base::FeatureList::IsEnabled(kSyncTrustedVaultPassphraseiOSRPC);
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace syncer
