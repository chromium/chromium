// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/switches.h"

namespace switches {

const base::Feature kSyncSendInterestedDataTypes = {
    "SyncSendInterestedDataTypes", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseSyncInvalidations = {"UseSyncInvalidations",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseSyncInvalidationsForWalletAndOffer = {
    "UseSyncInvalidationsForWalletAndOffer", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches
