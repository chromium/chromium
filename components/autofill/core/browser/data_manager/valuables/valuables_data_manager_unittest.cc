
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/browser/ui/mock_autofill_image_fetcher.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_table.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::UnorderedElementsAre;

class MockValuablesDataManagerObserver : public ValuablesDataManager::Observer {
 public:
  MockValuablesDataManagerObserver() = default;
  MockValuablesDataManagerObserver(const MockValuablesDataManagerObserver&) =
      delete;
  MockValuablesDataManagerObserver& operator=(
      const MockValuablesDataManagerObserver&) = delete;
  ~MockValuablesDataManagerObserver() override = default;

  MOCK_METHOD(void, OnValuablesDataChanged, (), (override));
};

class ValuablesDataManagerTest : public testing::Test {
 public:
  ValuablesDataManagerTest() {
    auto table = std::make_unique<ValuablesTable>();
    valuables_table_ = table.get();
    helper_ =
        std::make_unique<AutofillWebDataServiceTestHelper>(std::move(table));
    prefs_ = test::PrefServiceForTesting();
  }

  AutofillWebDataServiceTestHelper& helper() { return *helper_; }

  AutofillWebDataService& webdata_service() {
    return *helper().autofill_webdata_service();
  }

  MockAutofillImageFetcher& image_fetcher() { return mock_image_fetcher_; }

  ValuablesTable& valuables_table() { return *valuables_table_; }

  PrefService& prefs() { return *prefs_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list{
      syncer::kSyncAutofillLoyaltyCard};
  std::unique_ptr<PrefService> prefs_;
  NiceMock<MockAutofillImageFetcher> mock_image_fetcher_;
  raw_ptr<ValuablesTable> valuables_table_;
  std::unique_ptr<AutofillWebDataServiceTestHelper> helper_;
};

// Tests that the `ValuablesDataManager` correctly loads loyalty cards from the
// database in the constructor.
TEST_F(ValuablesDataManagerTest, GetLoyaltyCards) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  const LoyaltyCard card2 = test::CreateLoyaltyCard2();

  valuables_table().SetLoyaltyCards({card1, card2});

  ValuablesDataManager valuables_data_manager(&webdata_service(), &prefs(),
                                              &image_fetcher());
  EXPECT_THAT(valuables_data_manager.GetLoyaltyCards(), IsEmpty());
  EXPECT_CALL(image_fetcher(),
              FetchValuableImagesForURLs(UnorderedElementsAre(
                  card1.program_logo(), card2.program_logo())));

  helper().WaitUntilIdle();
  EXPECT_THAT(valuables_data_manager.GetLoyaltyCards(),
              UnorderedElementsAre(card1, card2));
}

// Tests that the `ValuablesDataManager` correctly generates loyalty cards to
// suggest ordered by merchant name.
TEST_F(ValuablesDataManagerTest, GetLoyaltyCardsToSuggest) {
  base::HistogramTester histogram_tester;
  const LoyaltyCard card1 = LoyaltyCard(
      /*loyalty_card_id=*/ValuableId("loyalty_card_id_1"),
      /*merchant_name=*/"CVS Pharmacy",
      /*program_name=*/"CVS Extra",
      /*program_logo=*/GURL("https://empty.url.com"),
      /*loyalty_card_number=*/"987654321987654321", {});
  const LoyaltyCard card2 = LoyaltyCard(
      /*loyalty_card_id=*/ValuableId("loyalty_card_id_3"),
      /*merchant_name=*/"Walgreens",
      /*program_name=*/"CustomerCard",
      /*program_logo=*/GURL("https://empty.url.com"),
      /*loyalty_card_number=*/"998766823", {});
  const LoyaltyCard card3 =
      LoyaltyCard(/*loyalty_card_id=*/ValuableId("loyalty_card_id_2"),
                  /*merchant_name=*/"Ticket Maester",
                  /*program_name=*/"TourLoyal",
                  /*program_logo=*/GURL("https://empty.url.com"),
                  /*loyalty_card_number=*/"37262999281", {});

  valuables_table().SetLoyaltyCards({card1, card2, card3});

  ValuablesDataManager valuables_data_manager(&webdata_service(), &prefs(),
                                              &image_fetcher());
  EXPECT_THAT(valuables_data_manager.GetLoyaltyCards(), IsEmpty());
  EXPECT_CALL(image_fetcher(), FetchValuableImagesForURLs(UnorderedElementsAre(
                                   card1.program_logo(), card2.program_logo(),
                                   card3.program_logo())));

  helper().WaitUntilIdle();
  EXPECT_THAT(valuables_data_manager.GetLoyaltyCardsToSuggest(),
              ElementsAre(card1, card3, card2));
  // Validate the basic count metrics.
  histogram_tester.ExpectTotalCount("Autofill.LoyaltyCard.StoredCardsCount", 1);
  histogram_tester.ExpectBucketCount("Autofill.LoyaltyCard.StoredCardsCount", 3,
                                     1);
}

