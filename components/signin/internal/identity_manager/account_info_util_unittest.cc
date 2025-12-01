// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_info_util.h"

#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {
// Returns a base::Value::Dict corresponding to the user info as would be
// returned by gaia server with provided values (if null is passed for a value,
// it will not be set in the returned user_info object).
base::Value::Dict CreateUserInfoWithValues(const char* email,
                                           const char* gaia,
                                           const char* hosted_domain,
                                           const char* full_name,
                                           const char* given_name,
                                           const char* locale,
                                           const char* picture_url,
                                           const char* sub = nullptr) {
  base::Value::Dict user_info;
  if (email) {
    user_info.Set("email", base::Value(email));
  }

  if (gaia) {
    user_info.Set("id", base::Value(gaia));
  }

  if (hosted_domain) {
    user_info.Set("hd", base::Value(hosted_domain));
  }

  if (full_name) {
    user_info.Set("name", base::Value(full_name));
  }

  if (given_name) {
    user_info.Set("given_name", base::Value(given_name));
  }

  if (locale) {
    user_info.Set("locale", base::Value(locale));
  }

  if (picture_url) {
    user_info.Set("picture", base::Value(picture_url));
  }

  if (sub) {
    user_info.Set("sub", base::Value(sub));
  }

  return user_info;
}

base::Value::Dict CreateAccountCapabilitiesValue(
    const std::vector<std::pair<std::string, bool>>& capabilities) {
  base::Value::Dict dict;
  base::Value* list = dict.Set("accountCapabilities", base::Value::List());

  for (const auto& capability : capabilities) {
    base::Value::Dict entry;
    entry.Set("name", capability.first);
    entry.Set("booleanValue", capability.second);
    list->GetList().Append(std::move(entry));
  }

  return dict;
}

// Tests that AccountInfoFromUserInfo returns an AccountInfo with the value
// extracted from the passed base::Value.
TEST(AccountInfoUtilTest, FromUserInfo) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/"gaia_id_user_example_com",
          /*hosted_domain=*/"example.com", /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user"));

  ASSERT_TRUE(maybe_account_info.has_value());

  AccountInfo& account_info = maybe_account_info.value();
  ASSERT_EQ(account_info.email, "user@example.com");
  ASSERT_EQ(account_info.gaia.ToString(), "gaia_id_user_example_com");
  ASSERT_EQ(account_info.GetHostedDomain(), "example.com");
  ASSERT_EQ(account_info.full_name, "full name");
  ASSERT_EQ(account_info.given_name, "given name");
  ASSERT_EQ(account_info.locale, "locale");
  ASSERT_EQ(account_info.picture_url, "https://example.com/picture/user");
}

// Tests that AccountInfoFromUserInfo returns an AccountInfo with the value
// extracted from the passed base::Value when the GAIA ID is stored in
// the "sub" value.
TEST(AccountInfoUtilTest, FromUserInfoWithSub) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/nullptr,
          /*hosted_domain=*/"example.com", /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user",
          /*sub=*/"gaia_id_user_example_com"));

  ASSERT_TRUE(maybe_account_info.has_value());

  AccountInfo& account_info = maybe_account_info.value();
  ASSERT_EQ(account_info.email, "user@example.com");
  ASSERT_EQ(account_info.gaia.ToString(), "gaia_id_user_example_com");
  ASSERT_EQ(account_info.GetHostedDomain(), "example.com");
  ASSERT_EQ(account_info.full_name, "full name");
  ASSERT_EQ(account_info.given_name, "given name");
  ASSERT_EQ(account_info.locale, "locale");
  ASSERT_EQ(account_info.picture_url, "https://example.com/picture/user");
}

