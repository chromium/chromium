// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards_handler.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/password_manager/promo_card.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Return;

namespace password_manager {

namespace {

const char kTestCallbackId[] = "test-callback-id";

class MockPromoCard : public PromoCardInterface {
 public:
  MockPromoCard(const std::string& id, PrefService* prefs)
      : PromoCardInterface(id, prefs), id_(id) {}

  std::string GetPromoID() const override { return id_; }

  MOCK_METHOD(std::u16string, GetTitle, (), (const, override));
  MOCK_METHOD(bool, ShouldShowPromo, (), (const, override));
  MOCK_METHOD(std::u16string, GetDescription, (), (const, override));

  int number_of_times_shown() const { return number_of_times_shown_; }
  bool was_dismissed() const { return was_dismissed_; }

 private:
  const std::string id_;
};

}  // namespace

class PromoCardsHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  PromoCardsHandlerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    prefs_.registry()->RegisterListPref(prefs::kPasswordManagerPromoCardsList);

    std::vector<std::unique_ptr<password_manager::PromoCardInterface>> cards;
    cards.emplace_back(
        std::make_unique<MockPromoCard>("password_checkup_promo", &prefs_));
    card1_ = static_cast<MockPromoCard*>(cards.back().get());
    cards.emplace_back(
        std::make_unique<MockPromoCard>("password_shortcut_promo", &prefs_));
    card2_ = static_cast<MockPromoCard*>(cards.back().get());

    auto handler =
        std::make_unique<PromoCardsHandler>(profile(), std::move(cards));
    handler_ = handler.get();
    web_ui_.AddMessageHandler(std::move(handler));
    static_cast<content::WebUIMessageHandler*>(handler_)
        ->AllowJavascriptForTesting();
    web_ui_.set_web_contents(web_contents());
  }

  void TearDown() override {
    static_cast<content::WebUIMessageHandler*>(handler_)->DisallowJavascript();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  const base::Value::Dict& GetLastSuccessfulResponse() {
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

  MockPromoCard* first_card() { return card1_; }
  MockPromoCard* second_card() { return card2_; }

 private:
  TestingPrefServiceSimple prefs_;
  content::TestWebUI web_ui_;
  raw_ptr<PromoCardsHandler> handler_;
  raw_ptr<MockPromoCard> card1_;
  raw_ptr<MockPromoCard> card2_;
};

TEST_F(PromoCardsHandlerTest, GetAvailablePromoCard) {
  ASSERT_EQ(0, first_card()->number_of_times_shown());
  ASSERT_EQ(0, second_card()->number_of_times_shown());

  base::Value::List args;
  args.Append(kTestCallbackId);

  EXPECT_CALL(*first_card(), ShouldShowPromo).WillRepeatedly(Return(false));
  EXPECT_CALL(*second_card(), ShouldShowPromo).WillRepeatedly(Return(true));

  EXPECT_CALL(*second_card(), GetTitle).WillRepeatedly(Return(u"Title"));
  EXPECT_CALL(*second_card(), GetDescription)
      .WillRepeatedly(Return(u"Description"));

  web_ui()->ProcessWebUIMessage(GURL(), "getAvailablePromoCard",
                                std::move(args));

  // Verify that promo was shown and content returned matches promo content.
  EXPECT_EQ(0, first_card()->number_of_times_shown());
  EXPECT_EQ(1, second_card()->number_of_times_shown());

  const base::Value::Dict& response = GetLastSuccessfulResponse();
  EXPECT_EQ(second_card()->GetPromoID(), *response.FindString("id"));
  EXPECT_EQ(base::UTF16ToUTF8(second_card()->GetTitle()),
            *response.FindString("title"));
  EXPECT_EQ(base::UTF16ToUTF8(second_card()->GetDescription()),
            *response.FindString("description"));
}

TEST_F(PromoCardsHandlerTest, TheOldestPromoReturned) {
  // Mark both promo cards as shown.
  first_card()->OnPromoCardShown();
  AdvanceClock(base::Days(1));
  second_card()->OnPromoCardShown();
  ASSERT_LT(first_card()->last_time_shown(), second_card()->last_time_shown());

  ASSERT_EQ(1, first_card()->number_of_times_shown());
  ASSERT_EQ(1, second_card()->number_of_times_shown());

  base::Value::List args;
  args.Append(kTestCallbackId);

  EXPECT_CALL(*first_card(), ShouldShowPromo).WillRepeatedly(Return(true));
  EXPECT_CALL(*second_card(), ShouldShowPromo).WillRepeatedly(Return(true));

  web_ui()->ProcessWebUIMessage(GURL(), "getAvailablePromoCard",
                                std::move(args));

  // Verify that promo was shown.
  EXPECT_EQ(2, first_card()->number_of_times_shown());
  EXPECT_EQ(1, second_card()->number_of_times_shown());

  const base::Value::Dict& response = GetLastSuccessfulResponse();
  EXPECT_EQ(first_card()->GetPromoID(), *response.FindString("id"));
}

TEST_F(PromoCardsHandlerTest, NoAvailablePromo) {
  ASSERT_EQ(0, first_card()->number_of_times_shown());
  ASSERT_EQ(0, second_card()->number_of_times_shown());

  base::Value::List args;
  args.Append(kTestCallbackId);

  EXPECT_CALL(*first_card(), ShouldShowPromo).WillRepeatedly(Return(false));
  EXPECT_CALL(*second_card(), ShouldShowPromo).WillRepeatedly(Return(false));

  web_ui()->ProcessWebUIMessage(GURL(), "getAvailablePromoCard",
                                std::move(args));
  VerifyLastRequestRejected();
  EXPECT_EQ(0, first_card()->number_of_times_shown());
  EXPECT_EQ(0, second_card()->number_of_times_shown());
}

TEST_F(PromoCardsHandlerTest, RecordPromoDismissed) {
  ASSERT_FALSE(first_card()->was_dismissed());
  ASSERT_FALSE(second_card()->was_dismissed());

  base::Value::List args;
  args.Append(first_card()->GetPromoID());

  web_ui()->ProcessWebUIMessage(GURL(), "recordPromoDismissed",
                                std::move(args));

  EXPECT_TRUE(first_card()->was_dismissed());
  EXPECT_FALSE(second_card()->was_dismissed());
}

}  // namespace password_manager
