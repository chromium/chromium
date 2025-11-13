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
  AccountInfo account_info;
  account_info.account_id = CoreAccountId::FromString("test_account_id");
  account_info.gaia = GaiaId("test_gaia_id");
  account_info.email = "test_email@example.com";
  account_info.full_name = "Test Full Name";
  account_info.given_name = "Test Given Name";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en-US";
  account_info.picture_url = "https://example.com/picture.jpg";
  account_info.is_child_account = signin::Tribool::kTrue;
  account_info.is_under_advanced_protection = true;
  account_info.last_downloaded_image_url_with_size =
      "https://example.com/picture_with_size.jpg";
  account_info.access_point = signin_metrics::AccessPoint::kAvatarBubbleSignIn;

  base::Value::Dict dict = signin::AccountInfoSerializer::ToValue(account_info);
  std::optional<AccountInfo> deserialized_account_info =
      signin::AccountInfoSerializer::FromValue(dict);

  ASSERT_TRUE(deserialized_account_info.has_value());
  EXPECT_EQ(account_info.account_id, deserialized_account_info->account_id);
  EXPECT_EQ(account_info.gaia, deserialized_account_info->gaia);
  EXPECT_EQ(account_info.email, deserialized_account_info->email);
  EXPECT_EQ(account_info.full_name, deserialized_account_info->full_name);
  EXPECT_EQ(account_info.given_name, deserialized_account_info->given_name);
  EXPECT_EQ(account_info.hosted_domain,
            deserialized_account_info->hosted_domain);
  EXPECT_EQ(account_info.locale, deserialized_account_info->locale);
  EXPECT_EQ(account_info.picture_url, deserialized_account_info->picture_url);
  EXPECT_EQ(account_info.is_child_account,
            deserialized_account_info->is_child_account);
  EXPECT_EQ(account_info.is_under_advanced_protection,
            deserialized_account_info->is_under_advanced_protection);
  EXPECT_EQ(account_info.last_downloaded_image_url_with_size,
            deserialized_account_info->last_downloaded_image_url_with_size);
  EXPECT_EQ(account_info.access_point, deserialized_account_info->access_point);
}

TEST_F(AccountInfoSerializerTest, FromValue_Invalid) {
  base::Value::Dict dict;
  EXPECT_FALSE(signin::AccountInfoSerializer::FromValue(dict).has_value());
}

TEST_F(AccountInfoSerializerTest, FromValue_NoGaia) {
  base::Value::Dict dict;
  dict.Set("account_id", "");
  EXPECT_FALSE(signin::AccountInfoSerializer::FromValue(dict).has_value());
}
