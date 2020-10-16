// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/strings/grit/components_strings.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace autofill {

namespace {
const char kTestGuid[] = "00000000-0000-0000-0000-000000000001";
const char kTestNumber[] = "4234567890123456";  // Visa
const char kTestUrl[] = "http://www.example.com/";
const char kTestUrlWithParam[] =
    "http://www.example.com/en/payments?name=checkout";

}  // namespace

class AutofillOfferManagerTest : public testing::Test {
 public:
  AutofillOfferManagerTest() = default;
  ~AutofillOfferManagerTest() override = default;

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_manager_.Init(/*profile_database=*/database_,
                                /*account_database=*/nullptr,
                                /*pref_service=*/autofill_client_.GetPrefs(),
                                /*identity_manager=*/nullptr,
                                /*client_profile_validator=*/nullptr,
                                /*history_service=*/nullptr,
                                /*is_off_the_record=*/false);
    personal_data_manager_.SetPrefService(autofill_client_.GetPrefs());
    autofill_offer_manager_ =
        std::make_unique<AutofillOfferManager>(&personal_data_manager_);
  }

  CreditCard CreateCreditCard(std::string guid,
                              std::string number = kTestNumber) {
    CreditCard card = CreditCard();
    test::SetCreditCardInfo(&card, "Jane Doe", number.c_str(),
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1");
    card.set_guid(guid);
    card.set_record_type(CreditCard::MASKED_SERVER_CARD);

    personal_data_manager_.AddServerCreditCard(card);
    return card;
  }

  void CreateCreditCardOfferForCard(const CreditCard& card,
                                    std::string offer_reward_amount,
                                    bool expired = false) {
    AutofillOfferData offer_data;
    offer_data.offer_id = 4444;
    offer_data.offer_reward_amount = offer_reward_amount;
    if (expired) {
      offer_data.expiry = AutofillClock::Now() - base::TimeDelta::FromDays(2);
    } else {
      offer_data.expiry = AutofillClock::Now() + base::TimeDelta::FromDays(2);
    }
    offer_data.merchant_domain = {GURL(kTestUrl)};
    offer_data.eligible_instrument_id = {card.instrument_id()};
    personal_data_manager_.AddCreditCardOfferData(offer_data);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestAutofillClient autofill_client_;
  scoped_refptr<AutofillWebDataService> database_;
  TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<AutofillOfferManager> autofill_offer_manager_ = nullptr;
};

TEST_F(AutofillOfferManagerTest, UpdateSuggestionsWithOffers_EligibleCashback) {
  CreditCard card = CreateCreditCard(kTestGuid);
  CreateCreditCardOfferForCard(card, "5%");

  std::vector<Suggestion> suggestions = {Suggestion()};
  suggestions[0].backend_id = kTestGuid;
  autofill_offer_manager_->UpdateSuggestionsWithOffers(GURL(kTestUrlWithParam),
                                                       suggestions);

  EXPECT_EQ(suggestions[0].offer_label,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK));
}

TEST_F(AutofillOfferManagerTest, UpdateSuggestionsWithOffers_ExpiredOffer) {
  CreditCard card = CreateCreditCard(kTestGuid);
  CreateCreditCardOfferForCard(card, "5%", /*expired=*/true);

  std::vector<Suggestion> suggestions = {Suggestion()};
  suggestions[0].backend_id = kTestGuid;
  autofill_offer_manager_->UpdateSuggestionsWithOffers(GURL(kTestUrlWithParam),
                                                       suggestions);

  EXPECT_TRUE(suggestions[0].offer_label.empty());
}

TEST_F(AutofillOfferManagerTest, UpdateSuggestionsWithOffers_WrongUrl) {
  CreditCard card = CreateCreditCard(kTestGuid);
  CreateCreditCardOfferForCard(card, "5%");

  std::vector<Suggestion> suggestions = {Suggestion()};
  suggestions[0].backend_id = kTestGuid;
  autofill_offer_manager_->UpdateSuggestionsWithOffers(
      GURL("http://wrongurl.com/"), suggestions);

  EXPECT_TRUE(suggestions[0].offer_label.empty());
}

}  // namespace autofill
