// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_card.h"

#include "base/json/values_util.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Value;

namespace password_manager {

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

class FakePromoCard : public PromoCardInterface {
 public:
  explicit FakePromoCard(PrefService* prefs)
      : PromoCardInterface(GetPromoID(), prefs) {}

  static constexpr char kId[] = "fake_promo_card";

  // PromoCardInterface implementation.
  std::string GetPromoID() const override { return kId; }

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

class PromoCardBaseTest : public testing::Test {
 public:
  void SetUp() override {
    prefs_.registry()->RegisterListPref(prefs::kPasswordManagerPromoCardsList);
  }

  TestingPrefServiceSimple* pref_service() { return &prefs_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple prefs_;
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

}  // namespace password_manager