// Tests that loyalty cards can be fetched by ID.
TEST_F(ValuablesDataManagerTest, GetLoyaltyCardById) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  const LoyaltyCard card2 = test::CreateLoyaltyCard2();
  valuables_table().SetLoyaltyCards({card1, card2});

  ValuablesDataManager valuables_data_manager(&webdata_service(), &prefs(),
                                              &image_fetcher());
  EXPECT_THAT(valuables_data_manager.GetLoyaltyCards(), IsEmpty());
  helper().WaitUntilIdle();
  EXPECT_THAT(valuables_data_manager.GetLoyaltyCardById(card1.id()),
              testing::Optional(card1));
  EXPECT_THAT(valuables_data_manager.GetLoyaltyCardById(card2.id()),
              testing::Optional(card2));
  EXPECT_THAT(
      valuables_data_manager.GetLoyaltyCardById(ValuableId("invalid_id")),
      testing::Eq(std::nullopt));
}

// Verify that the `ValuablesDataManager` correctly updates the list of loyalty
// cards when the Chrome Sync writes them to the database.
TEST_F(ValuablesDataManagerTest, DataChangedBySync) {
  base::HistogramTester histogram_tester;
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  const LoyaltyCard card2 = test::CreateLoyaltyCard2();
  {
    InSequence seq;
    // First, the `ValuablesDataManager` should fetch icons for a single card.
    EXPECT_CALL(
        image_fetcher(),
        FetchValuableImagesForURLs(UnorderedElementsAre(card1.program_logo())));
    // After that, the `ValuablesDataManager` should fetch icons for both cards.
    EXPECT_CALL(image_fetcher(),
                FetchValuableImagesForURLs(UnorderedElementsAre(
                    card1.program_logo(), card2.program_logo())));
  }
  valuables_table().SetLoyaltyCards({card1});

  ValuablesDataManager valuables_data_manager(&webdata_service(), &prefs(),
                                              &image_fetcher());
  helper().WaitUntilIdle();
  EXPECT_THAT(valuables_data_manager.GetLoyaltyCards(),
              UnorderedElementsAre(card1));

  MockValuablesDataManagerObserver observer;
  base::ScopedObservation<ValuablesDataManager,
                          MockValuablesDataManagerObserver>
      observation{&observer};
  observation.Observe(&valuables_data_manager);
  EXPECT_CALL(observer, OnValuablesDataChanged);

  // Loyalty cards are passed unsorted by sync.
  valuables_table().SetLoyaltyCards({card1, card2});
  // Make sure all async tasks are executed.
  helper().WaitUntilIdle();

  webdata_service().GetAutofillBackend(
      base::BindOnce([](AutofillWebDataBackend* backend) {
        backend->NotifyOnAutofillChangedBySync(
            syncer::DataType::AUTOFILL_VALUABLE);
      }));
  // `WaitUntilIdle()` needs to be called twice here:
  // * `NotifyOnAutofillChangedBySync()` posts a task to the UI sequence.
  // * the task to fetch the list of loyalty cards is posted to the db sequence.
  // * the task to update the cache in the `ValuablesDataManager` is then posted
  // to
  //   the UI sequence.
  helper().WaitUntilIdle();
  helper().WaitUntilIdle();
  EXPECT_THAT(valuables_data_manager.GetLoyaltyCards(),
              UnorderedElementsAre(card1, card2));

  // Validate the basic count metrics.
  histogram_tester.ExpectTotalCount("Autofill.LoyaltyCard.StoredCardsCount", 2);
  histogram_tester.ExpectBucketCount("Autofill.LoyaltyCard.StoredCardsCount", 1,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.LoyaltyCard.StoredCardsCount", 2,
                                     1);
}

TEST_F(ValuablesDataManagerTest, GetCachedValuableImageForUrl) {
  ValuablesDataManager valuables_data_manager(&webdata_service(), &prefs(),
                                              &image_fetcher());
  EXPECT_CALL(image_fetcher(), FetchValuableImagesForURLs(IsEmpty()));
  helper().WaitUntilIdle();

  const GURL expected_url = GURL("https://example.image");
  EXPECT_CALL(
      image_fetcher(),
      GetCachedImageForUrl(
          expected_url, AutofillImageFetcherBase::ImageType::kValuableImage));
  valuables_data_manager.GetCachedValuableImageForUrl(expected_url);
}

class ValuablesDataManagerPaymentMethodsOffTest
    : public ValuablesDataManagerTest {
 public:
  ValuablesDataManagerPaymentMethodsOffTest() {
    prefs().SetBoolean(prefs::kAutofillCreditCardEnabled, false);
  }
};

TEST_F(ValuablesDataManagerPaymentMethodsOffTest,
       GetLoyaltyCardsWhenPaymentMethodsOff) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  const LoyaltyCard card2 = test::CreateLoyaltyCard2();

  valuables_table().SetLoyaltyCards({card1, card2});

  ValuablesDataManager valuables_data_manager(&webdata_service(), &prefs(),
                                              &image_fetcher());
  helper().WaitUntilIdle();

  EXPECT_THAT(valuables_data_manager.GetLoyaltyCards(), IsEmpty());
  EXPECT_THAT(valuables_data_manager.GetLoyaltyCardsToSuggest(), IsEmpty());
}

TEST_F(ValuablesDataManagerPaymentMethodsOffTest,
       GetLoyaltyCardByIdWhenPaymentMethodsOff) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  valuables_table().SetLoyaltyCards({card1});

  ValuablesDataManager valuables_data_manager(&webdata_service(), &prefs(),
                                              &image_fetcher());
  helper().WaitUntilIdle();

  EXPECT_THAT(valuables_data_manager.GetLoyaltyCardById(card1.id()),
              testing::Eq(std::nullopt));
}

}  // namespace
}  // namespace autofill
