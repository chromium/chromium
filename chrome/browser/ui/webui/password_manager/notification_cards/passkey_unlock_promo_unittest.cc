// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_cards/passkey_unlock_promo.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/password_manager/notification_card.h"
#include "chrome/browser/webauthn/passkey_unlock_manager.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::Return;

void InitFeatureList(base::test::ScopedFeatureList& scoped_feature_list,
                     const std::string& enable_features = "",
                     const std::string& disable_features = "") {
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->InitFromCommandLine(enable_features, disable_features);
  feature_list->EnableRuntimeMutability(
      password_manager::features::kPasskeyUnlockPromo, base::DoNothing());
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));
}

class MockPasskeyUnlockManager : public webauthn::PasskeyUnlockManager {
 public:
  MOCK_METHOD(bool, ShouldDisplayErrorUi, (), (const, override));
};

class PasskeyUnlockPromoTest : public testing::Test {
 public:
  PasskeyUnlockPromoTest() {
    InitFeatureList(scoped_feature_list_,
                    /*enable_features=*/password_manager::features::
                        kPasskeyUnlockPromo.name);
  }

  MockPasskeyUnlockManager* passkey_unlock_manager() {
    return &mock_passkey_unlock_manager_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockPasskeyUnlockManager> mock_passkey_unlock_manager_;
};

TEST_F(PasskeyUnlockPromoTest, PromoCardProperties) {
  auto promo =
      std::make_unique<PasskeyUnlockPromo>(passkey_unlock_manager());

  EXPECT_EQ(promo->GetCardID(), password_manager::kPasskeyUnlockPromoId);
  EXPECT_EQ(promo->GetNotificationCardType(),
            password_manager::NotificationCardType::kPasskeyUnlock);
  EXPECT_EQ(promo->GetNotificationSeverity(),
            password_manager::NotificationSeverity::kPromo);
  EXPECT_TRUE(promo->IsDismissible());
  EXPECT_FALSE(promo->GetTitle().empty());
  EXPECT_FALSE(promo->GetDescription().empty());
  EXPECT_FALSE(promo->GetActionButtonText().empty());
}

TEST_F(PasskeyUnlockPromoTest, ShouldShowCardWhenErrorUiNeeded) {
  auto promo =
      std::make_unique<PasskeyUnlockPromo>(passkey_unlock_manager());

  EXPECT_CALL(*passkey_unlock_manager(), ShouldDisplayErrorUi())
      .WillRepeatedly(Return(true));

  // Should show when not dismissed, even if shown many times.
  password_manager::NotificationCardPrefState state;
  state.number_of_times_shown = 50;
  state.was_dismissed = false;
  EXPECT_TRUE(promo->ShouldShowCard(state));
}

TEST_F(PasskeyUnlockPromoTest, ShouldNotShowCardWhenDismissed) {
  auto promo =
      std::make_unique<PasskeyUnlockPromo>(passkey_unlock_manager());

  EXPECT_CALL(*passkey_unlock_manager(), ShouldDisplayErrorUi())
      .WillRepeatedly(Return(true));

  password_manager::NotificationCardPrefState state;
  state.was_dismissed = true;
  EXPECT_FALSE(promo->ShouldShowCard(state));
}

TEST_F(PasskeyUnlockPromoTest, ShouldNotShowCardWhenNoError) {
  auto promo =
      std::make_unique<PasskeyUnlockPromo>(passkey_unlock_manager());

  EXPECT_CALL(*passkey_unlock_manager(), ShouldDisplayErrorUi())
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(
      promo->ShouldShowCard(password_manager::NotificationCardPrefState{}));
}

TEST_F(PasskeyUnlockPromoTest, ShouldNotShowWhenNullManager) {
  auto promo = std::make_unique<PasskeyUnlockPromo>(nullptr);
  EXPECT_FALSE(
      promo->ShouldShowCard(password_manager::NotificationCardPrefState{}));
}

TEST_F(PasskeyUnlockPromoTest, ShouldNotShowWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  InitFeatureList(scoped_feature_list, /*enable_features=*/"",
                  /*disable_features=*/password_manager::features::
                      kPasskeyUnlockPromo.name);

  auto promo = std::make_unique<PasskeyUnlockPromo>(passkey_unlock_manager());

  EXPECT_CALL(*passkey_unlock_manager(), ShouldDisplayErrorUi())
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(
      promo->ShouldShowCard(password_manager::NotificationCardPrefState{}));
}

TEST_F(PasskeyUnlockPromoTest, ShouldShowCardMutatedAtRuntime) {
  base::test::ScopedFeatureList scoped_feature_list;
  InitFeatureList(scoped_feature_list);

  auto promo = std::make_unique<PasskeyUnlockPromo>(passkey_unlock_manager());
  EXPECT_CALL(*passkey_unlock_manager(), ShouldDisplayErrorUi())
      .WillRepeatedly(Return(true));

  // 1. Initially disabled: card is not shown.
  EXPECT_FALSE(
      promo->ShouldShowCard(password_manager::NotificationCardPrefState{}));

  // 2. Mutate disabled -> enabled at runtime: card is shown.
  scoped_feature_list.MutateRuntimeMutableFeature(
      password_manager::features::kPasskeyUnlockPromo, true);
  EXPECT_TRUE(
      promo->ShouldShowCard(password_manager::NotificationCardPrefState{}));

  // 3. Mutate enabled -> disabled at runtime: card is hidden again.
  scoped_feature_list.MutateRuntimeMutableFeature(
      password_manager::features::kPasskeyUnlockPromo, false);
  EXPECT_FALSE(
      promo->ShouldShowCard(password_manager::NotificationCardPrefState{}));
}

}  // namespace