// Tests that AccountInfoFromUserInfo returns an AccountInfo with the value
// extracted from the passed base::Value, and that the GAIA ID stored in "id"
// takes precedence over the "sub" value.
TEST(AccountInfoUtilTest, FromUserInfoWithIdAndSub) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/"gaia_id_user_example_com",
          /*hosted_domain=*/"example.com", /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user",
          /*sub=*/"gaia_sub_user_example_com"));

  ASSERT_TRUE(maybe_account_info.has_value());

  AccountInfo& account_info = maybe_account_info.value();
  ASSERT_EQ(account_info.email, "user@example.com");
  ASSERT_EQ(account_info.gaia.ToString(), "gaia_id_user_example_com");
  ASSERT_EQ(account_info.GetHostedDomain(), "example.com");
  ASSERT_EQ(account_info.full_name, "full name");
  ASSERT_EQ(account_info.given_name, "given name");
  ASSERT_EQ(account_info.locale, "locale");
  ASSERT_EQ(account_info.picture_url, "https://example.com/picture/user");
}

// Tests that AccountInfoFromUserInfo returns an AccountInfo with empty or
// default values if no fields are set in the user_info except for email or
// gaia id.
TEST(AccountInfoUtilTest, FromUserInfo_EmptyValues) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/"gaia_id_user_example_com",
          /*hosted_domain=*/"", /*full_name=*/"",
          /*given_name=*/"", /*locale=*/"", /*picture_url=*/""));

  ASSERT_TRUE(maybe_account_info.has_value());

  AccountInfo& account_info = maybe_account_info.value();
  ASSERT_EQ(account_info.email, "user@example.com");
  ASSERT_EQ(account_info.gaia.ToString(), "gaia_id_user_example_com");
  ASSERT_EQ(account_info.GetHostedDomain(), std::string());
  ASSERT_EQ(account_info.full_name, std::string());
  ASSERT_EQ(account_info.given_name, std::string());
  ASSERT_EQ(account_info.locale, std::string());
  ASSERT_EQ(account_info.picture_url, kNoPictureURLFound);
}

// Tests that AccountInfoFromUserInfo returns an AccountInfo with the value
// extracted from the passed base::Value, with default value for |hosted_domain|
// if missing.
TEST(AccountInfoUtilTest, FromUserInfo_NoHostedDomain) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/"gaia_id_user_example_com",
          /*hosted_domain=*/nullptr, /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user"));

  ASSERT_TRUE(maybe_account_info.has_value());

  AccountInfo& account_info = maybe_account_info.value();
  ASSERT_EQ(account_info.GetHostedDomain(), std::string());
}

// Tests that AccountInfoFromUserInfo returns an AccountInfo with the value
// extracted from the passed base::Value, with default value for |picture_url|
// if missing.
TEST(AccountInfoUtilTest, FromUserInfo_NoPictureUrl) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/"gaia_id_user_example_com",
          /*hosted_domain=*/"example.com", /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/nullptr));

  ASSERT_TRUE(maybe_account_info.has_value());

  AccountInfo& account_info = maybe_account_info.value();
  ASSERT_EQ(account_info.picture_url, kNoPictureURLFound);
}

// Tests that if AccountInfoFromUserInfo fails if the value passed has no
// value for |email|.
TEST(AccountInfoUtilTest, FromUserInfo_NoEmail) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/nullptr, /*gaia=*/"gaia_id_user_example_com",
          /*hosted_domain=*/"example.com", /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user"));

  EXPECT_FALSE(maybe_account_info.has_value());
}

// Tests that if AccountInfoFromUserInfo fails if the value passed has empty
// string as value for |email|.
TEST(AccountInfoUtilTest, FromUserInfo_EmptyEmail) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"", /*gaia=*/"gaia_id_user_example_com",
          /*hosted_domain=*/"example.com", /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user"));

  EXPECT_FALSE(maybe_account_info.has_value());
}

// Tests that if AccountInfoFromUserInfo fails if the value passed has no
// value for |gaia|.
TEST(AccountInfoUtilTest, FromUserInfo_NoGaiaId) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/nullptr,
          /*hosted_domain=*/"example.com", /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user"));

  EXPECT_FALSE(maybe_account_info.has_value());
}

