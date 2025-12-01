// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/data_models/walletable_pass.h"

#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"

namespace wallet {

// static
LoyaltyCard LoyaltyCard::FromProto(
    const optimization_guide::proto::LoyaltyCard& proto,
    std::optional<WalletBarcode> barcode) {
  LoyaltyCard card;
  card.plan_name = proto.plan_name();
  card.issuer_name = proto.issuer_name();
  card.member_id = proto.member_id();
  card.barcode = std::move(barcode);
  return card;
}

LoyaltyCard::LoyaltyCard() = default;
LoyaltyCard::LoyaltyCard(const LoyaltyCard&) = default;
LoyaltyCard& LoyaltyCard::operator=(const LoyaltyCard&) = default;
LoyaltyCard::LoyaltyCard(LoyaltyCard&&) = default;
LoyaltyCard& LoyaltyCard::operator=(LoyaltyCard&&) = default;
LoyaltyCard::~LoyaltyCard() = default;

// static
EventPass EventPass::FromProto(
    const optimization_guide::proto::EventPass& proto,
    std::optional<WalletBarcode> barcode) {
  EventPass event;
  event.event_name = proto.event_name();
  event.event_start_date = proto.event_start_date();
  event.event_start_time = proto.event_start_time();
  event.event_end_time = proto.event_end_time();
  event.seat = proto.seat();
  event.row = proto.row();
  event.section = proto.section();
  event.gate = proto.gate();
  event.venue = proto.venue();
  event.address = proto.address();
  event.owner_name = proto.owner_name();
  event.issuer_name = proto.issuer_name();
  event.barcode = std::move(barcode);
  return event;
}

EventPass::EventPass() = default;
EventPass::EventPass(const EventPass&) = default;
EventPass& EventPass::operator=(const EventPass&) = default;
EventPass::EventPass(EventPass&&) = default;
EventPass& EventPass::operator=(EventPass&&) = default;
EventPass::~EventPass() = default;

// static
TransitTicket TransitTicket::FromProto(
    const optimization_guide::proto::TransitTicket& proto,
    std::optional<WalletBarcode> barcode) {
  TransitTicket ticket;
  ticket.issuer_name = proto.issuer_name();
  ticket.card_number = proto.card_number();
  ticket.date_of_expiry = proto.date_of_expiry();
  ticket.card_verification_code = proto.card_verification_code();
  ticket.owner_name = proto.owner_name();
  ticket.agency_name = proto.agency_name();
  ticket.vehicle_number = proto.vehicle_number();
  ticket.seat_number = proto.seat_number();
  ticket.carriage_number = proto.carriage_number();
  ticket.platform = proto.platform();
  ticket.ticket_type = proto.ticket_type();
  ticket.validity_period = proto.validity_period();
  ticket.origin = proto.origin();
  ticket.destination = proto.destination();
  ticket.time_of_travel = proto.time_of_travel();
  ticket.date_of_travel = proto.date_of_travel();
  ticket.barcode = std::move(barcode);

  return ticket;
}

TransitTicket::TransitTicket() = default;
TransitTicket::TransitTicket(const TransitTicket&) = default;
TransitTicket& TransitTicket::operator=(const TransitTicket&) = default;
TransitTicket::TransitTicket(TransitTicket&&) = default;
TransitTicket& TransitTicket::operator=(TransitTicket&&) = default;
TransitTicket::~TransitTicket() = default;

// static
std::optional<BoardingPass> BoardingPass::FromBCBP(
    const WalletBarcode& barcode) {
  // TODO(crbug.com/463515055): Decode BCBP barcode to boarding pass.
  return std::nullopt;
}

BoardingPass::BoardingPass() = default;
BoardingPass::BoardingPass(const BoardingPass&) = default;
BoardingPass& BoardingPass::operator=(const BoardingPass&) = default;
BoardingPass::BoardingPass(BoardingPass&&) = default;
BoardingPass& BoardingPass::operator=(BoardingPass&&) = default;
BoardingPass::~BoardingPass() = default;

// static
std::optional<WalletablePass> WalletablePass::FromProto(
    const optimization_guide::proto::WalletablePass& proto,
    std::optional<WalletBarcode> barcode) {
  if (proto.has_loyalty_card()) {
    LoyaltyCard card =
        LoyaltyCard::FromProto(proto.loyalty_card(), std::move(barcode));
    WalletablePass pass;
    pass.pass_data = std::move(card);
    return pass;
  } else if (proto.has_event_pass()) {
    EventPass event =
        EventPass::FromProto(proto.event_pass(), std::move(barcode));
    WalletablePass pass;
    pass.pass_data = std::move(event);
    return pass;
  } else if (proto.has_transit_ticket()) {
    TransitTicket ticket =
        TransitTicket::FromProto(proto.transit_ticket(), std::move(barcode));
    WalletablePass pass;
    pass.pass_data = std::move(ticket);
    return pass;
  }
  return std::nullopt;
}

// static
std::optional<WalletablePass> WalletablePass::CreateBoardingPass(
    const WalletBarcode& barcode) {
  std::optional<BoardingPass> boarding_pass = BoardingPass::FromBCBP(barcode);
  if (boarding_pass) {
    WalletablePass pass;
    pass.pass_data = std::move(*boarding_pass);
    return pass;
  }
  return std::nullopt;
}

WalletablePass::WalletablePass() = default;
WalletablePass::WalletablePass(const WalletablePass&) = default;
WalletablePass& WalletablePass::operator=(const WalletablePass&) = default;
WalletablePass::WalletablePass(WalletablePass&&) = default;
WalletablePass& WalletablePass::operator=(WalletablePass&&) = default;
WalletablePass::~WalletablePass() = default;

}  // namespace wallet
