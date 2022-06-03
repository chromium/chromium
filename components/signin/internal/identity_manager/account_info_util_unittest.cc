// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_info_util.h"

#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "testing/platform_test.h"

namespace {
// Returns a base::Value corresponding to the user info as would be returned
// by gaia server with provided values (if null is passed for a value, it will
// not be set in the returned user_info object).
base::Value CreateUserInfoWithValues(const char* email,
                                     const char* gaia,
                                     const char* hosted_domain,
                                     const char* full_name,
                                     const char* given_name,
                                     const char* locale,
                                     const char* picture_url) {
  base::Value user_info(base::Value::Type::DICTIONARY);
  if (email)
    user_info.SetKey("email", base::Value(email));

  if (gaia)
    user_info.SetKey("id", base::Value(gaia));

  if (hosted_domain)
    user_info.SetKey("hd", base::Value(hosted_domain));

  if (full_name)
    user_info.SetKey("name", base::Value(full_name));

  if (given_name)
    user_info.SetKey("given_name", base::Value(given_name));

  if (locale)
    user_info.SetKey("locale", base::Value(locale));

  if (picture_url)
    user_info.SetKey("picture", base::Value(picture_url));

  return user_info;
}

base::Value CreateAccountCapabilitiesValue(
    const std::vector<std::pair<std::string, bool>>& capabilities) {
  base::Value dict(base::Value::Type::DICTIONARY);
  base::Value* list =
      dict.SetKey("accountCapabilities", base::Value(base::Value::Type::LIST));

  for (const auto& capability : capabilities) {
    base::Value entry(base::Value::Type::DICTIONARY);
    entry.SetStringKey("name", capability.first);
    entry.SetBoolKey("booleanValue", capability.second);
    list->Append(std::move(entry));
  }

  return dict;
}

}  // namespace

using AccountInfoUtilTest = PlatformTest;

// Tests that AccountInfoFromUserInfo returns an AccountInfo with the value
// extracted from the passed base::Value.
TEST_F(AccountInfoUtilTest, FromUserInfo) {
  absl::optional<AccountInfo> maybe_account_info =
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
// default values if no fields are set in the user_info.
TEST_F(AccountInfoUtilTest, FromUserInfo_EmptyValues) {
  absl::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"", /*gaia=*/"", /*hosted_domain=*/"", /*full_name=*/"",
          /*given_name=*/"", /*locale=*/"", /*picture_url=*/""));

  ASSERT_TRUE(maybe_account_info.has_value());

  AccountInfo& account_info = maybe_account_info.value();
  ASSERT_EQ(account_info.email, std::string());
  ASSERT_EQ(account_info.gaia, std::string());
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
  absl::optional<AccountInfo> maybe_account_info =
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
  absl::optional<AccountInfo> maybe_account_info =
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
  absl::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/nullptr, /*gaia=*/"gaia_id_user_example_com",
          /*hosted_domain=*/"example.com", /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user"));

  EXPECT_FALSE(maybe_account_info.has_value());
}

// Tests that if AccountInfoFromUserInfo fails if the value passed has no
// value for |gaia|.
TEST_F(AccountInfoUtilTest, FromUserInfo_NoGaiaId) {
  absl::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(CreateUserInfoWithValues(
          /*email=*/"user@example.com", /*gaia=*/nullptr,
          /*hosted_domain=*/"example.com", /*full_name=*/"full name",
          /*given_name=*/"given name", /*locale=*/"locale",
          /*picture_url=*/"https://example.com/picture/user"));

  EXPECT_FALSE(maybe_account_info.has_value());
}

// Tests that if AccountInfoFromUserInfo fails if the value passed is not a
// dictionary.
TEST_F(AccountInfoUtilTest, FromUserInfo_NotADictionary) {
  absl::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(base::Value("not a dictionary"));

  EXPECT_FALSE(maybe_account_info.has_value());
}

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue) {
  absl::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(CreateAccountCapabilitiesValue(
          {{kCanOfferExtendedChromeSyncPromosCapabilityName, true}}));

  ASSERT_TRUE(capabilities.has_value());
  EXPECT_EQ(capabilities->can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kTrue);
}

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue_EmptyList) {
  absl::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(CreateAccountCapabilitiesValue({}));

  ASSERT_TRUE(capabilities.has_value());
  EXPECT_EQ(capabilities->can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kUnknown);
}

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue_SeveralCapabilities) {
  absl::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(CreateAccountCapabilitiesValue(
          {{"testcapability", true},
           {kCanOfferExtendedChromeSyncPromosCapabilityName, false}}));

  ASSERT_TRUE(capabilities.has_value());
  EXPECT_EQ(capabilities->can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kFalse);
}

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue_NonBooleanValue) {
  base::Value value(base::Value::Type::DICTIONARY);
  base::Value* list =
      value.SetKey("accountCapabilities", base::Value(base::Value::Type::LIST));
  base::Value entry(base::Value::Type::DICTIONARY);
  entry.SetStringKey("name", kCanOfferExtendedChromeSyncPromosCapabilityName);
  entry.SetIntKey("intValue", 42);
  list->Append(std::move(entry));

  absl::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(value);

  ASSERT_TRUE(capabilities.has_value());
  EXPECT_EQ(capabilities->can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kUnknown);
}

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue_NotADictionary) {
  absl::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(base::Value("not a dictionary"));

  EXPECT_FALSE(capabilities.has_value());
}

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue_DoesNotContainList) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey("accountCapabilities",
               base::Value(base::Value::Type::DICTIONARY));

  absl::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(value);

  EXPECT_FALSE(capabilities.has_value());
}

TEST_F(AccountInfoUtilTest, AccountCapabilitiesFromValue_NameNotFound) {
  base::Value value(base::Value::Type::DICTIONARY);
  base::Value* list =
      value.SetKey("accountCapabilities", base::Value(base::Value::Type::LIST));
  base::Value entry(base::Value::Type::DICTIONARY);
  entry.SetBoolKey("booleanValue", true);
  list->Append(std::move(entry));

  absl::optional<AccountCapabilities> capabilities =
      AccountCapabilitiesFromValue(value);

  EXPECT_FALSE(capabilities.has_value());
}
