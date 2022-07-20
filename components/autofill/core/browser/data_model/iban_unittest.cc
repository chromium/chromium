// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/iban.h"

#include <string>

#include "base/guid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(IbanTest, AssignmentOperator) {
  // Creates two IBANs with different parameters.
  std::string guid = base::GenerateGUID();
  Iban iban_0;
  iban_0.set_guid(guid);
  iban_0.set_nickname(u"Nickname 0");
  iban_0.set_value(u"DE91 1000 0000 0123 4567 89");
  Iban iban_1;
  guid = base::GenerateGUID();
  iban_1.set_guid(guid);
  iban_1.set_nickname(u"Nickname 1");
  iban_1.set_value(u"IE64 IRCE 9205 0112 3456 78");
  iban_1 = iban_0;

  EXPECT_EQ(iban_0, iban_1);
}

TEST(IbanTest, GetMetadata) {
  Iban local_iban = test::GetIban();
  local_iban.set_use_count(2);
  local_iban.set_use_date(base::Time::FromDoubleT(25));
  AutofillMetadata local_metadata = local_iban.GetMetadata();

  EXPECT_EQ(local_iban.guid(), local_metadata.id);
  EXPECT_EQ(local_iban.use_count(), local_metadata.use_count);
  EXPECT_EQ(local_iban.use_date(), local_metadata.use_date);
}

// Verify that we set nickname with the processed string. We replace all tabs
// and newlines with whitespace, replace multiple spaces into a single one
// and trim leading/trailing whitespace.
TEST(IbanTest, SetNickname) {
  Iban iban(base::GenerateGUID());

  // Normal input nickname.
  iban.set_nickname(u"My doctor's IBAN");
  EXPECT_EQ(u"My doctor's IBAN", iban.nickname());

  // Input nickname has leading and trailing whitespaces.
  iban.set_nickname(u"  My doctor's IBAN  ");
  EXPECT_EQ(u"My doctor's IBAN", iban.nickname());

  // Input nickname has newlines.
  iban.set_nickname(u"\r\n My doctor's\nIBAN \r\n");
  EXPECT_EQ(u"My doctor's IBAN", iban.nickname());

  // Input nickname has tabs.
  iban.set_nickname(u" \tMy doctor's\t IBAN\t ");
  EXPECT_EQ(u"My doctor's IBAN", iban.nickname());

  // Input nickname has newlines & whitespaces & tabs.
  iban.set_nickname(u"\n\t My doctor's \tIBAN \n \r\n");
  EXPECT_EQ(u"My doctor's IBAN", iban.nickname());

  // Input nickname has newlines & tabs & multi spaces.
  iban.set_nickname(u"\n\t My doctor's    \tIBAN \n \r\n");
  EXPECT_EQ(u"My doctor's IBAN", iban.nickname());
}

TEST(IbanTest, SetValue) {
  Iban iban(base::GenerateGUID());

  // Input value.
  iban.set_value(u"DE91 1000 0000 0123 4567 89");
  EXPECT_EQ(u"DE91 1000 0000 0123 4567 89", iban.value());
}

TEST(IbanTest, SetRawData) {
  Iban iban(base::GenerateGUID());

  // Verify RawInfo can be correctly set and read.
  iban.SetRawInfoWithVerificationStatus(
      IBAN_VALUE, u"DE91 1000 0000 0123 4567 89",
      structured_address::VerificationStatus::kUserVerified);
  EXPECT_EQ(u"DE91 1000 0000 0123 4567 89", iban.GetRawInfo(IBAN_VALUE));
}

}  // namespace autofill
