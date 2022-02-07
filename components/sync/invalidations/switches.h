// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_SWITCHES_H_
#define COMPONENTS_SYNC_INVALIDATIONS_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

// If enabled, interested data types, excluding Wallet and Offer, will be sent
// to the Sync Server as part of DeviceInfo.
inline constexpr base::Feature kSyncSendInterestedDataTypes = {
    "SyncSendInterestedDataTypes", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the device will register with FCM and listen to new
// invalidations. Also, FCM token will be set in DeviceInfo, which signals to
// the server that device listens to new invalidations.
// The device will not subscribe to old invalidations for any data types except
// Wallet and Offer, since that will be covered by the new system.
// SyncSendInterestedDataTypes must be enabled for this to take effect.
inline constexpr base::Feature kUseSyncInvalidations = {
    "UseSyncInvalidations", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, types related to Wallet and Offer will be included in interested
// data types, and the device will listen to new invalidations for those types
// (if they are enabled).
// The device will not register for old invalidations at all.
// UseSyncInvalidations must be enabled for this to take effect.
inline constexpr base::Feature kUseSyncInvalidationsForWalletAndOffer = {
    "UseSyncInvalidationsForWalletAndOffer", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches

#endif  // COMPONENTS_SYNC_INVALIDATIONS_SWITCHES_H_
