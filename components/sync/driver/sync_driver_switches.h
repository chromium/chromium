// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_DRIVER_SWITCHES_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_DRIVER_SWITCHES_H_

// TODO(crbug.com/1295324): Remove this header once downstream no longer
// includes it.
#include "base/feature_list.h"
#include "components/sync/base/features.h"

namespace switches {

inline constexpr base::Feature kSyncTrustedVaultPassphraseiOSRPC =
    syncer::kSyncTrustedVaultPassphraseiOSRPC;

inline bool IsSyncTrustedVaultPassphraseiOSRPCEnabled() {
  return syncer::IsSyncTrustedVaultPassphraseiOSRPCEnabled();
}

}  // namespace switches

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_DRIVER_SWITCHES_H_
