// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_cards_handler.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/webui/password_manager/notification_card.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::IsEmpty;
using testing::Matcher;
using ::testing::Return;
using testing::Truly;
using testing::UnorderedElementsAre;
using testing::Value;

namespace password_manager {

namespace {

const char kTestCallbackId[] = "test-callback-id";

auto HasSameNotificationCards(
    const std::vector<std::string>& notification_cards) {
  std::vector<Matcher<const base::Value&>> matchers;
  for (const auto& p : notification_cards) {
    matchers.push_back(Truly([p](const base::Value& a) {
      return p == *a.GetDict().FindString("id");
    }));
  }
  return UnorderedElementsAreArray(matchers);
}

class MockNotificationCard : public PasswordNotificationCardBase {
 public:
  MockNotificationCard(const std::string& id, PrefService* prefs)
      : PasswordNotificationCardBase(id, prefs) {}

  MOCK_METHOD(std::string, GetCardID, (), (const, override));
  MOCK_METHOD(NotificationCardType,
              GetNotificationCardType,
              (),
              (const, override));
  MOCK_METHOD(std::u16string, GetTitle, (), (const, override));
  MOCK_METHOD(bool, ShouldShowCard, (), (const, override));
  MOCK_METHOD(std::u16string, GetDescription, (), (const, override));

  int number_of_times_shown() const { return number_of_times_shown_; }
  bool was_dismissed() const { return was_dismissed_; }
};

}  // namespace

class NotificationCardsHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  NotificationCardsHandlerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_store_ = CreateAndUseTestPasswordStore(profile());

    prefs_.registry()->RegisterListPref(prefs::kPasswordManagerPromoCardsList);

    std::vector<std::unique_ptr<password_manager::PasswordNotificationCardBase>>
        cards;
    cards.emplace_back(std::make_unique<MockNotificationCard>(
        "password_checkup_promo", &prefs_));
    card1_ = static_cast<MockNotificationCard*>(cards.back().get());
    ON_CALL(*card1_, GetCardID).WillByDefault(Return("password_checkup_promo"));
    cards.emplace_back(std::make_unique<MockNotificationCard>(
        "password_shortcut_promo", &prefs_));
    card2_ = static_cast<MockNotificationCard*>(cards.back().get());
    ON_CALL(*card2_, GetCardID)
        .WillByDefault(Return("password_shortcut_promo"));

