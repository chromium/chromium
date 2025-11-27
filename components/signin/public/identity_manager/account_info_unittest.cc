// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_info.h"

#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

using signin::constants::kNoHostedDomainFound;

class AccountInfoTest : public testing::Test {};

TEST_F(AccountInfoTest, IsEmpty) {
  {
    AccountInfo info_empty;
    EXPECT_TRUE(info_empty.IsEmpty());
  }
  {
    AccountInfo info_with_account_id;
    info_with_account_id.account_id =
        CoreAccountId::FromGaiaId(GaiaId("test_id"));
    EXPECT_FALSE(info_with_account_id.IsEmpty());
  }
  {
    AccountInfo info_with_email;
    info_with_email.email = "test_email@email.com";
    EXPECT_FALSE(info_with_email.IsEmpty());
  }
  {
    AccountInfo info_with_gaia;
    info_with_gaia.gaia = GaiaId("test_gaia");
    EXPECT_FALSE(info_with_gaia.IsEmpty());
  }
}

TEST_F(AccountInfoTest, DefaultIsInvalid) {
  AccountInfo empty_info;
  EXPECT_EQ(signin::Tribool::kUnknown, empty_info.is_child_account);
  EXPECT_FALSE(empty_info.IsValid());
}

// Tests that IsValid() returns true when all mandatory fields are non-empty.
TEST_F(AccountInfoTest, IsValid) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test_id")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetFullName("test_name")
          .SetGivenName("test_name")
          .SetHostedDomain("test_domain")
          .SetAvatarUrl("test_picture_url")
          .Build();
  EXPECT_TRUE(info.IsValid());
}

// Tests that UpdateWith() correctly ignores parameters with a different
// account / id.
TEST_F(AccountInfoTest, UpdateWithDifferentAccountId) {
  AccountInfo info;
  info.account_id = CoreAccountId::FromGaiaId(GaiaId("test_id"));

  AccountInfo other;
  other.gaia = GaiaId("test_other_id");
  other.email = "test_other_id";
  other.account_id = CoreAccountId::FromGaiaId(other.gaia);

  EXPECT_FALSE(info.UpdateWith(other));
  EXPECT_TRUE(info.gaia.empty());
  EXPECT_TRUE(info.email.empty());
}

// Tests that UpdateWith() doesn't update the fields that were already set
// to the correct value.
TEST_F(AccountInfoTest, UpdateWithNoModification) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetIsChildAccount(signin::Tribool::kTrue)
          .SetIsUnderAdvancedProtection(true)
          .SetLocale("en")
          .SetLastAuthenticationAccessPoint(
              signin_metrics::AccessPoint::kSettings)
          .Build();

  AccountInfo other =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetIsUnderAdvancedProtection(false)
          .SetLocale("en")
          .Build();
  EXPECT_EQ(signin::Tribool::kUnknown, other.is_child_account);
  EXPECT_EQ(signin_metrics::AccessPoint::kUnknown, other.access_point);

  EXPECT_FALSE(info.UpdateWith(other));
  EXPECT_EQ(GaiaId("test_id"), info.gaia);
  EXPECT_EQ("test@example.com", info.email);
  EXPECT_EQ("en", info.locale);
  EXPECT_EQ(signin_metrics::AccessPoint::kSettings, info.access_point);
  EXPECT_EQ(signin::Tribool::kTrue, info.is_child_account);
  EXPECT_TRUE(info.is_under_advanced_protection);
}

// Tests that UpdateWith() correctly updates its fields that were not set.
TEST_F(AccountInfoTest, UpdateWithSuccessfulUpdate) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .Build();

  AccountInfo other =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetFullName("test_name")
          .SetGivenName("test_name")
          .SetLocale("fr")
          .SetIsChildAccount(signin::Tribool::kTrue)
          .SetLastAuthenticationAccessPoint(
              signin_metrics::AccessPoint::kSettings)
          .Build();
  AccountCapabilitiesTestMutator mutator(&other.capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);

  EXPECT_TRUE(info.UpdateWith(other));
  EXPECT_EQ(GaiaId("test_id"), info.gaia);
  EXPECT_EQ("test@example.com", info.email);
  EXPECT_EQ("test_name", info.full_name);
  EXPECT_EQ("test_name", info.given_name);
  EXPECT_EQ("fr", info.locale);
  EXPECT_EQ(signin_metrics::AccessPoint::kSettings, info.access_point);
  EXPECT_EQ(signin::Tribool::kTrue, info.is_child_account);
  EXPECT_EQ(
      signin::Tribool::kTrue,
      info.capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions());
}

// Tests that UpdateWith() sets default values for hosted_domain and
// picture_url if the properties are unset.
TEST_F(AccountInfoTest, UpdateWithDefaultValues) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .Build();

  AccountInfo other =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetHostedDomain(std::string())
          .SetAvatarUrl(kNoPictureURLFound)
          .Build();

  EXPECT_TRUE(info.UpdateWith(other));
  EXPECT_EQ(info.GetHostedDomain(), "");
  EXPECT_EQ(kNoPictureURLFound, info.picture_url);
}

