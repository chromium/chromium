// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_BOARDING_PASS_H_
#define COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_BOARDING_PASS_H_

#include <optional>
#include <string>

#include "base/time/time.h"
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

  friend bool operator==(const BoardingPass&, const BoardingPass&) = default;

  // The 2-3 character airline designator. e.g. "AC"
  std::string airline;
  // The flight number, as a string that can have leading zeros. e.g. "0834"
  std::string flight_code;
  // The 3-letter IATA airport code of the origin airport. e.g. "YUL"
  std::string origin;
  // The 3-letter IATA airport code of the destination airport. e.g. "FRA"
  std::string destination;
  // The date of the flight.
  base::Time date;
  // The passenger's name, usually in the format LASTNAME/FIRSTNAME. e.g.
  // "DESMARAIS/LUC"
  std::string passenger_name;

  // The detected barcode.
  std::optional<WalletBarcode> barcode;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_BOARDING_PASS_H_
