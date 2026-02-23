// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/private_pass_conversion_util.h"

#include <cstdint>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/wallet/core/browser/proto/private_pass.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using DriverLicense = ::wallet::PrivatePass::DriverLicense;
using IdCard = ::wallet::PrivatePass::IdCard;
using KnownTravelerNumber = ::wallet::PrivatePass::KnownTravelerNumber;
using Passport = ::wallet::PrivatePass::Passport;
using RedressNumber = ::wallet::PrivatePass::RedressNumber;
using wallet::PrivatePass;

// Matches a `wallet::PrivatePass::NaiveDate` against the provided date.
MATCHER_P3(EqualsDate, year, month, day, "") {
  return arg.year() == year && arg.month() == month && arg.day() == day;
}

TEST(PrivatePassConversionUtil, Passport) {
  EntityInstance entity = test::GetPassportEntityInstance({
      .name = u"Name",
      .number = u"12345678",
      .country = u"Austria",
      .expiry_date = u"2019-08-30",
      .issue_date = u"2010-09-01",
  });
  PrivatePass private_pass = EntityInstanceToPrivatePass(entity);
  EXPECT_EQ(private_pass.pass_id(), entity.guid().value());
  ASSERT_TRUE(private_pass.has_passport());
  const Passport& pass = private_pass.passport();
  EXPECT_EQ(pass.owner_name(), "Name");
  EXPECT_EQ(pass.passport_number(), "12345678");
  EXPECT_EQ(pass.country_code(), "AT");
  EXPECT_THAT(pass.expiration_date(), EqualsDate(2019, 8, 30));
  EXPECT_THAT(pass.issue_date(), EqualsDate(2010, 9, 1));
}

TEST(PrivatePassConversionUtil, DriverLicense) {
  EntityInstance entity = test::GetDriversLicenseEntityInstance({
      .name = u"Name",
      .region = u"California",
      .number = u"12345678",
      .expiration_date = u"2019-08-30",
      .issue_date = u"2010-09-01",
  });
  PrivatePass private_pass = EntityInstanceToPrivatePass(entity);
  EXPECT_EQ(private_pass.pass_id(), entity.guid().value());
  ASSERT_TRUE(private_pass.has_driver_license());
  const DriverLicense& pass = private_pass.driver_license();
  EXPECT_EQ(pass.owner_name(), "Name");
  EXPECT_EQ(pass.driver_license_number(), "12345678");
  EXPECT_EQ(pass.region(), "California");
  EXPECT_THAT(pass.expiration_date(), EqualsDate(2019, 8, 30));
  EXPECT_THAT(pass.issue_date(), EqualsDate(2010, 9, 1));
}

TEST(PrivatePassConversionUtil, NationalIdCard) {
  EntityInstance entity = test::GetNationalIdCardEntityInstance({
      .name = u"Name",
      .number = u"12345678",
      .country = u"Germany",
      .issue_date = u"2010-09-01",
      .expiry_date = u"2019-08-30",
  });
  PrivatePass private_pass = EntityInstanceToPrivatePass(entity);
  EXPECT_EQ(private_pass.pass_id(), entity.guid().value());
  ASSERT_TRUE(private_pass.has_id_card());
  const IdCard& pass = private_pass.id_card();
  EXPECT_EQ(pass.owner_name(), "Name");
  EXPECT_EQ(pass.id_number(), "12345678");
  EXPECT_EQ(pass.country_code(), "DE");
  EXPECT_THAT(pass.issue_date(), EqualsDate(2010, 9, 1));
  EXPECT_THAT(pass.expiration_date(), EqualsDate(2019, 8, 30));
}

TEST(PrivatePassConversionUtil, KnownTravelerNumber) {
  EntityInstance entity = test::GetKnownTravelerNumberInstance({
      .name = u"Name",
      .number = u"12345678",
      .expiration_date = u"2019-08-30",
  });
  PrivatePass private_pass = EntityInstanceToPrivatePass(entity);
  EXPECT_EQ(private_pass.pass_id(), entity.guid().value());
  ASSERT_TRUE(private_pass.has_known_traveler_number());
  const KnownTravelerNumber& pass = private_pass.known_traveler_number();
  EXPECT_EQ(pass.owner_name(), "Name");
  EXPECT_EQ(pass.known_traveler_number(), "12345678");
  EXPECT_THAT(pass.expiration_date(), EqualsDate(2019, 8, 30));
}

TEST(PrivatePassConversionUtil, RedressNumber) {
  EntityInstance entity = test::GetRedressNumberEntityInstance({
      .name = u"Name",
      .number = u"12345678",
  });
  PrivatePass private_pass = EntityInstanceToPrivatePass(entity);
  EXPECT_EQ(private_pass.pass_id(), entity.guid().value());
  ASSERT_TRUE(private_pass.has_redress_number());
  const RedressNumber& pass = private_pass.redress_number();
  EXPECT_EQ(pass.owner_name(), "Name");
  EXPECT_EQ(pass.redress_number(), "12345678");
}

// Tests that missing attributes are omitted from the conversion.
// Only tested for passports, since the conversion logic is implemented in an
// entity-agnostic way.
TEST(PrivatePassConversionUtil, PartialEntities) {
  EntityInstance entity = test::GetPassportEntityInstance({
      .name = nullptr,
      .number = u"12345678",
      .country = nullptr,
      .expiry_date = nullptr,
      .issue_date = nullptr,
  });
  PrivatePass private_pass = EntityInstanceToPrivatePass(entity);
  EXPECT_EQ(private_pass.pass_id(), entity.guid().value());
  ASSERT_TRUE(private_pass.has_passport());
  const Passport& pass = private_pass.passport();
  EXPECT_EQ(pass.passport_number(), "12345678");
  EXPECT_FALSE(pass.has_owner_name());
  EXPECT_FALSE(pass.has_country_code());
  EXPECT_FALSE(pass.has_expiration_date());
  EXPECT_FALSE(pass.has_issue_date());
}

// Tests that invalid and partial dates are dropped during conversion.
// Only tested for passports, since the conversion logic is implemented in an
// entity-agnostic way.
TEST(PrivatePassConversionUtil, Dates) {
  EntityInstance entity = test::GetPassportEntityInstance({
      .name = nullptr,
      .number = u"12345678",
      .country = nullptr,
      .expiry_date = u"1234",    // Invalid date
      .issue_date = u"2010-09",  // Partial date
  });
  using enum AttributeTypeName;
  ASSERT_TRUE(entity.attribute(AttributeType(kPassportExpirationDate)));
  ASSERT_TRUE(entity.attribute(AttributeType(kPassportIssueDate)));
  PrivatePass private_pass = EntityInstanceToPrivatePass(entity);
  EXPECT_EQ(private_pass.pass_id(), entity.guid().value());
  ASSERT_TRUE(private_pass.has_passport());
  const Passport& pass = private_pass.passport();
  EXPECT_EQ(pass.passport_number(), "12345678");
  EXPECT_FALSE(pass.has_expiration_date());
  EXPECT_FALSE(pass.has_issue_date());
}

}  // namespace

}  // namespace autofill
