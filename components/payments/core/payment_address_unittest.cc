// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_address.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests that two addresses are not equal if their property values differ or
// one is missing a value present in the other, and equal otherwise.
TEST(PaymentRequestTest, PaymentAddressEquality) {
  mojom::PaymentAddress address1;
  mojom::PaymentAddress address2;
  EXPECT_TRUE(address1.Equals(address2));

  address1.country = "Madagascar";
  EXPECT_FALSE(address1.Equals(address2));
  address2.country = "Monaco";
  EXPECT_FALSE(address1.Equals(address2));
  address2.country = "Madagascar";
  EXPECT_TRUE(address1.Equals(address2));

  std::vector<std::string> address_line1;
  address_line1.push_back("123 Main St.");
  address_line1.push_back("Apartment B");
  address1.address_line = address_line1;
  EXPECT_FALSE(address1.Equals(address2));
  std::vector<std::string> address_line2;
  address_line2.push_back("123 Main St.");
  address_line2.push_back("Apartment C");
  address2.address_line = address_line2;
  EXPECT_FALSE(address1.Equals(address2));
  address2.address_line = address_line1;
  EXPECT_TRUE(address1.Equals(address2));

  address1.region = "Quebec";
  EXPECT_FALSE(address1.Equals(address2));
  address2.region = "Newfoundland and Labrador";
  EXPECT_FALSE(address1.Equals(address2));
  address2.region = "Quebec";
  EXPECT_TRUE(address1.Equals(address2));

  address1.city = "Timbuktu";
  EXPECT_FALSE(address1.Equals(address2));
  address2.city = "Timbuk 3";
  EXPECT_FALSE(address1.Equals(address2));
  address2.city = "Timbuktu";
  EXPECT_TRUE(address1.Equals(address2));

  address1.dependent_locality = "Manhattan";
  EXPECT_FALSE(address1.Equals(address2));
  address2.dependent_locality = "Queens";
  EXPECT_FALSE(address1.Equals(address2));
  address2.dependent_locality = "Manhattan";
  EXPECT_TRUE(address1.Equals(address2));

  address1.postal_code = "90210";
  EXPECT_FALSE(address1.Equals(address2));
  address2.postal_code = "89049";
  EXPECT_FALSE(address1.Equals(address2));
  address2.postal_code = "90210";
  EXPECT_TRUE(address1.Equals(address2));

  address1.sorting_code = "14390";
  EXPECT_FALSE(address1.Equals(address2));
  address2.sorting_code = "09341";
  EXPECT_FALSE(address1.Equals(address2));
  address2.sorting_code = "14390";
  EXPECT_TRUE(address1.Equals(address2));

  address1.organization = "The Willy Wonka Candy Company";
  EXPECT_FALSE(address1.Equals(address2));
  address2.organization = "Sears";
  EXPECT_FALSE(address1.Equals(address2));
  address2.organization = "The Willy Wonka Candy Company";
  EXPECT_TRUE(address1.Equals(address2));

  address1.recipient = "Veruca Salt";
  EXPECT_FALSE(address1.Equals(address2));
  address2.recipient = "Veronica Mars";
  EXPECT_FALSE(address1.Equals(address2));
  address2.recipient = "Veruca Salt";
  EXPECT_TRUE(address1.Equals(address2));

  address1.phone = "888-867-5309";
  EXPECT_FALSE(address1.Equals(address2));
  address2.phone = "800-984-3672";
  EXPECT_FALSE(address1.Equals(address2));
  address2.phone = "888-867-5309";
  EXPECT_TRUE(address1.Equals(address2));
}

}  // namespace payments
