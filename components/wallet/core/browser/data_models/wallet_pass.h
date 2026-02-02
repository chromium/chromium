// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_WALLET_PASS_H_
#define COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_WALLET_PASS_H_

#include <optional>
#include <string>
#include <variant>

#include "base/time/time.h"
#include "components/wallet/core/browser/data_models/boarding_pass.h"
#include "components/wallet/core/browser/data_models/wallet_barcode.h"

namespace wallet {

// Represents the category of a walletable pass. These categories correspond to
// the different types of passes that can be saved to Google Wallet.
enum class PassCategory {
  kUnspecified = 0,
  kLoyaltyCard = 1,
  kEventPass = 2,
  kTransitTicket = 3,
  kBoardingPass = 4,
  kPassport = 5,
  kDriverLicense = 6,
  kNationalIdentityCard = 7,
  kKTN = 8,
  kRedressNumber = 9,
  kMaxValue = kRedressNumber,
};

// The following structs represent the data models for the different pass
// categories. They store the fields extracted from the pass and are used for
// display and network requests.
// Represents a loyalty card with its relevant details.
struct LoyaltyCard {
  LoyaltyCard();
  LoyaltyCard(const LoyaltyCard&);
  LoyaltyCard& operator=(const LoyaltyCard&);
  LoyaltyCard(LoyaltyCard&&);
  LoyaltyCard& operator=(LoyaltyCard&&);
  ~LoyaltyCard();

  friend bool operator==(const LoyaltyCard&, const LoyaltyCard&) = default;

  std::string plan_name;
  std::string issuer_name;
  std::string member_id;

  // The detected barcode.
  std::optional<WalletBarcode> barcode;
};

// Represents an event pass with its relevant details.
struct EventPass {
  EventPass();
  EventPass(const EventPass&);
  EventPass& operator=(const EventPass&);
  EventPass(EventPass&&);
  EventPass& operator=(EventPass&&);
  ~EventPass();

  friend bool operator==(const EventPass&, const EventPass&) = default;

  std::string event_name;
  base::Time event_start_time;
  base::Time event_end_time;
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

// Represents a transit ticket with its relevant details.
struct TransitTicket {
  TransitTicket();
  TransitTicket(const TransitTicket&);
  TransitTicket& operator=(const TransitTicket&);
  TransitTicket(TransitTicket&&);
  TransitTicket& operator=(TransitTicket&&);
  ~TransitTicket();

  friend bool operator==(const TransitTicket&, const TransitTicket&) = default;

  std::string issuer_name;
  std::string card_number;
  base::Time date_of_expiry;
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
  base::Time travel_time;

  // The detected barcode.
  std::optional<WalletBarcode> barcode;
};

// Represents a passport with its relevant details.
struct Passport {
  Passport();
  Passport(const Passport&);
  Passport& operator=(const Passport&);
  Passport(Passport&&);
  Passport& operator=(Passport&&);
  ~Passport();

  friend bool operator==(const Passport&, const Passport&) = default;

  std::string owner_name;
  std::string country_code;
  std::string passport_number;
  base::Time issue_date;
  base::Time expiration_date;
};

// Represents a driver's license with its relevant details.
struct DriverLicense {
  DriverLicense();
  DriverLicense(const DriverLicense&);
  DriverLicense& operator=(const DriverLicense&);
  DriverLicense(DriverLicense&&);
  DriverLicense& operator=(DriverLicense&&);
  ~DriverLicense();

  friend bool operator==(const DriverLicense&, const DriverLicense&) = default;

  std::string owner_name;
  std::string region;
  std::string driver_license_number;
  base::Time issue_date;
  base::Time expiration_date;
  std::string country_code;
};

// Represents a national identity card with its relevant details.
struct NationalIdentityCard {
  NationalIdentityCard();
  NationalIdentityCard(const NationalIdentityCard&);
  NationalIdentityCard& operator=(const NationalIdentityCard&);
  NationalIdentityCard(NationalIdentityCard&&);
  NationalIdentityCard& operator=(NationalIdentityCard&&);
  ~NationalIdentityCard();

  friend bool operator==(const NationalIdentityCard&,
                         const NationalIdentityCard&) = default;

  std::string owner_name;
  std::string region;
  std::string id_number;
  base::Time issue_date;
  base::Time expiration_date;
  std::string country_code;
};

// Represents a Known Traveler Number (KTN) with its relevant details.
struct KTN {
  KTN();
  KTN(const KTN&);
  KTN& operator=(const KTN&);
  KTN(KTN&&);
  KTN& operator=(KTN&&);
  ~KTN();

  friend bool operator==(const KTN&, const KTN&) = default;

  std::string owner_name;
  std::string known_traveller_number;
  base::Time expiration_date;
};

// Represents a redress number with its relevant details.
struct RedressNumber {
  RedressNumber();
  RedressNumber(const RedressNumber&);
  RedressNumber& operator=(const RedressNumber&);
  RedressNumber(RedressNumber&&);
  RedressNumber& operator=(RedressNumber&&);
  ~RedressNumber();

  friend bool operator==(const RedressNumber&, const RedressNumber&) = default;

  std::string owner_name;
  std::string redress_number;
};

// Represents a generic Google Wallet pass.
struct WalletPass {
  WalletPass();
  WalletPass(const WalletPass&);
  WalletPass& operator=(const WalletPass&);
  WalletPass(WalletPass&&);
  WalletPass& operator=(WalletPass&&);
  ~WalletPass();

  friend bool operator==(const WalletPass&, const WalletPass&) = default;

  // Returns the pass category of the Google Wallet pass.
  PassCategory GetPassCategory() const;

  // Contains the id of the Wallet pass. It should only be `std::nullopt` for
  // save pass requests.
  std::optional<std::string> id;

  std::variant<LoyaltyCard,
               EventPass,
               BoardingPass,
               TransitTicket,
               Passport,
               DriverLicense,
               NationalIdentityCard,
               KTN,
               RedressNumber>
      pass_data;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_WALLET_PASS_H_
