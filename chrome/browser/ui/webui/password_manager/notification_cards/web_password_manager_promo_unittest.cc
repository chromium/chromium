// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_cards/web_password_manager_promo.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebPasswordManagerPromoTest
    : public testing::TestWithParam<signin::ConsentLevel> {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatureState(
        syncer::kReplaceSyncPromosWithSignInPromos,
        GetParam() == signin::ConsentLevel::kSignin);
    sync_service_.SetSignedIn(GetParam());
  }

 protected:
  bool ShouldShowCard() {
    std::unique_ptr<password_manager::PasswordNotificationCardBase> card =
        std::make_unique<WebPasswordManagerPromo>(&sync_service_);
    return card->ShouldShowCard({});
  }

  syncer::TestSyncService sync_service_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(WebPasswordManagerPromoTest, ShownWithNormalEncryption) {
  EXPECT_TRUE(ShouldShowCard());
}

TEST_P(WebPasswordManagerPromoTest, NotShownWithCustomPassphrase) {
  sync_service_.GetUserSettings()->SetIsUsingExplicitPassphrase(
      /*enabled=*/true);

  EXPECT_FALSE(ShouldShowCard());
}

TEST_P(WebPasswordManagerPromoTest, NotShownWithPasswordsDisabled) {
  sync_service_.GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                                    {});

  EXPECT_FALSE(ShouldShowCard());
}

TEST_P(WebPasswordManagerPromoTest, NotShownWhenSignedOut) {
  sync_service_.SetSignedOut();

  EXPECT_FALSE(ShouldShowCard());
}

INSTANTIATE_TEST_SUITE_P(ConsentLevel,
                         WebPasswordManagerPromoTest,
                         testing::Values(signin::ConsentLevel::kSignin,
                                         signin::ConsentLevel::kSync));
