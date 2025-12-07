// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_BOARDING_PASS_H_
#define COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_BOARDING_PASS_H_

#include <optional>
#include <string>

#include "components/wallet/core/browser/data_models/wallet_barcode.h"

namespace wallet {

// Represents a simplified boarding pass.
struct BoardingPass {
  // Parses a BCBP barcode string into a BoardingPass object.
  static std::optional<BoardingPass> FromBarcode(const WalletBarcode& barcode);

  BoardingPass();
  BoardingPass(const BoardingPass&);
  BoardingPass& operator=(const BoardingPass&);
  BoardingPass(BoardingPass&&);
  BoardingPass& operator=(BoardingPass&&);
  ~BoardingPass();

  bool operator==(const BoardingPass& other) const = default;

  std::string airline;
  std::string flight_code;
  std::string origin;
  std::string destination;
  std::string date;

  // The detected barcode.
  std::optional<WalletBarcode> barcode;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_BOARDING_PASS_H_
