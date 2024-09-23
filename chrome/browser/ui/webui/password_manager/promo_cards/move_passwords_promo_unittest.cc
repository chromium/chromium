// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards/move_passwords_promo.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/test_sync_service.h"

using testing::IsEmpty;

namespace {
std::unique_ptr<KeyedService> TestingSyncFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}
}  // namespace

class PromoCardMovePasswordsTest : public ChromeRenderViewHostTestHarness {
 public:
  PromoCardMovePasswordsTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_store_ = CreateAndUseTestPasswordStore(profile());
    delegate_ =
        extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(
            profile(), true);

    fake_sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&TestingSyncFactoryFunction)));
  }

  void TearDown() override {
    fake_sync_service_ = nullptr;
    delegate_ = nullptr;
    profile_store_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void EnableAccountStorage() {
    fake_sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);
    ASSERT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
        pref_service(), fake_sync_service_.get()));
  }

  void SavePassword(password_manager::PasswordForm::Store store_type =
                        password_manager::PasswordForm::Store::kProfileStore) {
    password_manager::PasswordForm form;
    form.signon_realm = "https://example.com/";
    form.username_value = u"username";
    form.password_value = u"password";
    form.in_store = store_type;
    profile_store_->AddLogin(form);
    task_environment()->RunUntilIdle();
  }

  syncer::TestSyncService* sync_service() { return fake_sync_service_; }
  PrefService* pref_service() { return profile()->GetPrefs(); }
  extensions::PasswordsPrivateDelegate* delegate() { return delegate_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<syncer::TestSyncService> fake_sync_service_;
  scoped_refptr<password_manager::TestPasswordStore> profile_store_;
  scoped_refptr<extensions::PasswordsPrivateDelegate> delegate_;
};

TEST_F(PromoCardMovePasswordsTest, NoPromoIfNoPasswords) {
  EnableAccountStorage();

  ASSERT_THAT(pref_service()->GetList(
                  password_manager::prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<password_manager::PasswordPromoCardBase> promo =
      std::make_unique<MovePasswordsPromo>(profile(), delegate());

  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardMovePasswordsTest, NoPromoIfAccountStorageDisabled) {
  SavePassword();

  ASSERT_THAT(pref_service()->GetList(
                  password_manager::prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<password_manager::PasswordPromoCardBase> promo =
      std::make_unique<MovePasswordsPromo>(profile(), delegate());

  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardMovePasswordsTest, NoPromoIfNoLocalPasswords) {
  EnableAccountStorage();
  SavePassword(password_manager::PasswordForm::Store::kAccountStore);

  ASSERT_THAT(pref_service()->GetList(
                  password_manager::prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<password_manager::PasswordPromoCardBase> promo =
      std::make_unique<MovePasswordsPromo>(profile(), delegate());

  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardMovePasswordsTest, PromoShownWithSavedLocalPasswords) {
  EnableAccountStorage();
  SavePassword();

  ASSERT_THAT(pref_service()->GetList(
                  password_manager::prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<password_manager::PasswordPromoCardBase> promo =
      std::make_unique<MovePasswordsPromo>(profile(), delegate());

  EXPECT_TRUE(promo->ShouldShowPromo());
}

TEST_F(PromoCardMovePasswordsTest, PromoShownIn7DaysAfterDismiss) {
  base::HistogramTester histogram_tester;
  EnableAccountStorage();
  SavePassword();

  ASSERT_THAT(pref_service()->GetList(
                  password_manager::prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<password_manager::PasswordPromoCardBase> promo =
      std::make_unique<MovePasswordsPromo>(profile(), delegate());
  EXPECT_TRUE(promo->ShouldShowPromo());

  promo->OnPromoCardShown();
  promo->OnPromoCardDismissed();
  EXPECT_FALSE(promo->ShouldShowPromo());

  // Check that in 7 days it's shown again even after dismissing.
  task_environment()->AdvanceClock(base::Days(7) + base::Seconds(1));
  EXPECT_TRUE(promo->ShouldShowPromo());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PromoCard.Shown",
      password_manager::PromoCardType::kMovePasswords, 1);
}