    auto handler = std::make_unique<NotificationCardsHandler>(
        base::PassKey<NotificationCardsHandlerTest>(), profile(),
        std::move(cards));
    handler_ = handler.get();
    web_ui_.AddMessageHandler(std::move(handler));
    static_cast<content::WebUIMessageHandler*>(handler_)
        ->AllowJavascriptForTesting();
    web_ui_.set_web_contents(web_contents());
  }

  void TearDown() override {
    static_cast<content::WebUIMessageHandler*>(handler_)->DisallowJavascript();
    handler_ = nullptr;
    card1_ = nullptr;
    card2_ = nullptr;
    // Explicitly clear handlers to destroy them before the Profile is destroyed
    // in ChromeRenderViewHostTestHarness::TearDown().
    web_ui_.GetHandlersForTesting()->clear();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  const base::DictValue& GetLastSuccessfulResponse() {
    auto& data = *web_ui_.call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    EXPECT_EQ(kTestCallbackId, data.arg1()->GetString());
    EXPECT_TRUE(data.arg2()->GetBool());
    return data.arg3()->GetDict();
  }

  void VerifyLastRequestRejected() {
    auto& data = *web_ui_.call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    EXPECT_EQ(kTestCallbackId, data.arg1()->GetString());
    EXPECT_TRUE(data.arg2()->GetBool());
    EXPECT_EQ(base::Value(), *data.arg3());
  }

  void AdvanceClock(base::TimeDelta time) {
    task_environment()->AdvanceClock(time);
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  TestingPrefServiceSimple* pref_service() { return &prefs_; }
  MockNotificationCard* first_card() { return card1_; }
  MockNotificationCard* second_card() { return card2_; }

 private:
  scoped_refptr<TestPasswordStore> profile_store_;
  TestingPrefServiceSimple prefs_;
  content::TestWebUI web_ui_;
  raw_ptr<NotificationCardsHandler> handler_;
  raw_ptr<MockNotificationCard> card1_;
  raw_ptr<MockNotificationCard> card2_;
};

TEST_F(NotificationCardsHandlerTest, GetAllNotificationCards) {
  pref_service()->ClearPref(prefs::kPasswordManagerPromoCardsList);
  task_environment()->RunUntilIdle();

  // Enforce delegate creation before retrieving notification cards.
  scoped_refptr<extensions::PasswordsPrivateDelegate> delegate =
      extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(
          profile(), true);

  auto notification_card_handler = NotificationCardsHandler(profile());
  task_environment()->RunUntilIdle();

  const base::ListValue& list =
      profile()->GetPrefs()->GetList(prefs::kPasswordManagerPromoCardsList);
  task_environment()->RunUntilIdle();

  std::vector<std::string> notification_cards = {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      "password_checkup_promo", "passwords_on_web_promo",
      "password_shortcut_promo", "access_on_any_device_promo"
#endif
  };

#if (BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS)) && \
    BUILDFLAG(GOOGLE_CHROME_BRANDING)
  notification_cards.emplace_back("move_passwords_promo");
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  notification_cards.emplace_back("relaunch_chrome_promo");
#endif

  task_environment()->RunUntilIdle();
  EXPECT_THAT(list, HasSameNotificationCards(notification_cards));
}

TEST_F(NotificationCardsHandlerTest, GetAvailableNotificationCard) {
  ASSERT_EQ(0, first_card()->number_of_times_shown());
  ASSERT_EQ(0, second_card()->number_of_times_shown());

  base::ListValue args;
  args.Append(kTestCallbackId);

  EXPECT_CALL(*first_card(), ShouldShowCard).WillRepeatedly(Return(false));
  EXPECT_CALL(*second_card(), ShouldShowCard).WillRepeatedly(Return(true));

  EXPECT_CALL(*second_card(), GetTitle).WillRepeatedly(Return(u"Title"));
  EXPECT_CALL(*second_card(), GetDescription)
      .WillRepeatedly(Return(u"Description"));

  web_ui()->ProcessWebUIMessage(GURL(), "getAvailableNotificationCard",
                                std::move(args));

  // Verify that notification card was shown and content returned matches card
  // content.
  EXPECT_EQ(0, first_card()->number_of_times_shown());
  EXPECT_EQ(1, second_card()->number_of_times_shown());

  const base::DictValue& response = GetLastSuccessfulResponse();
  EXPECT_EQ(second_card()->GetCardID(), *response.FindString("id"));
  EXPECT_EQ(base::UTF16ToUTF8(second_card()->GetTitle()),
            *response.FindString("title"));
  EXPECT_EQ(base::UTF16ToUTF8(second_card()->GetDescription()),
            *response.FindString("description"));
}

TEST_F(NotificationCardsHandlerTest, TheOldestCardReturned) {
  // Mark both notification cards as shown.
  first_card()->OnNotificationCardShown();
  AdvanceClock(base::Days(1));
  second_card()->OnNotificationCardShown();
  ASSERT_LT(first_card()->last_time_shown(), second_card()->last_time_shown());

  ASSERT_EQ(1, first_card()->number_of_times_shown());
  ASSERT_EQ(1, second_card()->number_of_times_shown());

  base::ListValue args;
  args.Append(kTestCallbackId);

  EXPECT_CALL(*first_card(), ShouldShowCard).WillRepeatedly(Return(true));
  EXPECT_CALL(*second_card(), ShouldShowCard).WillRepeatedly(Return(true));

  web_ui()->ProcessWebUIMessage(GURL(), "getAvailableNotificationCard",
                                std::move(args));

  // Verify that notification card was shown.
  EXPECT_EQ(2, first_card()->number_of_times_shown());
  EXPECT_EQ(1, second_card()->number_of_times_shown());

  const base::DictValue& response = GetLastSuccessfulResponse();
  EXPECT_EQ(first_card()->GetCardID(), *response.FindString("id"));
}

TEST_F(NotificationCardsHandlerTest, NoAvailableCard) {
  ASSERT_EQ(0, first_card()->number_of_times_shown());
  ASSERT_EQ(0, second_card()->number_of_times_shown());

  base::ListValue args;
  args.Append(kTestCallbackId);

  EXPECT_CALL(*first_card(), ShouldShowCard).WillRepeatedly(Return(false));
  EXPECT_CALL(*second_card(), ShouldShowCard).WillRepeatedly(Return(false));

  web_ui()->ProcessWebUIMessage(GURL(), "getAvailableNotificationCard",
                                std::move(args));
  VerifyLastRequestRejected();
  EXPECT_EQ(0, first_card()->number_of_times_shown());
  EXPECT_EQ(0, second_card()->number_of_times_shown());
}

TEST_F(NotificationCardsHandlerTest, RecordCardDismissed) {
  ASSERT_FALSE(first_card()->was_dismissed());
  ASSERT_FALSE(second_card()->was_dismissed());

  base::ListValue args;
  args.Append(first_card()->GetCardID());

  web_ui()->ProcessWebUIMessage(GURL(), "recordNotificationDismissed",
                                std::move(args));

  EXPECT_TRUE(first_card()->was_dismissed());
  EXPECT_FALSE(second_card()->was_dismissed());
}

TEST_F(NotificationCardsHandlerTest,
       RelaunchChromeBannerHasTheHighestPriority) {
  MockNotificationCard* some_card = first_card();
  MockNotificationCard* relaunch_chrome_card = second_card();

  ASSERT_EQ(0, some_card->number_of_times_shown());
  ASSERT_EQ(0, relaunch_chrome_card->number_of_times_shown());

  base::ListValue args;
  args.Append(kTestCallbackId);

  EXPECT_CALL(*some_card, ShouldShowCard).WillRepeatedly(Return(true));
  EXPECT_CALL(*relaunch_chrome_card, ShouldShowCard)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*relaunch_chrome_card, GetNotificationCardType)
      .WillRepeatedly(Return(NotificationCardType::kRelauchChrome));

  web_ui()->ProcessWebUIMessage(GURL(), "getAvailableNotificationCard",
                                std::move(args));

  // Verify that notification card was shown.
  EXPECT_EQ(0, some_card->number_of_times_shown());
  EXPECT_EQ(1, relaunch_chrome_card->number_of_times_shown());
}

}  // namespace password_manager
