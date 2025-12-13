// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards/relaunch_chrome_promo.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/webui/password_manager/promo_card.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::IsEmpty;

namespace password_manager {

class PromoCardRelaunchChromeTest : public ChromeRenderViewHostTestHarness {
 public:
  PromoCardRelaunchChromeTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kRestartToGainAccessToKeychain);
  }

  PrefService* pref_service() { return profile()->GetPrefs(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PromoCardRelaunchChromeTest, ShouldShow) {
  auto promo = std::make_unique<RelaunchChromePromo>(pref_service());

  promo->set_is_encryption_available(false);
  EXPECT_TRUE(promo->ShouldShowPromo());

  promo->set_is_encryption_available(true);
  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardRelaunchChromeTest, ShouldShowAfterDismiss) {
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());

  auto promo = std::make_unique<RelaunchChromePromo>(pref_service());
  promo->set_is_encryption_available(false);
  EXPECT_TRUE(promo->ShouldShowPromo());

  promo->OnPromoCardDismissed();
  EXPECT_TRUE(promo->ShouldShowPromo());
}

}  // namespace password_manager
