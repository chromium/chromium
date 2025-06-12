// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_card.h"

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
#include "chrome/browser/ui/webui/password_manager/promo_cards/access_on_any_device_promo.h"
#include "chrome/browser/ui/webui/password_manager/promo_cards/password_checkup_promo.h"
#include "chrome/browser/ui/webui/password_manager/promo_cards/password_manager_shortcut_promo.h"
#include "chrome/browser/ui/webui/password_manager/promo_cards/web_password_manager_promo.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Value;

namespace password_manager {

namespace {

struct PrefInfo {
  std::string id;
  int number_of_times_shown = 0;
  base::Time last_time_shown;
  bool was_dismissed = false;
};

MATCHER_P(PromoCardPrefInfo, expected, "") {
  return Value(expected.id, *arg.GetDict().FindString("id")) &&
         Value(expected.number_of_times_shown,
               *arg.GetDict().FindInt("number_of_times_shown")) &&
         Value(expected.last_time_shown,
               base::ValueToTime(arg.GetDict().Find("last_time_shown"))
                   .value()) &&
         Value(expected.was_dismissed,
               *arg.GetDict().FindBool("was_dismissed"));
}

class FakePromoCard : public PasswordPromoCardBase {
 public:
  explicit FakePromoCard(PrefService* prefs)
      : PasswordPromoCardBase(GetPromoID(), prefs) {}

  static constexpr char kId[] = "password_checkup_promo";

  // PasswordPromoCardBase implementation.
  std::string GetPromoID() const override { return kId; }

  PromoCardType GetPromoCardType() const override {
    return PromoCardType::kCheckup;
  }

  bool ShouldShowPromo() const override { return true; }

  std::u16string GetTitle() const override { return u"Fake title"; }

  std::u16string GetDescription() const override {
    return u"Useless description";
  }

  std::u16string GetActionButtonText() const override {
    return u"Do something!";
  }

  int number_of_times_shown() const { return number_of_times_shown_; }
  bool was_dismissed() const { return was_dismissed_; }
};

std::unique_ptr<web_app::WebApp> CreateWebApp() {
  GURL url(chrome::kChromeUIPasswordManagerURL);
  webapps::AppId app_id = web_app::GenerateAppId(/*manifest_id=*/"", url);
  auto web_app = std::make_unique<web_app::WebApp>(app_id);
  web_app->SetStartUrl(url);
  web_app->SetScope(url.DeprecatedGetOriginAsURL());
  web_app->SetUserDisplayMode(web_app::mojom::UserDisplayMode::kStandalone);
  return web_app;
}

}  // namespace

class PromoCardBaseTest : public ChromeRenderViewHostTestHarness {
 public:
  PromoCardBaseTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_store_ = CreateAndUseTestPasswordStore(profile());
    AffiliationServiceFactory::GetInstance()->SetTestingSubclassFactoryAndUse(
        profile(), base::BindOnce([](content::BrowserContext*) {
          return std::make_unique<affiliations::FakeAffiliationService>();
        }));
  }

  PrefService* pref_service() { return profile()->GetPrefs(); }
  TestPasswordStore* store() { return profile_store_.get(); }

 private:
  scoped_refptr<TestPasswordStore> profile_store_;
};

TEST_F(PromoCardBaseTest, InitAddsPref) {
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());

  FakePromoCard card(pref_service());
  // There should be a record now in prefs since the constructor takes care of
  // registering it when it doesn't exist.
  const base::Value::List& promo_card_prefs =
      pref_service()->GetList(prefs::kPasswordManagerPromoCardsList);
  EXPECT_THAT(promo_card_prefs,
              ElementsAre(PromoCardPrefInfo(PrefInfo{card.GetPromoID()})));
}

TEST_F(PromoCardBaseTest, PrefValuesReflectedInCard) {
  base::Time now = base::Time::Now();
  {
    base::Value::Dict promo_card_pref_entry;
    promo_card_pref_entry.Set("id", FakePromoCard::kId);
    promo_card_pref_entry.Set("number_of_times_shown", 31);
    promo_card_pref_entry.Set("last_time_shown", base::TimeToValue(now));
    promo_card_pref_entry.Set("was_dismissed", true);

    ScopedListPrefUpdate update(pref_service(),
                                prefs::kPasswordManagerPromoCardsList);
    update.Get().Append(std::move(promo_card_pref_entry));
  }

  FakePromoCard card(pref_service());
  const base::Value::List& promo_card_prefs =
      pref_service()->GetList(prefs::kPasswordManagerPromoCardsList);
  ASSERT_THAT(promo_card_prefs, ElementsAre(PromoCardPrefInfo(PrefInfo{
                                    card.GetPromoID(), 31, now, true})));

  EXPECT_EQ(31, card.number_of_times_shown());
  EXPECT_EQ(now, card.last_time_shown());
  EXPECT_TRUE(card.was_dismissed());
}

