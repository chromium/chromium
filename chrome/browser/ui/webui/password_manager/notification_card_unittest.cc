// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_card.h"

#include <limits>
#include <memory>

#include "base/json/values_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/password_manager/notification_cards/access_on_any_device_promo.h"
#include "chrome/browser/ui/webui/password_manager/notification_cards/password_checkup_promo.h"
#include "chrome/browser/ui/webui/password_manager/notification_cards/password_manager_shortcut_promo.h"
#include "chrome/browser/ui/webui/password_manager/notification_cards/web_password_manager_promo.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/mock_enclave_manager.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Value;

namespace password_manager {

namespace {

class FakeNotificationCard : public PasswordNotificationCardBase {
 public:
  FakeNotificationCard() = default;

  static constexpr char kId[] = "password_checkup_promo";

  // PasswordNotificationCardBase implementation.
  std::string GetCardID() const override { return kId; }

  NotificationCardType GetNotificationCardType() const override {
    return NotificationCardType::kCheckup;
  }

  bool ShouldShowCard(const NotificationCardPrefState&) const override {
    return true;
  }

  std::u16string GetTitle() const override { return u"Fake title"; }

  std::u16string GetDescription() const override {
    return u"Useless description";
  }

  std::u16string GetActionButtonText() const override {
    return u"Do something!";
  }
};

std::unique_ptr<web_app::WebApp> CreateWebApp() {
  GURL url(chrome::kChromeUIPasswordManagerURL);
  auto web_app = web_app::test::CreateWebApp(url);
  web_app->SetUserDisplayMode(web_app::mojom::UserDisplayMode::kStandalone);
  return web_app;
}

}  // namespace

class NotificationCardBaseTest : public ChromeRenderViewHostTestHarness {
 public:
  NotificationCardBaseTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_store_ = CreateAndUseTestPasswordStore(profile());
    AffiliationServiceFactory::GetInstance()->SetTestingSubclassFactoryAndUse(
        profile(), base::BindOnce([](content::BrowserContext*) {
          return std::make_unique<affiliations::FakeAffiliationService>();
        }));
    EnclaveManagerFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<MockEnclaveManager>();
            }));
  }

  PrefService* pref_service() { return profile()->GetPrefs(); }
  TestPasswordStore* store() { return profile_store_.get(); }

 private:
  scoped_refptr<TestPasswordStore> profile_store_;
};

TEST_F(NotificationCardBaseTest, DefaultOverrides) {
  FakeNotificationCard card;
  EXPECT_EQ(card.PasswordNotificationCardBase::GetNotificationSeverity(),
            NotificationSeverity::kPromo);
  EXPECT_TRUE(card.PasswordNotificationCardBase::IsDismissible());
  EXPECT_EQ(card.PasswordNotificationCardBase::GetActionButtonText(),
            std::u16string());
}

class NotificationCardCheckupTest : public NotificationCardBaseTest {
 public:
  void SetUp() override {
    NotificationCardBaseTest::SetUp();
    delegate_ =
        extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(
            profile(), true);
  }

  void TearDown() override {
    delegate_ = nullptr;
    NotificationCardBaseTest::TearDown();
  }

  extensions::PasswordsPrivateDelegate* delegate() { return delegate_.get(); }

  void SavePassword() {
    auto form = PasswordForm();
    form.signon_realm = "https://example.com";
    form.username_value = u"username";
    form.in_store = PasswordForm::Store::kProfileStore;
    store()->AddLogin(password_manager::FromPasswordForm(std::move(form)));
    task_environment()->RunUntilIdle();
  }

 private:
  scoped_refptr<extensions::PasswordsPrivateDelegate> delegate_;
};

TEST_F(NotificationCardCheckupTest, NoCardIfNoPasswords) {
  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<PasswordCheckupPromo>(pref_service(), delegate());
  EXPECT_FALSE(card->ShouldShowCard(NotificationCardPrefState{}));
}

TEST_F(NotificationCardCheckupTest, NoCardIfLeakCheckDisabledByPolicy) {
  pref_service()->SetBoolean(
      password_manager::prefs::kPasswordLeakDetectionEnabled, false);
  SavePassword();

  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<PasswordCheckupPromo>(pref_service(), delegate());
  EXPECT_FALSE(card->ShouldShowCard(NotificationCardPrefState{}));
}

