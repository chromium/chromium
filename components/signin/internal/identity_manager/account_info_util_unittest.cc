// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_info_util.h"

#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "testing/platform_test.h"

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
                                           const char* picture_url) {
  base::Value::Dict user_info;
  if (email)
    user_info.Set("email", base::Value(email));

  if (gaia)
    user_info.Set("id", base::Value(gaia));

  if (hosted_domain)
    user_info.Set("hd", base::Value(hosted_domain));

  if (full_name)
    user_info.Set("name", base::Value(full_name));

  if (given_name)
    user_info.Set("given_name", base::Value(given_name));

  if (locale)
    user_info.Set("locale", base::Value(locale));

  if (picture_url)
    user_info.Set("picture", base::Value(picture_url));

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

}  // namespace

using AccountInfoUtilTest = PlatformTest;

// Tests that AccountInfoFromUserInfo returns an AccountInfo with the value
// extracted from the passed base::Value.
TEST_F(AccountInfoUtilTest, FromUserInfo) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/"gaia_id_user_example_com",
          /*hosted_domain=*/"example.com", /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user"));

  ASSERT_TRUE(maybe_account_info.has_value());

  AccountInfo& account_info = maybe_account_info.value();
  ASSERT_EQ(account_info.email, "user@example.com");
  ASSERT_EQ(account_info.gaia, "gaia_id_user_example_com");
  ASSERT_EQ(account_info.hosted_domain, "example.com");
  ASSERT_EQ(account_info.full_name, "full name");
  ASSERT_EQ(account_info.given_name, "given name");
  ASSERT_EQ(account_info.locale, "locale");
  ASSERT_EQ(account_info.picture_url, "https://example.com/picture/user");
}

// Tests that AccountInfoFromUserInfo returns an AccountInfo with empty or
// default values if no fields are set in the user_info except for email or
// gaia id.
TEST_F(AccountInfoUtilTest, FromUserInfo_EmptyValues) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/"gaia_id_user_example_com",
          /*hosted_domain=*/"", /*full_name=*/"",
          /*given_name=*/"", /*locale=*/"", /*picture_url=*/""));

  ASSERT_TRUE(maybe_account_info.has_value());

  AccountInfo& account_info = maybe_account_info.value();
  ASSERT_EQ(account_info.email, "user@example.com");
  ASSERT_EQ(account_info.gaia, "gaia_id_user_example_com");
  ASSERT_EQ(account_info.hosted_domain, kNoHostedDomainFound);
  ASSERT_EQ(account_info.full_name, std::string());
  ASSERT_EQ(account_info.given_name, std::string());
  ASSERT_EQ(account_info.locale, std::string());
  ASSERT_EQ(account_info.picture_url, kNoPictureURLFound);
}

// Tests that AccountInfoFromUserInfo returns an AccountInfo with the value
// extracted from the passed base::Value, with default value for |hosted_domain|
// if missing.
TEST_F(AccountInfoUtilTest, FromUserInfo_NoHostedDomain) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/"gaia_id_user_example_com",
          /*hosted_domain=*/nullptr, /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user"));

  ASSERT_TRUE(maybe_account_info.has_value());

  AccountInfo& account_info = maybe_account_info.value();
  ASSERT_EQ(account_info.hosted_domain, kNoHostedDomainFound);
}

// Tests that AccountInfoFromUserInfo returns an AccountInfo with the value
// extracted from the passed base::Value, with default value for |picture_url|
// if missing.
TEST_F(AccountInfoUtilTest, FromUserInfo_NoPictureUrl) {
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
TEST_F(AccountInfoUtilTest, FromUserInfo_NoEmail) {
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
TEST_F(AccountInfoUtilTest, FromUserInfo_EmptyEmail) {
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
TEST_F(AccountInfoUtilTest, FromUserInfo_NoGaiaId) {
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
TEST_F(AccountInfoUtilTest, FromUserInfo_EmptyGaiaId) {
  std::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/"",
          /*hosted_domain=*/"example.com", /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user"));

  EXPECT_FALSE(maybe_account_info.has_value());
}

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue) {
  std::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(CreateAccountCapabilitiesValue(
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

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue_EmptyList) {
  std::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(CreateAccountCapabilitiesValue({}));

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

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue_SeveralCapabilities) {
  std::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(CreateAccountCapabilitiesValue(
          {{"testcapability", true},
           {kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName,
            false}}));

  ASSERT_TRUE(capabilities.has_value());
  EXPECT_EQ(
      capabilities
          ->can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      signin::Tribool::kFalse);
}

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue_NonBooleanValue) {
  base::Value::Dict dict;
  base::Value* list = dict.Set("accountCapabilities", base::Value::List());
  base::Value::Dict entry;
  entry.Set(
      "name",
      kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName);
  entry.Set("intValue", 42);
  list->GetList().Append(std::move(entry));

  std::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(dict);

  ASSERT_TRUE(capabilities.has_value());
  EXPECT_EQ(
      capabilities
          ->can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      signin::Tribool::kUnknown);
}

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue_DoesNotContainList) {
  base::Value::Dict dict;
  dict.Set("accountCapabilities", base::Value::Dict());

  std::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(dict);

  EXPECT_FALSE(capabilities.has_value());
}

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue_NameNotFound) {
  base::Value::Dict dict;
  base::Value* list = dict.Set("accountCapabilities", base::Value::List());
  base::Value::Dict entry;
  entry.Set("booleanValue", true);
  list->GetList().Append(std::move(entry));

  std::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(dict);

  EXPECT_FALSE(capabilities.has_value());
}
