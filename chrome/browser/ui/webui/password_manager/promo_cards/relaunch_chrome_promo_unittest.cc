// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards/relaunch_chrome_promo.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/webui/password_manager/promo_card.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_LINUX)
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker_linux.h"
#endif

using testing::IsEmpty;

namespace password_manager {

#if BUILDFLAG(IS_LINUX)
namespace {
std::unique_ptr<KeyStorageLinux> GetNullKeyStorage() {
  return nullptr;
}
void MockLockedKeychain() {
  OSCrypt::ClearCacheForTesting();
  OSCrypt::UseMockKeyStorageForTesting(base::BindOnce(&GetNullKeyStorage));
}
}  // namespace
#endif

class PromoCardRelaunchChromeTest : public ChromeRenderViewHostTestHarness {
 public:
  PromoCardRelaunchChromeTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    OSCryptMocker::SetUp();
#if BUILDFLAG(IS_LINUX)
    OSCryptMockerLinux::SetUp();
#endif
    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kRestartToGainAccessToKeychain);
  }

  ~PromoCardRelaunchChromeTest() override { OSCryptMocker::TearDown(); }

  PrefService* pref_service() { return profile()->GetPrefs(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(IS_MAC)
TEST_F(PromoCardRelaunchChromeTest, ShouldShow) {
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<RelaunchChromePromo>(pref_service());

  OSCryptMocker::SetBackendLocked(true);
  EXPECT_TRUE(promo->ShouldShowPromo());

  OSCryptMocker::SetBackendLocked(false);
  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardRelaunchChromeTest, ShouldShowAfterDismiss) {
  OSCryptMocker::SetBackendLocked(true);
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());

  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<RelaunchChromePromo>(pref_service());
  EXPECT_TRUE(promo->ShouldShowPromo());

  promo->OnPromoCardDismissed();
  EXPECT_TRUE(promo->ShouldShowPromo());
}
#elif BUILDFLAG(IS_LINUX)
TEST_F(PromoCardRelaunchChromeTest, ShouldShow) {
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<RelaunchChromePromo>(pref_service());

  MockLockedKeychain();
  EXPECT_TRUE(promo->ShouldShowPromo());

  OSCrypt::SetEncryptionPasswordForTesting("something");
  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardRelaunchChromeTest, ShouldShowAfterDismiss) {
  MockLockedKeychain();
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());

  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<RelaunchChromePromo>(pref_service());
  EXPECT_TRUE(promo->ShouldShowPromo());

  promo->OnPromoCardDismissed();
  EXPECT_TRUE(promo->ShouldShowPromo());
}
#endif
}  // namespace password_manager