TEST_F(NotificationCardCheckupTest, CardShownWithSavedPasswords) {
  SavePassword();

  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<PasswordCheckupPromo>(pref_service(), delegate());
  EXPECT_TRUE(card->ShouldShowCard(NotificationCardPrefState{}));
}

TEST_F(NotificationCardCheckupTest, CardShownFirstThreeTimes) {
  SavePassword();

  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<PasswordCheckupPromo>(pref_service(), delegate());

  EXPECT_TRUE(card->ShouldShowCard(NotificationCardPrefState{}));
  EXPECT_TRUE(card->ShouldShowCard(
      NotificationCardPrefState{.number_of_times_shown = 1}));
  EXPECT_TRUE(card->ShouldShowCard(
      NotificationCardPrefState{.number_of_times_shown = 2}));
  EXPECT_FALSE(card->ShouldShowCard(NotificationCardPrefState{
      .number_of_times_shown = 3, .last_time_shown = base::Time::Now()}));
}

TEST_F(NotificationCardCheckupTest, CardShownIn7DaysAfterDismiss) {
  SavePassword();

  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<PasswordCheckupPromo>(pref_service(), delegate());
  EXPECT_TRUE(card->ShouldShowCard(NotificationCardPrefState{}));

  NotificationCardPrefState dismissed_state{
      .was_dismissed = true,
      .number_of_times_shown = 1,
      .last_time_shown = base::Time::Now(),
  };
  EXPECT_FALSE(card->ShouldShowCard(dismissed_state));

  task_environment()->AdvanceClock(base::Days(7) + base::Seconds(1));
  EXPECT_TRUE(card->ShouldShowCard(dismissed_state));
}

class NotificationCardInWebTest
    : public NotificationCardBaseTest,
      public ::testing::WithParamInterface<signin::ConsentLevel> {
 public:
  void SetUp() override {
    if (GetParam() == signin::ConsentLevel::kSignin) {
      feature_list_.InitWithFeatures(
          {syncer::kReplaceSyncPromosWithSignInPromos}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {syncer::kReplaceSyncPromosWithSignInPromos});
    }
    NotificationCardBaseTest::SetUp();
    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(
                [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<syncer::TestSyncService>();
                })));
    sync_service_->SetSignedIn(GetParam());
  }

  void TearDown() override {
    sync_service_ = nullptr;
    NotificationCardBaseTest::TearDown();
  }

  syncer::TestSyncService* sync_service() { return sync_service_; }

 private:
  raw_ptr<syncer::TestSyncService> sync_service_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(ConsentLevel,
                         NotificationCardInWebTest,
                         ::testing::Values(signin::ConsentLevel::kSignin,
                                           signin::ConsentLevel::kSync));

TEST_P(NotificationCardInWebTest, NoCardIfNotSyncing) {
  sync_service()->SetSignedOut();

  if (GetParam() == signin::ConsentLevel::kSignin) {
    ASSERT_FALSE(sync_service()->GetActiveDataTypes().Has(syncer::PASSWORDS));
  } else {
    ASSERT_FALSE(sync_service()->IsSyncFeatureEnabled());
  }

  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<WebPasswordManagerPromo>(sync_service());

  EXPECT_FALSE(card->ShouldShowCard(NotificationCardPrefState{}));
}

TEST_P(NotificationCardInWebTest, CardIsShownWhenSyncing) {
  if (GetParam() == signin::ConsentLevel::kSignin) {
    ASSERT_TRUE(sync_service()->GetActiveDataTypes().Has(syncer::PASSWORDS));
  } else {
    ASSERT_TRUE(sync_service()->IsSyncFeatureEnabled());
  }

  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<WebPasswordManagerPromo>(sync_service());

  EXPECT_TRUE(card->ShouldShowCard(NotificationCardPrefState{}));
}

TEST_P(NotificationCardInWebTest, ShouldShowCardFirstThreeTimes) {
  if (GetParam() == signin::ConsentLevel::kSignin) {
    ASSERT_TRUE(sync_service()->GetActiveDataTypes().Has(syncer::PASSWORDS));
  } else {
    ASSERT_TRUE(sync_service()->IsSyncFeatureEnabled());
  }

  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<WebPasswordManagerPromo>(sync_service());

  EXPECT_TRUE(card->ShouldShowCard(NotificationCardPrefState{}));
  EXPECT_TRUE(card->ShouldShowCard(
      NotificationCardPrefState{.number_of_times_shown = 1}));
  EXPECT_TRUE(card->ShouldShowCard(
      NotificationCardPrefState{.number_of_times_shown = 2}));
  EXPECT_FALSE(card->ShouldShowCard(NotificationCardPrefState{
      .number_of_times_shown = 3, .last_time_shown = base::Time::Now()}));
}