// Tests that if AccountInfoFromUserInfo fails if the value passed has empty
// string as value for |gaia|.
TEST(AccountInfoUtilTest, FromUserInfo_EmptyGaiaId) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/"",
          /*hosted_domain=*/"example.com", /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user"));

  EXPECT_FALSE(maybe_account_info.has_value());
}

TEST(AccountInfoUtilTest, AccountCapabilitiesFromServerResponse) {
  std::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromServerResponse(CreateAccountCapabilitiesValue(
          {{kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName,
            true}}));

  ASSERT_TRUE(capabilities.has_value());
  EXPECT_EQ(
      capabilities
          ->can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      signin::Tribool::kTrue);
  EXPECT_EQ(
      capabilities
          ->can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      signin::Tribool::kTrue);
}

TEST(AccountInfoUtilTest, AccountCapabilitiesFromServerResponse_EmptyList) {
  std::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromServerResponse(CreateAccountCapabilitiesValue({}));

  ASSERT_TRUE(capabilities.has_value());
  EXPECT_EQ(
      capabilities
          ->can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      signin::Tribool::kUnknown);
  EXPECT_EQ(
      capabilities
          ->can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      signin::Tribool::kUnknown);
}

TEST(AccountInfoUtilTest,
     AccountCapabilitiesFromServerResponse_SeveralCapabilities) {
  std::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromServerResponse(CreateAccountCapabilitiesValue(
          {{"testcapability", true},
           {kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName,
            false}}));

  ASSERT_TRUE(capabilities.has_value());
  EXPECT_EQ(
      capabilities
          ->can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      signin::Tribool::kFalse);
}

TEST(AccountInfoUtilTest,
     AccountCapabilitiesFromServerResponse_NonBooleanValue) {
  base::Value::Dict dict;
  base::Value* list = dict.Set("accountCapabilities", base::Value::List());
  base::Value::Dict entry;
  entry.Set(
      "name",
      kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName);
  entry.Set("intValue", 42);
  list->GetList().Append(std::move(entry));

  std::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromServerResponse(dict);

  ASSERT_TRUE(capabilities.has_value());
  EXPECT_EQ(
      capabilities
          ->can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      signin::Tribool::kUnknown);
}

TEST(AccountInfoUtilTest,
     AccountCapabilitiesFromServerResponse_DoesNotContainList) {
  base::Value::Dict dict;
  dict.Set("accountCapabilities", base::Value::Dict());

  std::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromServerResponse(dict);

  EXPECT_FALSE(capabilities.has_value());
}

TEST(AccountInfoUtilTest, AccountCapabilitiesFromServerResponse_NameNotFound) {
  base::Value::Dict dict;
  base::Value* list = dict.Set("accountCapabilities", base::Value::List());
  base::Value::Dict entry;
  entry.Set("booleanValue", true);
  list->GetList().Append(std::move(entry));

  std::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromServerResponse(dict);

  EXPECT_FALSE(capabilities.has_value());
}

TEST(AccountInfoUtilTest, SerializeAndDeserializeAccountCapabilities) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      false);

  base::Value::Dict dict = SerializeAccountCapabilities(capabilities);
  AccountCapabilities deserialized_capabilities =
      DeserializeAccountCapabilities(dict);

  EXPECT_EQ(deserialized_capabilities.is_subject_to_parental_controls(),
            Tribool::kTrue);
  EXPECT_EQ(
      deserialized_capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      Tribool::kFalse);
  EXPECT_EQ(deserialized_capabilities.can_fetch_family_member_info(),
            Tribool::kUnknown);
}

TEST(AccountInfoUtilTest, DeserializeAccountCapabilities_Empty) {
  base::Value::Dict dict;
  AccountCapabilities capabilities = DeserializeAccountCapabilities(dict);
  EXPECT_FALSE(capabilities.AreAnyCapabilitiesKnown());
}

