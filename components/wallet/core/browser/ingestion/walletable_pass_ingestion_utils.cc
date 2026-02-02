// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/ingestion/walletable_pass_ingestion_utils.h"

#include <string>

#include "base/notreached.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"

namespace wallet {
namespace {

base::Time ParseTime(const std::string& time_string) {
  base::Time time;
  if (base::Time::FromUTCString(time_string.c_str(), &time)) {
    return time;
  }
  return base::Time();
}

LoyaltyCard ExtractLoyaltyCardFromProto(
    const optimization_guide::proto::LoyaltyCard& proto,
    std::optional<WalletBarcode> barcode) {
  LoyaltyCard card;
  card.plan_name = proto.plan_name();
  card.issuer_name = proto.issuer_name();
  card.member_id = proto.member_id();
  card.barcode = std::move(barcode);
  return card;
}

EventPass ExtractEventPassFromProto(
    const optimization_guide::proto::EventPass& proto,
    std::optional<WalletBarcode> barcode) {
  EventPass event;
  event.event_name = proto.event_name();
  event.event_start_time = ParseTime(proto.event_start_time());
  event.event_end_time = ParseTime(proto.event_end_time());
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

TransitTicket ExtractTransitTicketFromProto(
    const optimization_guide::proto::TransitTicket& proto,
    std::optional<WalletBarcode> barcode) {
  TransitTicket ticket;
  ticket.issuer_name = proto.issuer_name();
  ticket.card_number = proto.card_number();
  ticket.date_of_expiry = ParseTime(proto.date_of_expiry());
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
  ticket.travel_time = ParseTime(proto.time_of_travel());
  ticket.barcode = std::move(barcode);
  return ticket;
}

}  // namespace

std::optional<WalletPass> ExtractWalletPassFromProto(
    const optimization_guide::proto::WalletablePass& proto,
    std::optional<WalletBarcode> barcode) {
  WalletPass pass;
  switch (proto.pass_case()) {
    case optimization_guide::proto::WalletablePass::kLoyaltyCard: {
      pass.pass_data =
          ExtractLoyaltyCardFromProto(proto.loyalty_card(), std::move(barcode));
      return pass;
    }
    case optimization_guide::proto::WalletablePass::kEventPass: {
      pass.pass_data =
          ExtractEventPassFromProto(proto.event_pass(), std::move(barcode));
      return pass;
    }
    case optimization_guide::proto::WalletablePass::kTransitTicket: {
      pass.pass_data = ExtractTransitTicketFromProto(proto.transit_ticket(),
                                                     std::move(barcode));
      return pass;
    }
    case optimization_guide::proto::WalletablePass::PASS_NOT_SET:
      return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace wallet
