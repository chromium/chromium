// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/data_models/wallet_pass.h"

#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace wallet {

LoyaltyCard::LoyaltyCard() = default;
LoyaltyCard::LoyaltyCard(const LoyaltyCard&) = default;
LoyaltyCard& LoyaltyCard::operator=(const LoyaltyCard&) = default;
LoyaltyCard::LoyaltyCard(LoyaltyCard&&) = default;
LoyaltyCard& LoyaltyCard::operator=(LoyaltyCard&&) = default;
LoyaltyCard::~LoyaltyCard() = default;

EventPass::EventPass() = default;
EventPass::EventPass(const EventPass&) = default;
EventPass& EventPass::operator=(const EventPass&) = default;
EventPass::EventPass(EventPass&&) = default;
EventPass& EventPass::operator=(EventPass&&) = default;
EventPass::~EventPass() = default;

TransitTicket::TransitTicket() = default;
TransitTicket::TransitTicket(const TransitTicket&) = default;
TransitTicket& TransitTicket::operator=(const TransitTicket&) = default;
TransitTicket::TransitTicket(TransitTicket&&) = default;
TransitTicket& TransitTicket::operator=(TransitTicket&&) = default;
TransitTicket::~TransitTicket() = default;

Passport::Passport() = default;
Passport::Passport(const Passport&) = default;
Passport& Passport::operator=(const Passport&) = default;
Passport::Passport(Passport&&) = default;
Passport& Passport::operator=(Passport&&) = default;
Passport::~Passport() = default;

DriverLicense::DriverLicense() = default;
DriverLicense::DriverLicense(const DriverLicense&) = default;
DriverLicense& DriverLicense::operator=(const DriverLicense&) = default;
DriverLicense::DriverLicense(DriverLicense&&) = default;
DriverLicense& DriverLicense::operator=(DriverLicense&&) = default;
DriverLicense::~DriverLicense() = default;

NationalIdentityCard::NationalIdentityCard() = default;
NationalIdentityCard::NationalIdentityCard(const NationalIdentityCard&) =
    default;
NationalIdentityCard& NationalIdentityCard::operator=(
    const NationalIdentityCard&) = default;
NationalIdentityCard::NationalIdentityCard(NationalIdentityCard&&) = default;
NationalIdentityCard& NationalIdentityCard::operator=(NationalIdentityCard&&) =
    default;
NationalIdentityCard::~NationalIdentityCard() = default;

KTN::KTN() = default;
KTN::KTN(const KTN&) = default;
KTN& KTN::operator=(const KTN&) = default;
KTN::KTN(KTN&&) = default;
KTN& KTN::operator=(KTN&&) = default;
KTN::~KTN() = default;

RedressNumber::RedressNumber() = default;
RedressNumber::RedressNumber(const RedressNumber&) = default;
RedressNumber& RedressNumber::operator=(const RedressNumber&) = default;
RedressNumber::RedressNumber(RedressNumber&&) = default;
RedressNumber& RedressNumber::operator=(RedressNumber&&) = default;
RedressNumber::~RedressNumber() = default;

WalletPass::WalletPass() = default;
WalletPass::WalletPass(const WalletPass&) = default;
WalletPass& WalletPass::operator=(const WalletPass&) = default;
WalletPass::WalletPass(WalletPass&&) = default;
WalletPass& WalletPass::operator=(WalletPass&&) = default;
WalletPass::~WalletPass() = default;

PassCategory WalletPass::GetPassCategory() const {
  return std::visit(
      absl::Overload(
          [](const LoyaltyCard&) { return PassCategory::kLoyaltyCard; },
          [](const EventPass&) { return PassCategory::kEventPass; },
          [](const TransitTicket&) { return PassCategory::kTransitTicket; },
          [](const BoardingPass&) { return PassCategory::kBoardingPass; },
          [](const Passport&) { return PassCategory::kPassport; },
          [](const DriverLicense&) { return PassCategory::kDriverLicense; },
          [](const NationalIdentityCard&) {
            return PassCategory::kNationalIdentityCard;
          },
          [](const KTN&) { return PassCategory::kKTN; },
          [](const RedressNumber&) { return PassCategory::kRedressNumber; }),
      pass_data);
}

}  // namespace wallet
