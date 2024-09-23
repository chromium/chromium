// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards/screenlock_reauth_promo.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/webui/password_manager/promo_card.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

using ::testing::IsEmpty;
using ::testing::Return;

class PromoCardScreenlockReauthTest : public ChromeRenderViewHostTestHarness {
 public:
  PromoCardScreenlockReauthTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  PrefService* pref_service() { return profile()->GetPrefs(); }
};

TEST_F(PromoCardScreenlockReauthTest, NoPromoIfFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kScreenlockReauthPromoCard);
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*(authenticator.get()), CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));

  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<ScreenlockReauthPromo>(profile(),
                                              std::move(authenticator));
  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardScreenlockReauthTest, NoPromoIfScreenlockAlreadyEnabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kScreenlockReauthPromoCard);
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*(authenticator.get()), CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));
  pref_service()->SetBoolean(prefs::kBiometricAuthenticationBeforeFilling,
                             true);

  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<ScreenlockReauthPromo>(profile(),
                                              std::move(authenticator));
  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardScreenlockReauthTest,
       NoPromoIfScreenlockNotAvailiableOnDevice) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kScreenlockReauthPromoCard);
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*(authenticator.get()), CanAuthenticateWithBiometrics)
      .WillOnce(Return(false));

  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<ScreenlockReauthPromo>(profile(),
                                              std::move(authenticator));

  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardScreenlockReauthTest, NoPromoIfScreenlockExplicitlyDisabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kScreenlockReauthPromoCard);
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*(authenticator.get()), CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));
  pref_service()->SetBoolean(prefs::kBiometricAuthenticationBeforeFilling,
                             false);

  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<ScreenlockReauthPromo>(profile(),
                                              std::move(authenticator));

  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardScreenlockReauthTest, PromoShownOnlyThreeTimes) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kScreenlockReauthPromoCard);
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*(authenticator.get()), CanAuthenticateWithBiometrics)
      .WillRepeatedly(Return(true));

  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<ScreenlockReauthPromo>(profile(),
                                              std::move(authenticator));

  // Show promo 3 times.
  promo->OnPromoCardShown();
  EXPECT_TRUE(promo->ShouldShowPromo());
  promo->OnPromoCardShown();
  EXPECT_TRUE(promo->ShouldShowPromo());
  promo->OnPromoCardShown();
  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardScreenlockReauthTest, PromoNotShownAfterDismiss) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kScreenlockReauthPromoCard);
  base::HistogramTester histogram_tester;
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*(authenticator.get()), CanAuthenticateWithBiometrics)
      .WillRepeatedly(Return(true));

  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<ScreenlockReauthPromo>(profile(),
                                              std::move(authenticator));
  EXPECT_TRUE(promo->ShouldShowPromo());

  promo->OnPromoCardShown();
  promo->OnPromoCardDismissed();
  EXPECT_FALSE(promo->ShouldShowPromo());
}

}  // namespace password_manager
