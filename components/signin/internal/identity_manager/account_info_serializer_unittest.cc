// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_info_serializer.h"

#include "base/values.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using AccountInfoSerializerTest = PlatformTest;

TEST_F(AccountInfoSerializerTest, ToAndFromValue) {
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
          .Build();

  base::Value::Dict dict = signin::AccountInfoSerializer::ToValue(account_info);
  std::optional<AccountInfo> deserialized_account_info =
      signin::AccountInfoSerializer::FromValue(dict);

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
}

TEST_F(AccountInfoSerializerTest, FromValue_Minimal) {
  auto dict = base::Value::Dict()
                  .Set("account_id", "test_account_id")
                  .Set("gaia", "gaia_id")
                  .Set("email", "test@example.org");
  std::optional<AccountInfo> account_info =
      signin::AccountInfoSerializer::FromValue(dict);
  ASSERT_NE(account_info, std::nullopt);
  EXPECT_EQ(account_info->gaia, GaiaId("gaia_id"));
  EXPECT_EQ(account_info->email, "test@example.org");
  EXPECT_EQ(account_info->account_id,
            CoreAccountId::FromString("test_account_id"));
}

TEST_F(AccountInfoSerializerTest, FromValue_EmptyDict) {
  base::Value::Dict dict;
  EXPECT_EQ(signin::AccountInfoSerializer::FromValue(dict), std::nullopt);
}

TEST_F(AccountInfoSerializerTest, FromValue_NoAccountId) {
  auto dict = base::Value::Dict()
                  .Set("account_id", "")
                  .Set("gaia", "gaia_id")
                  .Set("email", "test@example.org");
  EXPECT_EQ(signin::AccountInfoSerializer::FromValue(dict), std::nullopt);
}

TEST_F(AccountInfoSerializerTest, FromValue_NoGaia) {
  auto dict = base::Value::Dict()
                  .Set("account_id", "test_account_id")
                  .Set("gaia", "")
                  .Set("email", "test@example.org");
  std::optional<AccountInfo> account_info =
      signin::AccountInfoSerializer::FromValue(dict);
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

TEST_F(AccountInfoSerializerTest, FromValue_NoEmail) {
  auto dict = base::Value::Dict()
                  .Set("account_id", "test_account_id")
                  .Set("gaia", "gaia_id")
                  .Set("email", "");
  EXPECT_EQ(signin::AccountInfoSerializer::FromValue(dict), std::nullopt);
}
