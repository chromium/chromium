// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/common/wallet_features.h"

namespace wallet {

// Controls whether to enable walletable pass detection on web pages.
BASE_FEATURE(kWalletablePassDetection,
             "WalletablePassDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace wallet
