// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_result.h"

#include "base/json/values_util.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kOrigin1[] = "https://example1.com/";
constexpr char kOrigin2[] = "https://example2.com/";

}  // namespace

TEST(PasswordStatusCheckResultTest, ResultToFromDict) {
  auto result = std::make_unique<PasswordStatusCheckResult>();
  result->AddToCompromisedOrigins(kOrigin1);
  EXPECT_THAT(result->GetCompromisedOrigins(), testing::ElementsAre(kOrigin1));

  // When converting to dict, the values of the password data should be
  // correctly converted to base::Value.
  base::Value::Dict dict = result->ToDictValue();
  auto* compromised_origins_list =
      dict.FindList(kSafetyHubPasswordCheckOriginsKey);
  EXPECT_EQ(1U, compromised_origins_list->size());
  EXPECT_EQ(kOrigin1, compromised_origins_list->front().GetString());

  // When the Dict is restored into a PasswordStatusCheckResult, the values
  // should be correctly created.
  auto new_result = std::make_unique<PasswordStatusCheckResult>(dict);
  EXPECT_THAT(new_result->GetCompromisedOrigins(),
              testing::ElementsAre(kOrigin1));
}

TEST(PasswordStatusCheckResultTest, ResultIsTrigger) {
  auto result = std::make_unique<PasswordStatusCheckResult>();

  // When there is no leaked password, then notification should not be
  // triggered.
  EXPECT_FALSE(result->IsTriggerForMenuNotification());

  // When there is a leaked password, then notification can be triggered.
  result->AddToCompromisedOrigins(kOrigin1);
  EXPECT_TRUE(result->IsTriggerForMenuNotification());
}

TEST(PasswordStatusCheckResultTest, ResultWarrantsNewNotification) {
  auto old_result = std::make_unique<PasswordStatusCheckResult>();
  auto new_result = std::make_unique<PasswordStatusCheckResult>();
  EXPECT_FALSE(new_result->WarrantsNewMenuNotification(*old_result.get()));

  // kOrigin1 is set in new, but not in old -> warrants notification
  new_result->AddToCompromisedOrigins(kOrigin1);
  EXPECT_TRUE(new_result->WarrantsNewMenuNotification(*old_result.get()));
  // kOrigin1 is in both new and old -> no notification
  old_result->AddToCompromisedOrigins(kOrigin1);
  EXPECT_FALSE(new_result->WarrantsNewMenuNotification(*old_result.get()));
  EXPECT_EQ(
      new_result->GetNotificationString(),
      l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION, 1));

  // kOrigin2 is added in new, but not in old -> warrants notification
  new_result->AddToCompromisedOrigins(kOrigin2);
  EXPECT_TRUE(new_result->WarrantsNewMenuNotification(*old_result.get()));
  // kOrigin2 is also in both new and old -> no notification
  old_result->AddToCompromisedOrigins(kOrigin2);
  EXPECT_FALSE(new_result->WarrantsNewMenuNotification(*old_result.get()));
  EXPECT_EQ(
      new_result->GetNotificationString(),
      l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_HUB_COMPROMISED_PASSWORDS_MENU_NOTIFICATION, 2));
}

TEST(PasswordStatusCheckResultTest, CloneResult) {
  auto result = std::make_unique<PasswordStatusCheckResult>();
  result->AddToCompromisedOrigins(kOrigin1);

  auto cloned_result = result->Clone();
  EXPECT_EQ(result->GetCompromisedOrigins(), result->GetCompromisedOrigins());
}