TEST(AccountInfoUtilTest, DeserializeAccountCapabilities_UnknownCapability) {
  base::Value::Dict dict;
  dict.Set("unknown_capability", 1);
  AccountCapabilities capabilities = DeserializeAccountCapabilities(dict);
  EXPECT_FALSE(capabilities.AreAnyCapabilitiesKnown());
}

// Test to ensure the stability of the serialized format for AccountCapabilities
// as it's stored by Chrome clients.
//
// The dictionary format in this test shouldn't be modified unless there is a
// clear migration path to a new format.
TEST(AccountInfoUtilTest, DeserializeAccountCapabilities_FormatStability) {
  auto dict = base::Value::Dict()
                  .Set("accountcapabilities/guydolldmfya", 0)
                  .Set("accountcapabilities/gi2tklldmfya", 1);

  AccountCapabilities capabilities = DeserializeAccountCapabilities(dict);

  EXPECT_EQ(capabilities.is_subject_to_parental_controls(), Tribool::kFalse);
  EXPECT_EQ(
      capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      Tribool::kTrue);
  EXPECT_EQ(capabilities.can_fetch_family_member_info(), Tribool::kUnknown);
}

TEST(AccountInfoUtilTest, SerializeAndDeserializeAccountInfo) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      false);
  AccountInfo account_info =
      AccountInfo::Builder(GaiaId("test_gaia_id"), "test_email@example.com")
          .SetAccountId(CoreAccountId::FromString("test_account_id"))
          .SetIsUnderAdvancedProtection(true)
          .SetFullName("Test Full Name")
          .SetGivenName("Test Given Name")
          .SetHostedDomain("example.com")
          .SetLocale("en-US")
          .SetAvatarUrl("https://example.com/picture.jpg")
          .SetIsChildAccount(signin::Tribool::kTrue)
          .SetLastDownloadedAvatarUrlWithSize(
              "https://example.com/picture_with_size.jpg")
          .SetLastAuthenticationAccessPoint(
              signin_metrics::AccessPoint::kAvatarBubbleSignIn)
          .UpdateAccountCapabilitiesWith(capabilities)
          .Build();

  base::Value::Dict dict = SerializeAccountInfo(account_info);
  std::optional<AccountInfo> deserialized_account_info =
      DeserializeAccountInfo(dict);

  ASSERT_TRUE(deserialized_account_info.has_value());
  EXPECT_EQ(account_info.account_id, deserialized_account_info->account_id);
  EXPECT_EQ(account_info.gaia, deserialized_account_info->gaia);
  EXPECT_EQ(account_info.email, deserialized_account_info->email);
  EXPECT_EQ(account_info.is_under_advanced_protection,
            deserialized_account_info->is_under_advanced_protection);
  EXPECT_EQ(account_info.GetFullName(),
            deserialized_account_info->GetFullName());
  EXPECT_EQ(account_info.GetGivenName(),
            deserialized_account_info->GetGivenName());
  EXPECT_EQ(account_info.GetHostedDomain(),
            deserialized_account_info->GetHostedDomain());
  EXPECT_EQ(account_info.GetLocale(), deserialized_account_info->GetLocale());
  EXPECT_EQ(account_info.GetAvatarUrl(),
            deserialized_account_info->GetAvatarUrl());
  EXPECT_EQ(account_info.IsChildAccount(),
            deserialized_account_info->IsChildAccount());
  EXPECT_EQ(account_info.GetLastDownloadedAvatarUrlWithSize(),
            deserialized_account_info->GetLastDownloadedAvatarUrlWithSize());
  EXPECT_EQ(account_info.access_point, deserialized_account_info->access_point);
  EXPECT_EQ(
      account_info.GetAccountCapabilities().can_fetch_family_member_info(),
      Tribool::kUnknown);
  EXPECT_EQ(
      account_info.GetAccountCapabilities().is_subject_to_parental_controls(),
      Tribool::kTrue);
  EXPECT_EQ(
      account_info.GetAccountCapabilities()
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      Tribool::kFalse);
}

