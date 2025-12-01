// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/data_models/walletable_pass.h"

#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

TEST(WalletablePassTest, FromProto_LoyaltyCard_WithBarcode) {
  optimization_guide::proto::WalletablePass proto;
  auto* loyalty_proto = proto.mutable_loyalty_card();
  loyalty_proto->set_plan_name("Plan B");

  WalletBarcode passed_barcode;
  passed_barcode.raw_value = "loyalty_barcode";
  passed_barcode.format = WalletBarcodeFormat::QR_CODE;

  std::optional<WalletablePass> result =
      WalletablePass::FromProto(proto, passed_barcode);

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(std::holds_alternative<LoyaltyCard>(result->pass_data));

  const LoyaltyCard& card = std::get<LoyaltyCard>(result->pass_data);
  EXPECT_EQ(card.plan_name, "Plan B");
  ASSERT_TRUE(card.barcode.has_value());
  EXPECT_EQ(card.barcode->raw_value, "loyalty_barcode");
  EXPECT_EQ(card.barcode->format, WalletBarcodeFormat::QR_CODE);
}

TEST(WalletablePassTest, FromProto_EventPass_WithBarcode) {
  optimization_guide::proto::WalletablePass proto;
  auto* event_proto = proto.mutable_event_pass();
  event_proto->set_event_name("Concert Y");

  WalletBarcode passed_barcode;
  passed_barcode.raw_value = "event_barcode";
  passed_barcode.format = WalletBarcodeFormat::AZTEC;

  std::optional<WalletablePass> result =
      WalletablePass::FromProto(proto, passed_barcode);

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(std::holds_alternative<EventPass>(result->pass_data));

  const EventPass& event = std::get<EventPass>(result->pass_data);
  EXPECT_EQ(event.event_name, "Concert Y");
  ASSERT_TRUE(event.barcode.has_value());
  EXPECT_EQ(event.barcode->raw_value, "event_barcode");
  EXPECT_EQ(event.barcode->format, WalletBarcodeFormat::AZTEC);
}

TEST(WalletablePassTest, FromProto_TransitTicket) {
  optimization_guide::proto::WalletablePass proto;
  auto* transit_proto = proto.mutable_transit_ticket();
  transit_proto->set_issuer_name("MTA");
  transit_proto->set_ticket_type("Monthly Pass");
  transit_proto->set_origin("New York");
  transit_proto->set_destination("Brooklyn");

  WalletBarcode passed_barcode;
  passed_barcode.raw_value = "detected_barcode_value";
  passed_barcode.format = WalletBarcodeFormat::QR_CODE;

  std::optional<WalletablePass> result =
      WalletablePass::FromProto(proto, passed_barcode);

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(std::holds_alternative<TransitTicket>(result->pass_data));

  const TransitTicket& ticket = std::get<TransitTicket>(result->pass_data);
  EXPECT_EQ(ticket.issuer_name, "MTA");
  EXPECT_EQ(ticket.ticket_type, "Monthly Pass");
  EXPECT_EQ(ticket.origin, "New York");
  EXPECT_EQ(ticket.destination, "Brooklyn");
  ASSERT_TRUE(ticket.barcode.has_value());
  EXPECT_EQ(ticket.barcode->raw_value, "detected_barcode_value");
  EXPECT_EQ(ticket.barcode->format, WalletBarcodeFormat::QR_CODE);
}

TEST(WalletablePassTest, FromProto_Invalid) {
  optimization_guide::proto::WalletablePass proto;
  // Proto is empty, neither loyalty nor event pass is set.

  std::optional<WalletablePass> result = WalletablePass::FromProto(proto);

  EXPECT_FALSE(result.has_value());
}

}  // namespace wallet
