// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_model.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/vector_icon_types.h"

namespace payments {

class SecurePaymentConfirmationModelTest : public testing::Test {};

TEST_F(SecurePaymentConfirmationModelTest, SmokeTest) {
  SecurePaymentConfirmationModel model;

  std::u16string title(u"Use Touch ID to verify and complete your purchase?");
  std::u16string merchant_label(u"Store");
  std::u16string merchant_name(u"Test Merchant");
  std::u16string merchant_origin(u"merchant.com");
  std::u16string instrument_label(u"Payment");
  std::u16string instrument_value(u"Mastercard ****4444");
  SkBitmap instrument_icon;
  std::u16string total_label(u"Total");
  std::u16string total_value(u"$20.00 USD");
  std::u16string verify_button_label(u"Verify");
  std::u16string cancel_button_label(u"Cancel");
  std::u16string opt_out_label(u"Opt Out");
  std::u16string opt_out_link_text(u"Opt Out Link");
  std::u16string relying_party_id(u"example.test");

  model.set_title(title);
  EXPECT_EQ(title, model.title());

  model.set_merchant_label(merchant_label);
  EXPECT_EQ(merchant_label, model.merchant_label());

  model.set_merchant_name(merchant_name);
  EXPECT_EQ(merchant_name, model.merchant_name());

  model.set_merchant_origin(merchant_origin);
  EXPECT_EQ(merchant_origin, model.merchant_origin());

  model.set_instrument_label(instrument_label);
  EXPECT_EQ(instrument_label, model.instrument_label());

  model.set_instrument_value(instrument_value);
  EXPECT_EQ(instrument_value, model.instrument_value());

  model.set_instrument_icon(&instrument_icon);
  EXPECT_EQ(&instrument_icon, model.instrument_icon());

  model.set_total_label(total_label);
  EXPECT_EQ(total_label, model.total_label());

  model.set_total_value(total_value);
  EXPECT_EQ(total_value, model.total_value());

  model.set_verify_button_label(verify_button_label);
  EXPECT_EQ(verify_button_label, model.verify_button_label());

  model.set_cancel_button_label(cancel_button_label);
  EXPECT_EQ(cancel_button_label, model.cancel_button_label());

  model.set_opt_out_label(opt_out_label);
  EXPECT_EQ(opt_out_label, model.opt_out_label());

  model.set_opt_out_link_label(opt_out_link_text);
  EXPECT_EQ(opt_out_link_text, model.opt_out_link_label());

  model.set_relying_party_id(relying_party_id);
  EXPECT_EQ(relying_party_id, model.relying_party_id());

  // Default values for visibility and enabled states
  EXPECT_FALSE(model.progress_bar_visible());
  EXPECT_TRUE(model.verify_button_enabled());
  EXPECT_TRUE(model.verify_button_visible());
  EXPECT_TRUE(model.cancel_button_enabled());
  EXPECT_TRUE(model.cancel_button_visible());
  EXPECT_FALSE(model.opt_out_visible());

  model.set_progress_bar_visible(true);
  model.set_verify_button_enabled(false);
  model.set_verify_button_visible(false);
  model.set_cancel_button_enabled(false);
  model.set_cancel_button_visible(false);
  model.set_opt_out_visible(true);

  EXPECT_TRUE(model.progress_bar_visible());
  EXPECT_FALSE(model.verify_button_enabled());
  EXPECT_FALSE(model.verify_button_visible());
  EXPECT_FALSE(model.cancel_button_enabled());
  EXPECT_FALSE(model.cancel_button_visible());
  EXPECT_TRUE(model.opt_out_visible());
}

}  // namespace payments
