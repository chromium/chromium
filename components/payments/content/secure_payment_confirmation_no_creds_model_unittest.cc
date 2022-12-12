// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_no_creds_model.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/vector_icon_types.h"

namespace payments {

class SecurePaymentConfirmationNoCredsModelTest : public testing::Test {};

TEST_F(SecurePaymentConfirmationNoCredsModelTest, SmokeTest) {
  base::WeakPtr<SecurePaymentConfirmationNoCredsModel> weak_ptr;
  {
    SecurePaymentConfirmationNoCredsModel model;

    std::u16string no_creds_text(
        u"example.test may need to take additional steps to verify your "
        u"payment");
    std::u16string opt_out_label(u"Opt Out");
    std::u16string opt_out_link_text(u"Opt Out Link");
    std::u16string relying_party_id(u"example.test");

    model.set_no_creds_text(no_creds_text);
    EXPECT_EQ(no_creds_text, model.no_creds_text());

    model.set_opt_out_label(opt_out_label);
    EXPECT_EQ(opt_out_label, model.opt_out_label());

    model.set_opt_out_link_label(opt_out_link_text);
    EXPECT_EQ(opt_out_link_text, model.opt_out_link_label());

    model.set_relying_party_id(relying_party_id);
    EXPECT_EQ(relying_party_id, model.relying_party_id());

    // Opt out is not visible by default.
    EXPECT_FALSE(model.opt_out_visible());
    model.set_opt_out_visible(true);
    EXPECT_TRUE(model.opt_out_visible());

    weak_ptr = model.GetWeakPtr();
    ASSERT_NE(nullptr, weak_ptr.get());
  }
  ASSERT_EQ(nullptr, weak_ptr.get());
}

}  // namespace payments
