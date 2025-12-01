// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_WALLETABLE_PASS_H_
#define COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_WALLETABLE_PASS_H_

#include <optional>
#include <string>
#include <variant>

#include "components/wallet/core/browser/data_models/wallet_barcode.h"

namespace optimization_guide::proto {
class EventPass;
class LoyaltyCard;
class TransitTicket;
class WalletablePass;
}  // namespace optimization_guide::proto

namespace wallet {

// Represents a loyalty card with its relevant details.
struct LoyaltyCard {
  static LoyaltyCard FromProto(
      const optimization_guide::proto::LoyaltyCard& proto,
      std::optional<WalletBarcode> barcode);

  LoyaltyCard();
  LoyaltyCard(const LoyaltyCard&);
  LoyaltyCard& operator=(const LoyaltyCard&);
  LoyaltyCard(LoyaltyCard&&);
  LoyaltyCard& operator=(LoyaltyCard&&);
  ~LoyaltyCard();

  bool operator==(const LoyaltyCard& other) const = default;

  std::string plan_name;
  std::string issuer_name;
  std::string member_id;

  // The detected barcode.
  std::optional<WalletBarcode> barcode;
};

// Represents an event pass with its relevant details.
struct EventPass {
  static EventPass FromProto(const optimization_guide::proto::EventPass& proto,
                             std::optional<WalletBarcode> barcode);

  EventPass();
  EventPass(const EventPass&);
  EventPass& operator=(const EventPass&);
  EventPass(EventPass&&);
  EventPass& operator=(EventPass&&);
  ~EventPass();

  bool operator==(const EventPass& other) const = default;

  std::string event_name;
  std::string event_start_date;
  std::string event_start_time;
  std::string event_end_time;
  std::string seat;
  std::string row;
  std::string section;
  std::string gate;
  std::string venue;
  std::string address;
  std::string owner_name;
  std::string issuer_name;

  // The detected barcode.
  std::optional<WalletBarcode> barcode;
};

// Represents a simplified boarding pass.
struct BoardingPass {
  static std::optional<BoardingPass> FromBCBP(const WalletBarcode& barcode);

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

// Represents a transit ticket with its relevant details.
struct TransitTicket {
  static TransitTicket FromProto(
      const optimization_guide::proto::TransitTicket& proto,
      std::optional<WalletBarcode> barcode);

  TransitTicket();
  TransitTicket(const TransitTicket&);
  TransitTicket& operator=(const TransitTicket&);
  TransitTicket(TransitTicket&&);
  TransitTicket& operator=(TransitTicket&&);
  ~TransitTicket();

  bool operator==(const TransitTicket& other) const = default;

  std::string issuer_name;
  std::string card_number;
  std::string date_of_expiry;
  std::string card_verification_code;
  std::string owner_name;
  std::string agency_name;
  std::string vehicle_number;
  std::string seat_number;
  std::string carriage_number;
  std::string platform;
  std::string ticket_type;
  std::string validity_period;
  std::string origin;
  std::string destination;
  std::string time_of_travel;
  std::string date_of_travel;

  // The detected barcode.
  std::optional<WalletBarcode> barcode;
};

// Represents a generic walletable pass, which can be either a LoyaltyCard or an
// EventPass or a BoardingPass.
struct WalletablePass {
  static std::optional<WalletablePass> FromProto(
      const optimization_guide::proto::WalletablePass& proto,
      std::optional<WalletBarcode> barcode = std::nullopt);
  static std::optional<WalletablePass> CreateBoardingPass(
      const WalletBarcode& barcode);

  WalletablePass();
  WalletablePass(const WalletablePass&);
  WalletablePass& operator=(const WalletablePass&);
  WalletablePass(WalletablePass&&);
  WalletablePass& operator=(WalletablePass&&);
  ~WalletablePass();

  bool operator==(const WalletablePass& other) const = default;

  std::variant<LoyaltyCard, EventPass, BoardingPass, TransitTicket> pass_data;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_WALLETABLE_PASS_H_
