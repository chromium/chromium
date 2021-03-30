// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential_enrollment_model.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

TEST(PaymentCredentialEnrollmentModelTest, SmokeTest) {
  PaymentCredentialEnrollmentModel model;

  std::u16string title(u"Use Touch ID to verify and complete your purchase?");
  std::u16string description(base::UTF8ToUTF16(
      "Save payment information to this device and skip bank verification next "
      "time when you use Touch ID to verify your payment with Visa ••••4444."));
  std::u16string accept_button_label(u"Use Touch ID");
  std::u16string cancel_button_label(u"No thanks");

  model.set_title(title);
  EXPECT_EQ(title, model.title());

  model.set_description(description);
  EXPECT_EQ(description, model.description());

  model.set_accept_button_label(accept_button_label);
  EXPECT_EQ(accept_button_label, model.accept_button_label());

  model.set_cancel_button_label(cancel_button_label);
  EXPECT_EQ(cancel_button_label, model.cancel_button_label());

  // Default values for visibility and enabled states
  EXPECT_FALSE(model.progress_bar_visible());
  EXPECT_TRUE(model.accept_button_enabled());
  EXPECT_TRUE(model.accept_button_visible());
  EXPECT_TRUE(model.cancel_button_enabled());
  EXPECT_TRUE(model.cancel_button_visible());

  model.set_progress_bar_visible(true);
  model.set_accept_button_enabled(false);
  model.set_accept_button_visible(false);
  model.set_cancel_button_enabled(false);
  model.set_cancel_button_visible(false);

  EXPECT_TRUE(model.progress_bar_visible());
  EXPECT_FALSE(model.accept_button_enabled());
  EXPECT_FALSE(model.accept_button_visible());
  EXPECT_FALSE(model.cancel_button_enabled());
  EXPECT_FALSE(model.cancel_button_visible());
}

}  // namespace payments