TEST_F(PromoCardBaseTest, OnPromoCardDismissed) {
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());

  FakePromoCard card(pref_service());
  EXPECT_FALSE(card.was_dismissed());

  card.OnPromoCardDismissed();
  EXPECT_TRUE(card.was_dismissed());

  const base::Value::List& promo_card_prefs =
      pref_service()->GetList(prefs::kPasswordManagerPromoCardsList);
  ASSERT_THAT(promo_card_prefs,
              ElementsAre(PromoCardPrefInfo(
                  PrefInfo{card.GetPromoID(), 0, base::Time(), true})));
}

TEST_F(PromoCardBaseTest, OnPromoCardShown) {
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());

  FakePromoCard card(pref_service());
  EXPECT_EQ(0, card.number_of_times_shown());
  EXPECT_EQ(base::Time(), card.last_time_shown());

  card.OnPromoCardShown();
  EXPECT_EQ(1, card.number_of_times_shown());
  EXPECT_EQ(base::Time::Now(), card.last_time_shown());

  const base::Value::List& promo_card_prefs =
      pref_service()->GetList(prefs::kPasswordManagerPromoCardsList);
  ASSERT_THAT(promo_card_prefs,
              ElementsAre(PromoCardPrefInfo(
                  PrefInfo{card.GetPromoID(), 1, base::Time::Now(), false})));
}

class PromoCardCheckupTest : public PromoCardBaseTest {
 public:
  void SetUp() override {
    PromoCardBaseTest::SetUp();
    delegate_ =
        extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(
            profile(), true);
  }

  void TearDown() override {
    delegate_ = nullptr;
    PromoCardBaseTest::TearDown();
  }

  extensions::PasswordsPrivateDelegate* delegate() { return delegate_.get(); }

  void SavePassword() {
    auto form = PasswordForm();
    form.signon_realm = "https://example.com";
    form.username_value = u"username";
    form.in_store = PasswordForm::Store::kProfileStore;
    store()->AddLogin(form);
    task_environment()->RunUntilIdle();
  }

 private:
  scoped_refptr<extensions::PasswordsPrivateDelegate> delegate_;
};

TEST_F(PromoCardCheckupTest, NoPromoIfNoPasswords) {
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<PasswordCheckupPromo>(pref_service(), delegate());

  EXPECT_THAT(
      pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
      testing::ElementsAre(PromoCardPrefInfo(PrefInfo{promo->GetPromoID()})));

  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardCheckupTest, NoPromoIfLeakCheckDisabledByPolicy) {
  pref_service()->SetBoolean(
      password_manager::prefs::kPasswordLeakDetectionEnabled, false);
  SavePassword();

  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<PasswordCheckupPromo>(pref_service(), delegate());

  EXPECT_THAT(
      pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
      testing::ElementsAre(PromoCardPrefInfo(PrefInfo{promo->GetPromoID()})));

  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardCheckupTest, PromoShownWithSavedPasswords) {
  SavePassword();

  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<PasswordCheckupPromo>(pref_service(), delegate());

  EXPECT_TRUE(promo->ShouldShowPromo());
}

TEST_F(PromoCardCheckupTest, PromoShownFirstThreeTimes) {
  SavePassword();

  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<PasswordCheckupPromo>(pref_service(), delegate());

  EXPECT_TRUE(promo->ShouldShowPromo());
  // Show promo 3 times.
  promo->OnPromoCardShown();
  EXPECT_TRUE(promo->ShouldShowPromo());
  promo->OnPromoCardShown();
  EXPECT_TRUE(promo->ShouldShowPromo());
  promo->OnPromoCardShown();
  EXPECT_FALSE(promo->ShouldShowPromo());

  // Check that in 7 days it's shown again.
  task_environment()->AdvanceClock(base::Days(7) + base::Seconds(1));
  EXPECT_TRUE(promo->ShouldShowPromo());
}

TEST_F(PromoCardCheckupTest, PromoShownIn7DaysAfterDismiss) {
  base::HistogramTester histogram_tester;

  SavePassword();

  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<PasswordCheckupPromo>(pref_service(), delegate());
  EXPECT_TRUE(promo->ShouldShowPromo());

  promo->OnPromoCardShown();
  promo->OnPromoCardDismissed();
  EXPECT_FALSE(promo->ShouldShowPromo());

  // Check that in 7 days it's shown again even after dismissing.
  task_environment()->AdvanceClock(base::Days(7) + base::Seconds(1));
  EXPECT_TRUE(promo->ShouldShowPromo());

  histogram_tester.ExpectUniqueSample("PasswordManager.PromoCard.Shown", 0, 1);
}

