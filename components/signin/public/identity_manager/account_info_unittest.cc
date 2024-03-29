// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "testing/gtest/include/gtest/gtest.h"

class AccountInfoTest : public testing::Test {};

TEST_F(AccountInfoTest, IsEmpty) {
  {
    AccountInfo info_empty;
    EXPECT_TRUE(info_empty.IsEmpty());
  }
  {
    AccountInfo info_with_account_id;
    info_with_account_id.account_id = CoreAccountId::FromGaiaId("test_id");
    EXPECT_FALSE(info_with_account_id.IsEmpty());
  }
  {
    AccountInfo info_with_email;
    info_with_email.email = "test_email@email.com";
    EXPECT_FALSE(info_with_email.IsEmpty());
  }
  {
    AccountInfo info_with_gaia;
    info_with_gaia.gaia = "test_gaia";
    EXPECT_FALSE(info_with_gaia.IsEmpty());
  }
}

// Tests that IsValid() returns true only when all mandatory fields are
// non-empty.
TEST_F(AccountInfoTest, IsValid) {
  AccountInfo info;
  EXPECT_EQ(signin::Tribool::kUnknown, info.is_child_account);
  EXPECT_FALSE(info.IsValid());

  info.gaia = info.email = "test_id";
  info.account_id = CoreAccountId::FromGaiaId("test_id");
  EXPECT_FALSE(info.IsValid());

  info.full_name = info.given_name = "test_name";
  info.hosted_domain = "test_domain";
  info.picture_url = "test_picture_url";
  EXPECT_TRUE(info.IsValid());
}

// Tests that UpdateWith() correctly ignores parameters with a different
// account / id.
TEST_F(AccountInfoTest, UpdateWithDifferentAccountId) {
  AccountInfo info;
  info.account_id = CoreAccountId::FromGaiaId("test_id");

  AccountInfo other;
  other.gaia = other.email = "test_other_id";
  other.account_id = CoreAccountId::FromGaiaId("test_other_id");

  EXPECT_FALSE(info.UpdateWith(other));
  EXPECT_TRUE(info.gaia.empty());
  EXPECT_TRUE(info.email.empty());
}

// Tests that UpdateWith() doesn't update the fields that were already set
// to the correct value.
TEST_F(AccountInfoTest, UpdateWithNoModification) {
  AccountInfo info;
  info.gaia = info.email = "test_id";
  info.account_id = CoreAccountId::FromGaiaId("test_id");
  info.is_child_account = signin::Tribool::kTrue;
  info.is_under_advanced_protection = true;
  info.locale = "en";
  info.access_point = signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS;

  AccountInfo other;
  other.account_id = CoreAccountId::FromGaiaId("test_id");
  other.gaia = other.email = "test_id";
  EXPECT_EQ(signin::Tribool::kUnknown, other.is_child_account);
  EXPECT_EQ(signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
            other.access_point);
  other.is_under_advanced_protection = false;
  other.locale = "en";

  EXPECT_FALSE(info.UpdateWith(other));
  EXPECT_EQ("test_id", info.gaia);
  EXPECT_EQ("test_id", info.email);
  EXPECT_EQ("en", info.locale);
  EXPECT_EQ(signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS,
            info.access_point);
  EXPECT_EQ(signin::Tribool::kTrue, info.is_child_account);
  EXPECT_TRUE(info.is_under_advanced_protection);
}

// Tests that UpdateWith() correctly updates its fields that were not set.
TEST_F(AccountInfoTest, UpdateWithSuccessfulUpdate) {
  AccountInfo info;
  info.gaia = info.email = "test_id";
  info.account_id = CoreAccountId::FromGaiaId("test_id");

  AccountInfo other;
  other.account_id = CoreAccountId::FromGaiaId("test_id");
  other.full_name = other.given_name = "test_name";
  other.locale = "fr";
  other.is_child_account = signin::Tribool::kTrue;
  other.access_point = signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS;
  AccountCapabilitiesTestMutator mutator(&other.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);

  EXPECT_TRUE(info.UpdateWith(other));
  EXPECT_EQ("test_id", info.gaia);
  EXPECT_EQ("test_id", info.email);
  EXPECT_EQ("test_name", info.full_name);
  EXPECT_EQ("test_name", info.given_name);
  EXPECT_EQ("fr", info.locale);
  EXPECT_EQ(signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS,
            info.access_point);
  EXPECT_EQ(signin::Tribool::kTrue, info.is_child_account);
  EXPECT_EQ(
      signin::Tribool::kTrue,
      info.capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions());
}

// Tests that UpdateWith() sets default values for hosted_domain and
// picture_url if the properties are unset.
TEST_F(AccountInfoTest, UpdateWithDefaultValues) {
  AccountInfo info;
  info.gaia = info.email = "test_id";
  info.account_id = CoreAccountId::FromGaiaId("test_id");

  AccountInfo other;
  other.account_id = CoreAccountId::FromGaiaId("test_id");
  other.hosted_domain = kNoHostedDomainFound;
  other.picture_url = kNoPictureURLFound;

  EXPECT_TRUE(info.UpdateWith(other));
  EXPECT_EQ(kNoHostedDomainFound, info.hosted_domain);
  EXPECT_EQ(kNoPictureURLFound, info.picture_url);
}

// Tests that UpdateWith() ignores default values for hosted_domain and
// picture_url if they are already set.
TEST_F(AccountInfoTest, UpdateWithDefaultValuesNoOverride) {
  AccountInfo info;
  info.gaia = info.email = "test_id";
  info.account_id = CoreAccountId::FromGaiaId("test_id");
  info.hosted_domain = "test_domain";
  info.picture_url = "test_url";

  AccountInfo other;
  other.account_id = CoreAccountId::FromGaiaId("test_id");
  other.hosted_domain = kNoHostedDomainFound;
  other.picture_url = kNoPictureURLFound;

  EXPECT_FALSE(info.UpdateWith(other));
  EXPECT_EQ("test_domain", info.hosted_domain);
  EXPECT_EQ("test_url", info.picture_url);
}