TEST_P(NotificationCardInWebTest, CardNotShownAfterDismiss) {
  if (GetParam() == signin::ConsentLevel::kSignin) {
    ASSERT_TRUE(sync_service()->GetActiveDataTypes().Has(syncer::PASSWORDS));
  } else {
    ASSERT_TRUE(sync_service()->IsSyncFeatureEnabled());
  }

  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<WebPasswordManagerPromo>(sync_service());
  EXPECT_TRUE(card->ShouldShowCard(NotificationCardPrefState{}));

  NotificationCardPrefState dismissed_state{
      .was_dismissed = true,
      .number_of_times_shown = 1,
      .last_time_shown = base::Time::Now(),
  };
  EXPECT_FALSE(card->ShouldShowCard(dismissed_state));
}

class NotificationCardShortcutTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    provider()->Start();
  }

  PrefService* pref_service() { return profile()->GetPrefs(); }
  web_app::FakeWebAppProvider* provider() {
    return web_app::FakeWebAppProvider::Get(profile());
  }
};

TEST_F(NotificationCardShortcutTest, NoCardIfShortcutInstalled) {
  auto web_app = CreateWebApp();
  provider()->GetRegistrarMutable().registry().emplace(web_app->app_id(),
                                                       std::move(web_app));

  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<PasswordManagerShortcutPromo>(profile());
  EXPECT_FALSE(card->ShouldShowCard(NotificationCardPrefState{}));
}

TEST_F(NotificationCardShortcutTest, ShouldShowCardFirstThreeTimes) {
  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<PasswordManagerShortcutPromo>(profile());

  EXPECT_TRUE(card->ShouldShowCard(NotificationCardPrefState{}));
  EXPECT_TRUE(card->ShouldShowCard(
      NotificationCardPrefState{.number_of_times_shown = 1}));
  EXPECT_TRUE(card->ShouldShowCard(
      NotificationCardPrefState{.number_of_times_shown = 2}));
  EXPECT_FALSE(card->ShouldShowCard(NotificationCardPrefState{
      .number_of_times_shown = 3, .last_time_shown = base::Time::Now()}));
}

TEST_F(NotificationCardShortcutTest, CardNotShownAfterDismiss) {
  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<PasswordManagerShortcutPromo>(profile());
  EXPECT_TRUE(card->ShouldShowCard(NotificationCardPrefState{}));

  NotificationCardPrefState dismissed_state{
      .was_dismissed = true,
      .number_of_times_shown = 1,
      .last_time_shown = base::Time::Now(),
  };
  EXPECT_FALSE(card->ShouldShowCard(dismissed_state));
}

using NotificationCardAccessAnyDeviceTest = NotificationCardBaseTest;

TEST_F(NotificationCardAccessAnyDeviceTest, ShouldShowCardFirstThreeTimes) {
  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<AccessOnAnyDevicePromo>();

  EXPECT_TRUE(card->ShouldShowCard(NotificationCardPrefState{}));
  EXPECT_TRUE(card->ShouldShowCard(
      NotificationCardPrefState{.number_of_times_shown = 1}));
  EXPECT_TRUE(card->ShouldShowCard(
      NotificationCardPrefState{.number_of_times_shown = 2}));
  EXPECT_FALSE(card->ShouldShowCard(NotificationCardPrefState{
      .number_of_times_shown = 3, .last_time_shown = base::Time::Now()}));
}

TEST_F(NotificationCardAccessAnyDeviceTest, CardNotShownAfterDismiss) {
  std::unique_ptr<PasswordNotificationCardBase> card =
      std::make_unique<AccessOnAnyDevicePromo>();
  EXPECT_TRUE(card->ShouldShowCard(NotificationCardPrefState{}));

  NotificationCardPrefState dismissed_state{
      .was_dismissed = true,
      .number_of_times_shown = 1,
      .last_time_shown = base::Time::Now(),
  };
  EXPECT_FALSE(card->ShouldShowCard(dismissed_state));
}

}  // namespace password_manager
