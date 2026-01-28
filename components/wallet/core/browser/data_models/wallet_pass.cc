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
          [](const BoardingPass&) { return PassCategory::kBoardingPass; }),
      pass_data);
}

}  // namespace wallet
