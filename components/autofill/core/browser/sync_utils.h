// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SYNC_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SYNC_UTILS_H_

namespace autofill {

// Since these values are persisted to logs, they should not be re-numbered or
// removed.
enum AutofillSyncSigninState {
  // The user is not signed in to Chromium.
  kSignedOut,
  // The user is signed in to Chromium.
  kSignedIn,
  // The user is signed in to Chromium and sync transport is active for Wallet
  // data.
  kSignedInAndWalletSyncTransportEnabled,
  // The user is signed in, has enabled the sync feature and has not disabled
  // Wallet sync.
  kSignedInAndSyncFeatureEnabled,
  // The user has enabled the sync feature, but has then signed out, so sync is
  // paused.
  kSyncPaused,
  kNumSyncStates,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SYNC_UTILS_H_