// Test to ensure the stability of the serialized format as it's stored by
// Chrome clients.
//
// The dictionary format in this test shouldn't be modified unless there is a
// clear migration path to a new format.
TEST(AccountInfoUtilTest, DeserializeAccountInfo_FormatStability) {
  auto dict = base::Value::Dict()
                  .Set("access_point", 31)
                  .Set("account_id", "test_account_id")
                  .Set("accountcapabilities",
                       base::Value::Dict()
                           .Set("accountcapabilities/guydolldmfya", 0)
                           .Set("accountcapabilities/gi2tklldmfya", 1))
                  .Set("email", "test@example.com")
                  .Set("full_name", "Test Name")
                  .Set("gaia", "test_gaia_id")
                  .Set("given_name", "Test")
                  .Set("hd", "example.com")
                  .Set("is_supervised_child", 0)
                  .Set("is_under_advanced_protection", false)
                  .Set("last_downloaded_image_url_with_size",
                       "https://example.com/a/my_pic=s256-c-ns")
                  .Set("locale", "en")
                  .Set("picture_url", "https://example.com/a/my_pic=s96-c");
  std::optional<AccountInfo> account_info = DeserializeAccountInfo(dict);

  ASSERT_NE(account_info, std::nullopt);
  EXPECT_EQ(account_info->gaia, GaiaId("test_gaia_id"));
  EXPECT_EQ(account_info->email, "test@example.com");
  EXPECT_EQ(account_info->account_id,
            CoreAccountId::FromString("test_account_id"));
  EXPECT_EQ(account_info->is_under_advanced_protection, false);
  EXPECT_EQ(account_info->GetFullName(), "Test Name");
  EXPECT_EQ(account_info->GetGivenName(), "Test");
  EXPECT_EQ(account_info->GetHostedDomain(), "example.com");
  EXPECT_EQ(account_info->IsChildAccount(), Tribool::kFalse);
  EXPECT_EQ(account_info->GetLastDownloadedAvatarUrlWithSize(),
            "https://example.com/a/my_pic=s256-c-ns");
  EXPECT_EQ(account_info->GetAvatarUrl(), "https://example.com/a/my_pic=s96-c");
  EXPECT_EQ(account_info->GetLocale(), "en");
  EXPECT_EQ(
      account_info->GetAccountCapabilities().is_subject_to_parental_controls(),
      Tribool::kFalse);
  EXPECT_EQ(
      account_info->GetAccountCapabilities()
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      Tribool::kTrue);
  EXPECT_EQ(
      account_info->GetAccountCapabilities().can_fetch_family_member_info(),
      Tribool::kUnknown);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_EQ(account_info->GetLastAuthenticationAccessPoint(),
            signin_metrics::AccessPoint::kWebSignin);
#endif
}

TEST(AccountInfoUtilTest, DeserializeAccountInfo_Minimal) {
  auto dict = base::Value::Dict()
                  .Set("account_id", "test_account_id")
                  .Set("gaia", "gaia_id")
                  .Set("email", "test@example.org");
  std::optional<AccountInfo> account_info = DeserializeAccountInfo(dict);
  ASSERT_NE(account_info, std::nullopt);
  EXPECT_EQ(account_info->gaia, GaiaId("gaia_id"));
  EXPECT_EQ(account_info->email, "test@example.org");
  EXPECT_EQ(account_info->account_id,
            CoreAccountId::FromString("test_account_id"));
  // All other fields should be unknown.
  EXPECT_EQ(account_info->GetFullName(), std::nullopt);
  EXPECT_FALSE(
      account_info->GetAccountCapabilities().AreAnyCapabilitiesKnown());
}