class PromoCardInWebTest : public PromoCardBaseTest {
 public:
  void SetUp() override {
    PromoCardBaseTest::SetUp();
    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(
                [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<syncer::TestSyncService>();
                })));
  }

  void TearDown() override {
    sync_service_ = nullptr;
    PromoCardBaseTest::TearDown();
  }

  syncer::TestSyncService* sync_service() { return sync_service_; }

 private:
  raw_ptr<syncer::TestSyncService> sync_service_;
};

TEST_F(PromoCardInWebTest, NoPromoIfNotSyncing) {
  sync_service()->SetSignedOut();
  ASSERT_FALSE(sync_service()->IsSyncFeatureEnabled());

  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<WebPasswordManagerPromo>(pref_service(), sync_service());

  EXPECT_THAT(
      pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
      testing::ElementsAre(PromoCardPrefInfo(PrefInfo{promo->GetPromoID()})));
  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardInWebTest, PromoIsShownWhenSyncing) {
  ASSERT_TRUE(sync_service()->IsSyncFeatureEnabled());

  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<WebPasswordManagerPromo>(pref_service(), sync_service());

  EXPECT_TRUE(promo->ShouldShowPromo());
}

TEST_F(PromoCardInWebTest, ShouldShowPromoFirstThreeTimes) {
  ASSERT_TRUE(sync_service()->IsSyncFeatureEnabled());

  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<WebPasswordManagerPromo>(pref_service(), sync_service());

  // Show promo 3 times.
  promo->OnPromoCardShown();
  EXPECT_TRUE(promo->ShouldShowPromo());
  promo->OnPromoCardShown();
  EXPECT_TRUE(promo->ShouldShowPromo());
  promo->OnPromoCardShown();
  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardInWebTest, PromoNotShownAfterDismiss) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(sync_service()->IsSyncFeatureEnabled());

  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<WebPasswordManagerPromo>(pref_service(), sync_service());
  EXPECT_TRUE(promo->ShouldShowPromo());

  promo->OnPromoCardDismissed();
  EXPECT_FALSE(promo->ShouldShowPromo());
}

class PromoCardShortcutTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    provider_ = web_app::FakeWebAppProvider::Get(profile());
    provider_->Start();
  }

  void TearDown() override { WebAppTest::TearDown(); }

  PrefService* pref_service() { return profile()->GetPrefs(); }
  web_app::FakeWebAppProvider* provider() { return provider_; }

 private:
  raw_ptr<web_app::FakeWebAppProvider, DanglingUntriaged> provider_;
};

TEST_F(PromoCardShortcutTest, NoPromoIfShortcutInstalled) {
  auto web_app = CreateWebApp();
  provider()->GetRegistrarMutable().registry().emplace(web_app->app_id(),
                                                       std::move(web_app));

  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<PasswordManagerShortcutPromo>(profile());
  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardShortcutTest, ShouldShowPromoFirstThreeTimes) {
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<PasswordManagerShortcutPromo>(profile());

  // Show promo 3 times.
  promo->OnPromoCardShown();
  EXPECT_TRUE(promo->ShouldShowPromo());
  promo->OnPromoCardShown();
  EXPECT_TRUE(promo->ShouldShowPromo());
  promo->OnPromoCardShown();
  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardShortcutTest, PromoNotShownAfterDismiss) {
  base::HistogramTester histogram_tester;
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<PasswordManagerShortcutPromo>(profile());
  EXPECT_TRUE(promo->ShouldShowPromo());

  promo->OnPromoCardDismissed();
  EXPECT_FALSE(promo->ShouldShowPromo());
}

using PromoCardAccessAnyDeviceTest = PromoCardBaseTest;

TEST_F(PromoCardAccessAnyDeviceTest, ShouldShowPromoFirstThreeTimes) {
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<AccessOnAnyDevicePromo>(pref_service());

  // Show promo 3 times.
  promo->OnPromoCardShown();
  EXPECT_TRUE(promo->ShouldShowPromo());
  promo->OnPromoCardShown();
  EXPECT_TRUE(promo->ShouldShowPromo());
  promo->OnPromoCardShown();
  EXPECT_FALSE(promo->ShouldShowPromo());
}

TEST_F(PromoCardAccessAnyDeviceTest, PromoNotShownAfterDismiss) {
  base::HistogramTester histogram_tester;
  ASSERT_THAT(pref_service()->GetList(prefs::kPasswordManagerPromoCardsList),
              IsEmpty());
  std::unique_ptr<PasswordPromoCardBase> promo =
      std::make_unique<AccessOnAnyDevicePromo>(pref_service());
  EXPECT_TRUE(promo->ShouldShowPromo());

  promo->OnPromoCardDismissed();
  EXPECT_FALSE(promo->ShouldShowPromo());
}

}  // namespace password_manager
