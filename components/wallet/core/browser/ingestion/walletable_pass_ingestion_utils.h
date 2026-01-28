// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_INGESTION_WALLETABLE_PASS_INGESTION_UTILS_H_
#define COMPONENTS_WALLET_CORE_BROWSER_INGESTION_WALLETABLE_PASS_INGESTION_UTILS_H_

#include <optional>

#include "components/wallet/core/browser/data_models/wallet_barcode.h"
#include "components/wallet/core/browser/data_models/wallet_pass.h"

namespace optimization_guide::proto {
class WalletablePass;
}  // namespace optimization_guide::proto

namespace wallet {

// Extracts a WalletPass from its proto representation.
std::optional<WalletPass> ExtractWalletPassFromProto(
    const optimization_guide::proto::WalletablePass& proto,
    std::optional<WalletBarcode> barcode = std::nullopt);

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_INGESTION_WALLETABLE_PASS_INGESTION_UTILS_H_