TEST(AccountInfoUtilTest, DeserializeAccountInfo_EmptyDict) {
  base::Value::Dict dict;
  EXPECT_EQ(DeserializeAccountInfo(dict), std::nullopt);
}

TEST(AccountInfoUtilTest, DeserializeAccountInfo_NoAccountId) {
  auto dict = base::Value::Dict()
                  .Set("account_id", "")
                  .Set("gaia", "gaia_id")
                  .Set("email", "test@example.org");
  EXPECT_EQ(DeserializeAccountInfo(dict), std::nullopt);
}

TEST(AccountInfoUtilTest, DeserializeAccountInfo_NoGaia) {
  auto dict = base::Value::Dict()
                  .Set("account_id", "test_account_id")
                  .Set("gaia", "")
                  .Set("email", "test@example.org");
  std::optional<AccountInfo> account_info = DeserializeAccountInfo(dict);
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_NE(account_info, std::nullopt);
  EXPECT_EQ(account_info->gaia, GaiaId());
  EXPECT_EQ(account_info->email, "test@example.org");
  EXPECT_EQ(account_info->account_id,
            CoreAccountId::FromString("test_account_id"));
#else
  EXPECT_EQ(account_info, std::nullopt);
#endif
}

TEST(AccountInfoUtilTest, DeserializeAccountInfo_NoEmail) {
  auto dict = base::Value::Dict()
                  .Set("account_id", "test_account_id")
                  .Set("gaia", "gaia_id")
                  .Set("email", "");
  EXPECT_EQ(DeserializeAccountInfo(dict), std::nullopt);
}

TEST(AccountInfoUtilTest, DeserializeAccountInfo_EmptyStringValues) {
  // Tests that empty strings in the dictionary are ignored.
  auto dict = base::Value::Dict()
                  .Set("account_id", "test_account_id")
                  .Set("gaia", "gaia_id")
                  .Set("email", "test@example.org")
                  .Set("full_name", "")
                  .Set("given_name", "")
                  .Set("locale", "")
                  .Set("last_downloaded_image_url_with_size", "")
                  .Set("hd", "")
                  .Set("picture_url", "");

  std::optional<AccountInfo> account_info = DeserializeAccountInfo(dict);
  ASSERT_NE(account_info, std::nullopt);
  EXPECT_EQ(account_info->gaia, GaiaId("gaia_id"));
  EXPECT_EQ(account_info->email, "test@example.org");
  EXPECT_EQ(account_info->GetFullName(), std::nullopt);
  EXPECT_EQ(account_info->GetGivenName(), std::nullopt);
  EXPECT_EQ(account_info->GetLocale(), std::nullopt);
  EXPECT_EQ(account_info->GetLastDownloadedAvatarUrlWithSize(), std::nullopt);
  EXPECT_EQ(account_info->GetHostedDomain(), std::nullopt);
  EXPECT_EQ(account_info->GetAvatarUrl(), std::nullopt);
}

TEST(AccountInfoUtilTest, DeserializeAccountInfo_SentinelValues) {
  auto dict = base::Value::Dict()
                  .Set("account_id", "test_account_id")
                  .Set("gaia", "gaia_id")
                  .Set("email", "test@example.org")
                  .Set("hd", "NO_HOSTED_DOMAIN")
                  .Set("picture_url", "NO_PICTURE_URL");

  std::optional<AccountInfo> account_info = DeserializeAccountInfo(dict);
  ASSERT_NE(account_info, std::nullopt);
  EXPECT_EQ(account_info->gaia, GaiaId("gaia_id"));
  EXPECT_EQ(account_info->email, "test@example.org");
  EXPECT_EQ(account_info->GetHostedDomain(), std::string());
  EXPECT_EQ(account_info->GetAvatarUrl(), std::string());
}

}  // namespace

}  // namespace signin