// Tests that UpdateWith() ignores default values for hosted_domain and
// picture_url if they are already set.
TEST_F(AccountInfoTest, UpdateWithDefaultValuesNoOverride) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetHostedDomain("test_domain")
          .SetAvatarUrl("test_url")
          .Build();
  AccountCapabilitiesTestMutator(&info.capabilities)
      .set_is_subject_to_enterprise_features(true);

  AccountInfo other =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetHostedDomain(std::string())
          .SetAvatarUrl(kNoPictureURLFound)
          .Build();

  EXPECT_FALSE(info.UpdateWith(other));
  EXPECT_EQ(info.GetHostedDomain(), "test_domain");
  EXPECT_EQ(info.picture_url, "test_url");
}

TEST_F(AccountInfoTest, BuilderPopulatesCoreAccountInfoFields) {
  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("test_id")))
          .SetIsUnderAdvancedProtection(true)
          .Build();

  EXPECT_EQ(info.gaia, GaiaId("test_id"));
  EXPECT_EQ(info.email, "test@example.com");
  EXPECT_EQ(info.account_id, CoreAccountId::FromGaiaId(GaiaId("test_id")));
  EXPECT_TRUE(info.is_under_advanced_protection);
}

TEST_F(AccountInfoTest, GettersEmptyAccountInfo) {
  AccountInfo info;
  EXPECT_EQ(info.GetFullName(), std::nullopt);
  EXPECT_EQ(info.GetGivenName(), std::nullopt);
  EXPECT_EQ(info.GetHostedDomain(), std::nullopt);
  EXPECT_EQ(info.GetAvatarUrl(), std::nullopt);
  EXPECT_EQ(info.GetLastDownloadedAvatarUrlWithSize(), std::nullopt);
  EXPECT_EQ(info.GetAvatarImage(), std::nullopt);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_EQ(info.GetLastAuthenticationAccessPoint(),
            signin_metrics::AccessPoint::kUnknown);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_FALSE(info.GetAccountCapabilities().AreAnyCapabilitiesKnown());
  EXPECT_EQ(info.IsChildAccount(), signin::Tribool::kUnknown);
  EXPECT_EQ(info.GetLocale(), std::nullopt);
}

TEST_F(AccountInfoTest, GettersPopulatedAccountInfo) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);

  AccountInfo info =
      AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
          .SetFullName("full_name")
          .SetGivenName("given_name")
          .SetHostedDomain("hosted_domain")
          .SetAvatarUrl("picture_url")
          .SetLastDownloadedAvatarUrlWithSize("picture_url_with_size")
          .SetAvatarImage(gfx::test::CreateImage(/*size*/ 24))
          .SetLastAuthenticationAccessPoint(
              signin_metrics::AccessPoint::kSettings)
          .UpdateAccountCapabilitiesWith(capabilities)
          .SetIsChildAccount(signin::Tribool::kFalse)
          .SetLocale("fr")
          .Build();

  EXPECT_EQ(info.GetFullName(), "full_name");
  EXPECT_EQ(info.GetGivenName(), "given_name");
  EXPECT_EQ(info.GetHostedDomain(), "hosted_domain");
  EXPECT_EQ(info.GetAvatarUrl(), "picture_url");
  EXPECT_EQ(info.GetLastDownloadedAvatarUrlWithSize(), "picture_url_with_size");
  EXPECT_NE(info.GetAvatarImage(), std::nullopt);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_EQ(info.GetLastAuthenticationAccessPoint(),
            signin_metrics::AccessPoint::kSettings);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_TRUE(info.GetAccountCapabilities().AreAnyCapabilitiesKnown());
  EXPECT_EQ(info.IsChildAccount(), signin::Tribool::kFalse);
  EXPECT_EQ(info.GetLocale(), "fr");
}

TEST_F(AccountInfoTest, DeprecatedSentinelValues) {
  AccountInfo info = AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
                         .SetHostedDomain(kNoHostedDomainFound)
                         .SetAvatarUrl(kNoPictureURLFound)
                         .Build();

  EXPECT_EQ(info.GetHostedDomain(), std::string());
  EXPECT_EQ(info.GetAvatarUrl(), std::string());
}

TEST_F(AccountInfoTest, EmptyTheSameAsDeprecatedSentinelValues) {
  AccountInfo info = AccountInfo::Builder(GaiaId("test_id"), "test@example.com")
                         .SetHostedDomain(std::string())
                         .SetAvatarUrl(std::string())
                         .Build();

  EXPECT_EQ(info.GetHostedDomain(), std::string());
  EXPECT_EQ(info.GetAvatarUrl(), std::string());
}

TEST_F(AccountInfoTest, CreateWithPossiblyEmptyGaiaId) {
  AccountInfo info = AccountInfo::Builder::CreateWithPossiblyEmptyGaiaId(
                         GaiaId(), "test@example.org")
                         .Build();

  EXPECT_TRUE(info.gaia.empty());
  EXPECT_EQ(info.email, "test@example.org");
}
