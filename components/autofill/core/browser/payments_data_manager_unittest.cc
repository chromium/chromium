// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments_data_manager.h"

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit_test_api.h"
#include "components/autofill/core/browser/data_model/ewallet.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/payments_data_manager_test_base.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace autofill {
namespace {

using testing::Pointee;

const base::Time kArbitraryTime = base::Time::FromSecondsSinceUnixEpoch(25);
const base::Time kSomeLaterTime = base::Time::FromSecondsSinceUnixEpoch(1000);

template <typename T>
bool CompareElements(T* a, T* b) {
  return a->Compare(*b) < 0;
}

template <typename T>
bool ElementsEqual(T* a, T* b) {
  return a->Compare(*b) == 0;
}

// Verifies that two vectors have the same elements (according to T::Compare)
// while ignoring order. This is useful because multiple profiles or credit
// cards that are added to the SQLite DB within the same second will be returned
// in GUID (aka random) order.
template <typename T>
void ExpectSameElements(const std::vector<T*>& expectations,
                        const std::vector<T*>& results) {
  ASSERT_EQ(expectations.size(), results.size());

  std::vector<T*> expectations_copy = expectations;
  std::sort(expectations_copy.begin(), expectations_copy.end(),
            CompareElements<T>);
  std::vector<T*> results_copy = results;
  std::sort(results_copy.begin(), results_copy.end(), CompareElements<T>);

  EXPECT_EQ(
      base::ranges::mismatch(results_copy, expectations_copy, ElementsEqual<T>)
          .first,
      results_copy.end());
}

class MockPaymentsDataManagerObserver : public PaymentsDataManager::Observer {
 public:
  MOCK_METHOD(void, OnPaymentsDataChanged, (), (override));
};

class PaymentsDataManagerHelper : public PaymentsDataManagerTestBase {
 protected:
  PaymentsDataManagerHelper() = default;

  void ResetPaymentsDataManager(bool use_sync_transport_mode = false) {
    payments_data_manager_.reset();
    MakePrimaryAccountAvailable(use_sync_transport_mode, identity_test_env_,
                                sync_service_);
    payments_data_manager_ = std::make_unique<PaymentsDataManager>(
        profile_database_service_, account_database_service_,
        /*image_fetcher=*/nullptr, /*shared_storage_handler=*/nullptr,
        prefs_.get(), &sync_service_, identity_test_env_.identity_manager(),
        GeoIpCountryCode("US"), "en-US");
    payments_data_manager_->Refresh();
    WaitForOnPaymentsDataChanged();
  }

  void WaitForOnPaymentsDataChanged() {
    testing::NiceMock<MockPaymentsDataManagerObserver> observer;
    base::RunLoop run_loop;
    ON_CALL(observer, OnPaymentsDataChanged)
        .WillByDefault(base::test::RunClosure(run_loop.QuitClosure()));
    base::ScopedObservation<PaymentsDataManager, PaymentsDataManager::Observer>
        observation{&observer};
    observation.Observe(payments_data_manager_.get());
    run_loop.Run();
  }

  PaymentsDataManager& payments_data_manager() {
    return *payments_data_manager_;
  }

  // Adds three local cards to the `payments_data_manager_`. The three cards are
  // different: two are from different companies and the third doesn't have a
  // number. All three have different owners and credit card number. This allows
  // to test the suggestions based on name as well as on credit card number.
  void SetUpReferenceLocalCreditCards() {
    ASSERT_EQ(0U, payments_data_manager().GetCreditCards().size());

    CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                            test::kEmptyOrigin);
    test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "2999", "1");
    credit_card0.set_use_count(3);
    credit_card0.set_use_date(AutofillClock::Now() - base::Days(1));
    payments_data_manager().AddCreditCard(credit_card0);

    CreditCard credit_card1("1141084B-72D7-4B73-90CF-3D6AC154673B",
                            test::kEmptyOrigin);
    credit_card1.set_use_count(300);
    credit_card1.set_use_date(AutofillClock::Now() - base::Days(10));
    test::SetCreditCardInfo(&credit_card1, "John Dillinger",
                            "4234567890123456" /* Visa */, "01", "2999", "1");
    payments_data_manager().AddCreditCard(credit_card1);

    CreditCard credit_card2("002149C1-EE28-4213-A3B9-DA243FFF021B",
                            test::kEmptyOrigin);
    credit_card2.set_use_count(1);
    credit_card2.set_use_date(AutofillClock::Now() - base::Days(1));
    test::SetCreditCardInfo(&credit_card2, "Bonnie Parker",
                            "5105105105105100" /* Mastercard */, "12", "2999",
                            "1");
    payments_data_manager().AddCreditCard(credit_card2);
    WaitForOnPaymentsDataChanged();
    ASSERT_EQ(3U, payments_data_manager().GetCreditCards().size());
  }

  // Add 2 credit cards. One local, one masked.
  void SetUpTwoCardTypes() {
    EXPECT_EQ(0U, payments_data_manager().GetCreditCards().size());
    CreditCard masked_server_card;
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley", "3456", "04",
                            "2999", "1");
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    masked_server_card.set_record_type(
        CreditCard::RecordType::kMaskedServerCard);
    masked_server_card.set_server_id("masked_id");
    masked_server_card.SetNetworkForMaskedCard(kVisaCard);
    masked_server_card.set_use_count(15);
    test_api(payments_data_manager()).AddServerCreditCard(masked_server_card);
    WaitForOnPaymentsDataChanged();
    ASSERT_EQ(1U, payments_data_manager().GetCreditCards().size());

    CreditCard local_card;
    test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                            "4234567890123463",  // Visa
                            "08", "2999", "1");
    local_card.set_guid("00000000-0000-0000-0000-000000000009");
    local_card.set_record_type(CreditCard::RecordType::kLocalCard);
    local_card.set_use_count(5);
    payments_data_manager().AddCreditCard(local_card);
    WaitForOnPaymentsDataChanged();
    ASSERT_EQ(2U, payments_data_manager().GetCreditCards().size());
  }

  PaymentsAutofillTable* GetServerDataTable() {
    return payments_data_manager()
                   .IsSyncFeatureEnabledForPaymentsServerMetrics()
               ? profile_autofill_table_.get()
               : account_autofill_table_.get();
  }

  void RemoveByGUIDFromPaymentsDataManager(const std::string& guid) {
    payments_data_manager().RemoveByGUID(guid);
    WaitForOnPaymentsDataChanged();
  }

  void SetServerCards(const std::vector<CreditCard>& server_cards) {
    test::SetServerCreditCards(GetServerDataTable(), server_cards);
  }

  void AddOfferDataForTest(AutofillOfferData offer_data) {
    test_api(payments_data_manager())
        .AddOfferData(std::make_unique<AutofillOfferData>(offer_data));
  }

  void AddLocalIban(Iban& iban) {
    // AddAsLocalIban() expects only newly extracted IBANs in Iban::kUnknown
    // state to be saved.
    iban.set_record_type(Iban::kUnknown);
    iban.set_identifier(
        Iban::Guid(payments_data_manager().AddAsLocalIban(iban)));
    WaitForOnPaymentsDataChanged();
    iban.set_record_type(Iban::kLocalIban);
  }

  // Populates payments autofill table with credit card benefits data.
  void SetCreditCardBenefits(
      const std::vector<CreditCardBenefit>& credit_card_benefits) {
    GetServerDataTable()->SetCreditCardBenefits(credit_card_benefits);
  }

 private:
  std::unique_ptr<PaymentsDataManager> payments_data_manager_;
};

class MockAutofillImageFetcher : public AutofillImageFetcherBase {
 public:
  MOCK_METHOD(
      void,
      FetchImagesForURLs,
      (base::span<const GURL> card_art_urls,
       base::span<const AutofillImageFetcherBase::ImageSize> image_sizes,
       base::OnceCallback<void(
           const std::vector<std::unique_ptr<CreditCardArtImage>>&)> callback),
      (override));
};
class PaymentsDataManagerTest : public PaymentsDataManagerHelper,
                                public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPaymentsDataManager();
  }
  void TearDown() override { TearDownTest(); }
};

class PaymentsDataManagerSyncTransportModeTest
    : public PaymentsDataManagerHelper,
      public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPaymentsDataManager(
        /*use_sync_transport_mode=*/true);
  }
  void TearDown() override { TearDownTest(); }
};

// Test that server IBANs can be added and automatically loaded/cached.
TEST_F(PaymentsDataManagerTest, AddAndReloadServerIbans) {
  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();

  GetServerDataTable()->SetServerIbansForTesting({server_iban1, server_iban2});
  std::vector<const Iban*> expected_ibans = {&server_iban1, &server_iban2};
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  ExpectSameElements(expected_ibans, payments_data_manager().GetServerIbans());

  // Reset the PaymentsDataManager. This tests that the personal data was saved
  // to the web database, and that we can load the IBANs from the web database.
  ResetPaymentsDataManager();

  // Verify that we've reloaded the IBANs from the web database.
  ExpectSameElements(expected_ibans, payments_data_manager().GetServerIbans());
}

// Test that all (local and server) IBANs can be returned.
TEST_F(PaymentsDataManagerTest, GetIbans) {
  payments_data_manager().SetSyncingForTest(true);

  Iban local_iban1;
  local_iban1.set_value(std::u16string(test::kIbanValue16));
  Iban local_iban2;
  local_iban2.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();

  AddLocalIban(local_iban1);
  AddLocalIban(local_iban2);

  GetServerDataTable()->SetServerIbansForTesting({server_iban1, server_iban2});
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  std::vector<const Iban*> all_ibans = {&local_iban1, &local_iban2,
                                        &server_iban1, &server_iban2};
  ExpectSameElements(all_ibans, payments_data_manager().GetIbans());
}

// Test that a local IBAN is removed from suggestions when it has a matching
// prefix and suffix (either equal or starting with) and the same length as a
// server IBAN.
TEST_F(PaymentsDataManagerTest,
       GetIbansToSuggestRemovesLocalIbanThatMatchesServerIban) {
  payments_data_manager().SetSyncingForTest(true);

  // `local_iban` and `server_iban` have the same prefix, suffix and length.
  Iban local_iban;
  local_iban.set_value(u"FR76 3000 6000 0112 3456 7890 189");
  local_iban.set_use_date(AutofillClock::Now() - base::Days(4));

  Iban server_iban(Iban::InstrumentId(1234567));
  server_iban.set_prefix(u"FR76");
  server_iban.set_suffix(u"0189");
  server_iban.set_use_date(AutofillClock::Now() - base::Days(2));

  AddLocalIban(local_iban);

  GetServerDataTable()->SetServerIbansForTesting({server_iban});
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  EXPECT_THAT(payments_data_manager().GetOrderedIbansToSuggest(),
              testing::ElementsAre(server_iban));
}

// Test that IBANs are ordered according to the frecency rating. All of the
// IBANs in this test case have the use count = 1.
TEST_F(PaymentsDataManagerTest, GetIbansToSuggestOrdersByFrecency) {
  payments_data_manager().SetSyncingForTest(true);

  Iban local_iban1 = test::GetLocalIban();
  local_iban1.set_use_date(AutofillClock::Now() - base::Days(4));
  AddLocalIban(local_iban1);

  Iban local_iban2 = test::GetLocalIban2();
  local_iban2.set_use_date(AutofillClock::Now() - base::Days(3));
  AddLocalIban(local_iban2);

  Iban server_iban2 = test::GetServerIban2();
  server_iban2.set_use_date(AutofillClock::Now() - base::Days(2));

  Iban server_iban3 = test::GetServerIban3();
  server_iban3.set_use_date(AutofillClock::Now() - base::Days(1));

  GetServerDataTable()->SetServerIbansForTesting({server_iban2, server_iban3});
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  EXPECT_THAT(payments_data_manager().GetOrderedIbansToSuggest(),
              testing::ElementsAre(server_iban3, server_iban2, local_iban2,
                                   local_iban1));
}

TEST_F(PaymentsDataManagerTest, AddLocalIbans) {
  Iban iban1;
  iban1.set_value(std::u16string(test::kIbanValue16));
  iban1.set_nickname(u"Nickname for Iban");

  Iban iban2;
  iban2.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  iban2.set_nickname(u"Original nickname");

  Iban iban2_with_different_nickname = iban2;
  iban2_with_different_nickname.set_nickname(u"Different nickname");

  // Attempt to add all three IBANs to the database. The first two should add
  // successfully, but the third should get skipped because its value is
  // identical to `iban2`.
  AddLocalIban(iban1);
  AddLocalIban(iban2);
  // Do not add `WaitForOnPaymentsDataChanged()` for this
  // `AddAsLocalIban` operation, as it will be terminated prematurely for
  // `iban2_with_different_nickname` due to the presence of an IBAN with the
  // same value.
  payments_data_manager().AddAsLocalIban(iban2_with_different_nickname);

  std::vector<const Iban*> ibans = {&iban1, &iban2};
  ExpectSameElements(ibans, payments_data_manager().GetLocalIbans());
}

TEST_F(PaymentsDataManagerTest, NoIbansAddedIfDisabled) {
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));

  payments_data_manager().AddAsLocalIban(iban);
  payments_data_manager().AddAsLocalIban(iban1);

  EXPECT_EQ(0U, payments_data_manager().GetLocalIbans().size());
}

TEST_F(PaymentsDataManagerTest, AddingIbanUpdatesPref) {
  // The pref should always start disabled.
  ASSERT_FALSE(payments_data_manager().IsAutofillHasSeenIbanPrefEnabled());
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));

  payments_data_manager().AddAsLocalIban(iban);
  WaitForOnPaymentsDataChanged();
  // Adding an IBAN permanently enables the pref.
  EXPECT_TRUE(payments_data_manager().IsAutofillHasSeenIbanPrefEnabled());
}

TEST_F(PaymentsDataManagerTest, UpdateLocalIbans) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  iban.set_nickname(u"Nickname for Iban");
  AddLocalIban(iban);

  // Verify the `iban` has been added successfully.
  std::vector<const Iban*> ibans = {&iban};
  ExpectSameElements(ibans, payments_data_manager().GetLocalIbans());

  // Update the `iban` with new value.
  iban.SetRawInfo(IBAN_VALUE, u"GB98 MIDL 0700 9312 3456 78");
  payments_data_manager().UpdateIban(iban);
  WaitForOnPaymentsDataChanged();

  ibans = {&iban};
  ExpectSameElements(ibans, payments_data_manager().GetLocalIbans());

  // Update the `iban` with new nickname.
  iban.set_nickname(u"Another nickname");
  payments_data_manager().UpdateIban(iban);
  WaitForOnPaymentsDataChanged();

  ibans = {&iban};
  ExpectSameElements(ibans, payments_data_manager().GetLocalIbans());
}

TEST_F(PaymentsDataManagerTest, RemoveLocalIbans) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  iban.set_nickname(u"Nickname for Iban");
  AddLocalIban(iban);

  // Verify the `iban` has been added successfully.
  std::vector<const Iban*> ibans = {&iban};
  ExpectSameElements(ibans, payments_data_manager().GetLocalIbans());

  RemoveByGUIDFromPaymentsDataManager(iban.guid());
  EXPECT_TRUE(payments_data_manager().GetLocalIbans().empty());

  // Verify that removal of a GUID that doesn't exist won't crash.
  // `RemoveByGUIDFromPaymentsDataManager()` can't be used, since it try
  // waiting for the removal to complete.
  payments_data_manager().RemoveByGUID(iban.guid());
}

TEST_F(PaymentsDataManagerTest, RecordIbanUsage_LocalIban) {
  base::HistogramTester histogram_tester;
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  Iban local_iban;
  local_iban.set_value(u"FR76 3000 6000 0112 3456 7890 189");
  EXPECT_EQ(local_iban.use_count(), 1u);
  EXPECT_EQ(local_iban.use_date(), kArbitraryTime);
  EXPECT_EQ(local_iban.modification_date(), kArbitraryTime);

  AddLocalIban(local_iban);

  // Set the current time to sometime later.
  test_clock.SetNow(kSomeLaterTime);

  // Use `local_iban`, then verify usage stats.
  EXPECT_EQ(payments_data_manager().GetLocalIbans().size(), 1u);
  payments_data_manager().RecordUseOfIban(local_iban);
  WaitForOnPaymentsDataChanged();
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredIban.Local", 1);
  EXPECT_EQ(local_iban.use_count(), 2u);
  EXPECT_EQ(local_iban.use_date(), kSomeLaterTime);
  EXPECT_EQ(local_iban.modification_date(), kArbitraryTime);
}

TEST_F(PaymentsDataManagerTest, RecordIbanUsage_ServerIban) {
  base::HistogramTester histogram_tester;
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  Iban server_iban = test::GetServerIban();
  EXPECT_EQ(server_iban.use_count(), 1u);
  EXPECT_EQ(server_iban.use_date(), kArbitraryTime);
  EXPECT_EQ(server_iban.modification_date(), kArbitraryTime);
  GetServerDataTable()->SetServerIbansForTesting({server_iban});
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Set the current time to sometime later.
  test_clock.SetNow(kSomeLaterTime);

  // Use `server_iban`, then verify usage stats.
  EXPECT_EQ(payments_data_manager().GetServerIbans().size(), 1u);
  payments_data_manager().RecordUseOfIban(server_iban);
  WaitForOnPaymentsDataChanged();
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredIban.Server", 1);
  EXPECT_EQ(server_iban.use_count(), 2u);
  EXPECT_EQ(server_iban.use_date(), kSomeLaterTime);
  EXPECT_EQ(server_iban.modification_date(), kArbitraryTime);
}

TEST_F(PaymentsDataManagerTest, AddUpdateRemoveCreditCards) {
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  credit_card0.SetNickname(u"card zero");

  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "12", "2999",
                          "1");

  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card2.SetNickname(u"card two");

  // Add two test credit cards to the database.
  payments_data_manager().AddCreditCard(credit_card0);
  payments_data_manager().AddCreditCard(credit_card1);

  WaitForOnPaymentsDataChanged();

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  ExpectSameElements(cards, payments_data_manager().GetCreditCards());

  // Update, remove, and add.
  credit_card0.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Joe");
  credit_card0.SetNickname(u"new card zero");
  payments_data_manager().UpdateCreditCard(credit_card0);
  RemoveByGUIDFromPaymentsDataManager(credit_card1.guid());
  payments_data_manager().AddCreditCard(credit_card2);

  WaitForOnPaymentsDataChanged();

  cards.clear();
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card2);
  ExpectSameElements(cards, payments_data_manager().GetCreditCards());

  // Reset the PaymentsDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPaymentsDataManager();

  // Verify that we've loaded the credit cards from the web database.
  cards.clear();
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card2);
  ExpectSameElements(cards, payments_data_manager().GetCreditCards());

  // Add a server card.
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Jane Doe", "1111", "04", "2999", "1");
  credit_card3.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  credit_card3.set_server_id("server_id");
  credit_card3.SetNetworkForMaskedCard(kVisaCard);

  test_api(payments_data_manager()).AddServerCreditCard(credit_card3);
  WaitForOnPaymentsDataChanged();

  cards.push_back(&credit_card3);
  ExpectSameElements(cards, payments_data_manager().GetCreditCards());

  // Must not add a duplicate server card with same GUID.
  MockPaymentsDataManagerObserver observer;
  EXPECT_CALL(observer, OnPaymentsDataChanged).Times(0);
  base::ScopedObservation<PaymentsDataManager, PaymentsDataManager::Observer>
      observeration{&observer};
  observeration.Observe(&payments_data_manager());
  test_api(payments_data_manager()).AddServerCreditCard(credit_card3);
  ExpectSameElements(cards, payments_data_manager().GetCreditCards());

  // Must not add a duplicate card with same contents as another server card.
  CreditCard duplicate_server_card(credit_card3);
  duplicate_server_card.set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  test_api(payments_data_manager()).AddServerCreditCard(duplicate_server_card);
  ExpectSameElements(cards, payments_data_manager().GetCreditCards());
}

// Adds two local cards and one server cards with different modification dates.
// - `local_card1`'s modification date doesn't fall in the removal range, but
//   its CVC does. Expect that the CVC is cleared.
// - `local_card2`'s and `server_card`'s modification dates fall in the removal
//   range. Expect that only the local card is removed.
TEST_F(PaymentsDataManagerTest, RemoveLocalDataModifiedBetween) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  CreditCard local_card1 = test::GetCreditCard();
  // PaymentsAutofillTable sets modification dates when adding/updating.
  payments_data_manager().AddCreditCard(local_card1);
  WaitForOnPaymentsDataChanged();

  CreditCard local_card2 = test::GetCreditCard2();
  test_clock.Advance(base::Minutes(2));
  payments_data_manager().AddCreditCard(local_card2);
  WaitForOnPaymentsDataChanged();

  payments_data_manager().UpdateLocalCvc(local_card1.guid(), u"234");
  WaitForOnPaymentsDataChanged();

  CreditCard server_card = test::GetMaskedServerCard();
  test_clock.Advance(base::Minutes(3));
  test_api(payments_data_manager()).AddServerCreditCard(server_card);
  WaitForOnPaymentsDataChanged();

  payments_data_manager().RemoveLocalDataModifiedBetween(
      kArbitraryTime + base::Minutes(1), kArbitraryTime + base::Minutes(10));
  WaitForOnPaymentsDataChanged();
  local_card1.clear_cvc();
  EXPECT_THAT(payments_data_manager().GetLocalCreditCards(),
              testing::UnorderedElementsAre(Pointee(local_card1)));
  // TODO(crbug.com/40276087): `CreditCard::operator==()` compares GUIDs even
  // for server cards, which change after every load from the database.
  std::vector<CreditCard*> server_cards =
      payments_data_manager().GetServerCreditCards();
  ASSERT_EQ(server_cards.size(), 1u);
  EXPECT_EQ(server_cards[0]->Compare(server_card), 0);
}

TEST_F(PaymentsDataManagerTest, RecordUseOfCard) {
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  CreditCard card = test::GetCreditCard();
  ASSERT_EQ(card.use_count(), 1u);
  ASSERT_EQ(card.use_date(), kArbitraryTime);
  ASSERT_EQ(card.modification_date(), kArbitraryTime);
  payments_data_manager().AddCreditCard(card);
  WaitForOnPaymentsDataChanged();

  test_clock.SetNow(kSomeLaterTime);
  payments_data_manager().RecordUseOfCard(&card);
  WaitForOnPaymentsDataChanged();

  const CreditCard* pdm_card =
      payments_data_manager().GetCreditCardByGUID(card.guid());
  ASSERT_TRUE(pdm_card);
  EXPECT_EQ(pdm_card->use_count(), 2u);
  EXPECT_EQ(pdm_card->use_date(), kSomeLaterTime);
  EXPECT_EQ(pdm_card->modification_date(), kArbitraryTime);
}

// Test that UpdateLocalCvc function working as expected.
TEST_F(PaymentsDataManagerTest, UpdateLocalCvc) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  CreditCard credit_card = test::GetCreditCard();
  const std::u16string kCvc = u"111";
  credit_card.set_cvc(kCvc);
  payments_data_manager().AddCreditCard(credit_card);
  WaitForOnPaymentsDataChanged();
  ASSERT_EQ(payments_data_manager().GetLocalCreditCards().size(), 1U);
  EXPECT_EQ(payments_data_manager().GetLocalCreditCards()[0]->cvc(), kCvc);

  const std::u16string kNewCvc = u"222";
  payments_data_manager().UpdateLocalCvc(credit_card.guid(), kNewCvc);
  WaitForOnPaymentsDataChanged();
  ASSERT_EQ(payments_data_manager().GetLocalCreditCards().size(), 1U);
  EXPECT_EQ(payments_data_manager().GetLocalCreditCards()[0]->cvc(), kNewCvc);
}

// Test that verify add, update, remove server cvc function working as expected.
TEST_F(PaymentsDataManagerTest, ServerCvc) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  const std::u16string kCvc = u"111";
  CreditCard credit_card = test::GetMaskedServerCard();
  SetServerCards({credit_card});

  // Add an empty cvc will fail a CHECK().
  EXPECT_DEATH_IF_SUPPORTED(payments_data_manager().AddServerCvc(1, u""), "");

  payments_data_manager().AddServerCvc(credit_card.instrument_id(), kCvc);
  WaitForOnPaymentsDataChanged();
  ASSERT_EQ(payments_data_manager().GetCreditCards().size(), 1U);
  EXPECT_EQ(payments_data_manager().GetCreditCards()[0]->cvc(), kCvc);

  // Update an empty cvc will fail a CHECK().
  EXPECT_DEATH_IF_SUPPORTED(
      payments_data_manager().UpdateServerCvc(credit_card.instrument_id(), u""),
      "");
  // Update an non-exist card cvc will fail a CHECK().
  EXPECT_DEATH_IF_SUPPORTED(payments_data_manager().UpdateServerCvc(99999, u""),
                            "");

  const std::u16string kNewCvc = u"222";
  payments_data_manager().UpdateServerCvc(credit_card.instrument_id(), kNewCvc);
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(payments_data_manager().GetCreditCards()[0]->cvc(), kNewCvc);

  payments_data_manager().RemoveServerCvc(credit_card.instrument_id());
  WaitForOnPaymentsDataChanged();
  ASSERT_EQ(payments_data_manager().GetCreditCards().size(), 1U);
  EXPECT_TRUE(payments_data_manager().GetCreditCards()[0]->cvc().empty());
}

// Test that verify clear server cvc function working as expected.
TEST_F(PaymentsDataManagerTest, ClearServerCvc) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  // Add a server card cvc.
  const std::u16string kCvc = u"111";
  CreditCard credit_card = test::GetMaskedServerCard();
  SetServerCards({credit_card});
  payments_data_manager().AddServerCvc(credit_card.instrument_id(), kCvc);
  WaitForOnPaymentsDataChanged();
  ASSERT_EQ(payments_data_manager().GetCreditCards().size(), 1U);
  EXPECT_EQ(payments_data_manager().GetCreditCards()[0]->cvc(), kCvc);

  // After we clear server cvcs we should expect empty cvc.
  payments_data_manager().ClearServerCvcs();
  WaitForOnPaymentsDataChanged();
  EXPECT_TRUE(payments_data_manager().GetCreditCards()[0]->cvc().empty());
}

// Test that a new credit card has its basic information set.
TEST_F(PaymentsDataManagerTest, AddCreditCard_BasicInformation) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Add a credit card to the database.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  payments_data_manager().AddCreditCard(credit_card);

  // Reload the database.
  ResetPaymentsDataManager();

  // Verify the addition.
  const std::vector<CreditCard*>& results =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, credit_card.Compare(*results[0]));

  // Make sure the use count and use date were set.
  EXPECT_EQ(1U, results[0]->use_count());
  EXPECT_EQ(kArbitraryTime, results[0]->use_date());
  EXPECT_EQ(kArbitraryTime, results[0]->modification_date());
}

// Test filling credit cards with unicode strings and crazy characters.
TEST_F(PaymentsDataManagerTest, AddCreditCard_CrazyCharacters) {
  std::vector<CreditCard> cards;
  CreditCard card1;
  card1.SetRawInfo(CREDIT_CARD_NAME_FULL,
                   u"\u751f\u6d3b\u5f88\u6709\u89c4\u5f8b "
                   u"\u4ee5\u73a9\u4e3a\u4e3b");
  card1.SetRawInfo(CREDIT_CARD_NUMBER, u"6011111111111117");
  card1.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
  card1.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2011");
  cards.push_back(card1);

  CreditCard card2;
  card2.SetRawInfo(CREDIT_CARD_NAME_FULL, u"John Williams");
  card2.SetRawInfo(CREDIT_CARD_NUMBER, u"WokoAwesome12345");
  card2.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"10");
  card2.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2015");
  cards.push_back(card2);

  CreditCard card3;
  card3.SetRawInfo(CREDIT_CARD_NAME_FULL,
                   u"\u0623\u062d\u0645\u062f\u064a "
                   u"\u0646\u062c\u0627\u062f "
                   u"\u0644\u0645\u062d\u0627\u0648\u0644\u0647 "
                   u"\u0627\u063a\u062a\u064a\u0627\u0644 "
                   u"\u0641\u064a \u0645\u062f\u064a\u0646\u0629 "
                   u"\u0647\u0645\u062f\u0627\u0646 ");
  card3.SetRawInfo(CREDIT_CARD_NUMBER,
                   u"\u092a\u0941\u0928\u0930\u094d\u091c\u0940"
                   u"\u0935\u093f\u0924 \u0939\u094b\u0917\u093e "
                   u"\u0928\u093e\u0932\u0902\u0926\u093e");
  card3.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"10");
  card3.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2015");
  cards.push_back(card3);

  CreditCard card4;
  card4.SetRawInfo(CREDIT_CARD_NAME_FULL,
                   u"\u039d\u03ad\u03b5\u03c2 "
                   u"\u03c3\u03c5\u03b3\u03c7\u03c9\u03bd\u03b5"
                   u"\u03cd\u03c3\u03b5\u03b9\u03c2 "
                   u"\u03ba\u03b1\u03b9 "
                   u"\u03ba\u03b1\u03c4\u03b1\u03c1\u03b3\u03ae"
                   u"\u03c3\u03b5\u03b9\u03c2");
  card4.SetRawInfo(CREDIT_CARD_NUMBER, u"00000000000000000000000");
  card4.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"01");
  card4.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2016");
  cards.push_back(card4);

  payments_data_manager().SetCreditCards(&cards);

  WaitForOnPaymentsDataChanged();

  ASSERT_EQ(cards.size(), payments_data_manager().GetCreditCards().size());
  for (size_t i = 0; i < cards.size(); ++i) {
    EXPECT_TRUE(
        base::Contains(cards, *payments_data_manager().GetCreditCards()[i]));
  }
}

// Test invalid credit card numbers typed in settings UI should be saved as-is.
TEST_F(PaymentsDataManagerTest, AddCreditCard_Invalid) {
  CreditCard card;
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"Not_0123-5Checked");

  std::vector<CreditCard> cards;
  cards.push_back(card);
  payments_data_manager().SetCreditCards(&cards);

  ASSERT_EQ(1u, payments_data_manager().GetCreditCards().size());
  ASSERT_EQ(card, *payments_data_manager().GetCreditCards()[0]);
}

TEST_F(PaymentsDataManagerTest, GetCreditCardByServerId) {
  CreditCard card = test::GetMaskedServerCardVisa();
  card.set_server_id("server id");
  test_api(payments_data_manager()).AddServerCreditCard(card);
  WaitForOnPaymentsDataChanged();

  ASSERT_EQ(1u, payments_data_manager().GetCreditCards().size());
  EXPECT_TRUE(payments_data_manager().GetCreditCardByServerId("server id"));
  EXPECT_FALSE(
      payments_data_manager().GetCreditCardByServerId("non-existing id"));
}

TEST_F(PaymentsDataManagerTest, UpdateUnverifiedCreditCards) {
  // Start with unverified data.
  CreditCard credit_card = test::GetCreditCard();
  EXPECT_FALSE(credit_card.IsVerified());

  // Add the data to the database.
  payments_data_manager().AddCreditCard(credit_card);
  WaitForOnPaymentsDataChanged();

  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              testing::UnorderedElementsAre(Pointee(credit_card)));

  // Try to update with just the origin changed.
  CreditCard original_credit_card(credit_card);
  credit_card.set_origin(kSettingsOrigin);
  EXPECT_TRUE(credit_card.IsVerified());
  payments_data_manager().UpdateCreditCard(credit_card);

  // Credit Card origin should not be overwritten.
  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              testing::UnorderedElementsAre(Pointee(original_credit_card)));

  // Try to update with data changed as well.
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Joe");
  payments_data_manager().UpdateCreditCard(credit_card);
  WaitForOnPaymentsDataChanged();

  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              testing::UnorderedElementsAre(Pointee(credit_card)));
}

TEST_F(PaymentsDataManagerTest, SetUniqueCreditCardLabels) {
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card0.SetRawInfo(CREDIT_CARD_NAME_FULL, u"John");
  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card1.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul");
  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card2.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Ringo");
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card3.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Other");
  CreditCard credit_card4(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card4.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Ozzy");
  CreditCard credit_card5(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card5.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Dio");

  // Add the test credit cards to the database.
  payments_data_manager().AddCreditCard(credit_card0);
  payments_data_manager().AddCreditCard(credit_card1);
  payments_data_manager().AddCreditCard(credit_card2);
  payments_data_manager().AddCreditCard(credit_card3);
  payments_data_manager().AddCreditCard(credit_card4);
  payments_data_manager().AddCreditCard(credit_card5);

  // Reset the PaymentsDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPaymentsDataManager();

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  cards.push_back(&credit_card2);
  cards.push_back(&credit_card3);
  cards.push_back(&credit_card4);
  cards.push_back(&credit_card5);
  ExpectSameElements(cards, payments_data_manager().GetCreditCards());
}

TEST_F(PaymentsDataManagerTest, SetEmptyCreditCard) {
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "", "", "", "", "");

  // Add the empty credit card to the database.
  payments_data_manager().AddCreditCard(credit_card0);

  // Note: no refresh here.

  // Reset the PaymentsDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPaymentsDataManager();

  // Verify that we've loaded the credit cards from the web database.
  ASSERT_EQ(0U, payments_data_manager().GetCreditCards().size());
}

// Tests that GetAutofillOffers returns all available offers.
TEST_F(PaymentsDataManagerTest, GetAutofillOffers) {
  // Add two card-linked offers and one promo code offer.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  AddOfferDataForTest(test::GetCardLinkedOfferData2());
  AddOfferDataForTest(test::GetPromoCodeOfferData());

  // Should return all three.
  EXPECT_EQ(3U, payments_data_manager().GetAutofillOffers().size());
}

// Tests that GetActiveAutofillPromoCodeOffersForOrigin returns only active and
// site-relevant promo code offers.
TEST_F(PaymentsDataManagerTest, GetActiveAutofillPromoCodeOffersForOrigin) {
  // Card-linked offers should not be returned.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  // Expired promo code offers should not be returned.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com"), /*is_expired=*/true));
  // Active promo code offers should be returned.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com"), /*is_expired=*/false));
  // Active promo code offers for a different site should not be returned.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.some-other-merchant.com"),
      /*is_expired=*/false));

  // Only the active offer for example.com should be returned.
  EXPECT_EQ(1U, payments_data_manager()
                    .GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());
}

// Tests that GetAutofillOffers does not return any offers if
// |IsAutofillWalletImportEnabled()| returns |false|.
TEST_F(PaymentsDataManagerTest, GetAutofillOffers_WalletImportDisabled) {
  // Add a card-linked offer and a promo code offer.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  AddOfferDataForTest(test::GetPromoCodeOfferData());

  ASSERT_EQ(2U, payments_data_manager().GetAutofillOffers().size());

  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  // Should return neither of them as the wallet import pref is disabled.
  EXPECT_EQ(0U, payments_data_manager().GetAutofillOffers().size());
}

// Tests that GetAutofillOffers does not return any offers if
// `IsAutofillPaymentMethodsEnabled()` returns `false`.
TEST_F(PaymentsDataManagerTest, GetAutofillOffers_AutofillCreditCardDisabled) {
  // Add a card-linked offer and a promo code offer.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  AddOfferDataForTest(test::GetPromoCodeOfferData());

  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Should return neither of the offers as the autofill credit card import pref
  // is disabled.
  EXPECT_EQ(0U, payments_data_manager().GetAutofillOffers().size());
}

// Tests that GetActiveAutofillPromoCodeOffersForOrigin does not return any
// promo code offers if |IsAutofillWalletImportEnabled()| returns |false|.
TEST_F(PaymentsDataManagerTest,
       GetActiveAutofillPromoCodeOffersForOrigin_WalletImportDisabled) {
  // Add an active promo code offer.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com")));

  ASSERT_EQ(1U, payments_data_manager()
                    .GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());

  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  // Should not return the offer as the wallet import pref is disabled.
  EXPECT_EQ(0U, payments_data_manager()
                    .GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());
}

// Tests that GetActiveAutofillPromoCodeOffersForOrigin does not return any
// promo code offers if `IsAutofillPaymentMethodsEnabled()` returns `false`.
TEST_F(PaymentsDataManagerTest,
       GetActiveAutofillPromoCodeOffersForOrigin_AutofillCreditCardDisabled) {
  // Add an active promo code offer.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com")));

  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Should not return the offer as the autofill credit card pref is disabled.
  EXPECT_EQ(0U, payments_data_manager()
                    .GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());
}

// Test that local credit cards are ordered as expected.
TEST_F(PaymentsDataManagerTest, GetCreditCardsToSuggest_LocalCardsRanking) {
  SetUpReferenceLocalCreditCards();

  // Sublabel is card number when filling name (exact format depends on
  // the platform, but the last 4 digits should appear).
  std::vector<CreditCard*> card_to_suggest =
      payments_data_manager().GetCreditCardsToSuggest();
  ASSERT_EQ(3U, card_to_suggest.size());

  // Ordered as expected.
  EXPECT_EQ(u"John Dillinger",
            card_to_suggest[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Clyde Barrow",
            card_to_suggest[1]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Bonnie Parker",
            card_to_suggest[2]->GetRawInfo(CREDIT_CARD_NAME_FULL));
}

// Test that local and server cards are ordered as expected.
TEST_F(PaymentsDataManagerTest,
       GetCreditCardsToSuggest_LocalAndServerCardsRanking) {
  SetUpReferenceLocalCreditCards();

  // Add some server cards.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b460");
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kMasterCard);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(5U, payments_data_manager().GetCreditCards().size());

  std::vector<CreditCard*> card_to_suggest =
      payments_data_manager().GetCreditCardsToSuggest();
  ASSERT_EQ(5U, card_to_suggest.size());

  // All cards should be ordered as expected.
  EXPECT_EQ(u"John Dillinger",
            card_to_suggest[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Jesse James",
            card_to_suggest[1]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Clyde Barrow",
            card_to_suggest[2]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Emmet Dalton",
            card_to_suggest[3]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Bonnie Parker",
            card_to_suggest[4]->GetRawInfo(CREDIT_CARD_NAME_FULL));
}

// Test that local and server cards are not shown if
// |kAutofillCreditCardEnabled| is set to |false|.
TEST_F(PaymentsDataManagerTest,
       GetCreditCardsToSuggest_CreditCardAutofillDisabled) {
  SetUpReferenceLocalCreditCards();

  // Add some server cards.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b460");
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kMasterCard);

  SetServerCards(server_cards);
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Disable Credit card autofill.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  WaitForOnPaymentsDataChanged();

  // Check that profiles were saved.
  EXPECT_EQ(5U, payments_data_manager().GetCreditCards().size());
  // Expect no autofilled values or suggestions.
  EXPECT_EQ(0U, payments_data_manager().GetCreditCardsToSuggest().size());

  std::vector<CreditCard*> card_to_suggest =
      payments_data_manager().GetCreditCardsToSuggest();
  ASSERT_EQ(0U, card_to_suggest.size());
}

// Test that local and server cards are not loaded into memory on start-up if
// |kAutofillCreditCardEnabled| is set to |false|.
TEST_F(PaymentsDataManagerTest,
       GetCreditCardsToSuggest_NoCardsLoadedIfDisabled) {
  SetUpReferenceLocalCreditCards();

  // Add some server cards.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b460");
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kMasterCard);

  SetServerCards(server_cards);

  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Expect 5 autofilled values or suggestions.
  EXPECT_EQ(5U, payments_data_manager().GetCreditCards().size());

  // Disable Credit card autofill.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  // Reload the database.
  ResetPaymentsDataManager();

  // Expect no credit card values or suggestions were loaded.
  EXPECT_EQ(0U, payments_data_manager().GetCreditCardsToSuggest().size());

  std::vector<CreditCard*> card_to_suggest =
      payments_data_manager().GetCreditCardsToSuggest();
  ASSERT_EQ(0U, card_to_suggest.size());
}

// Test that local credit cards are not added if |kAutofillCreditCardEnabled| is
// set to |false|.
TEST_F(PaymentsDataManagerTest,
       GetCreditCardsToSuggest_NoCreditCardsAddedIfDisabled) {
  // Disable Profile autofill.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Add a local credit card.
  CreditCard credit_card("002149C1-EE28-4213-A3B9-DA243FFF021B",
                         "https://www.example.com");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "04", "2999",
                          "1");
  payments_data_manager().AddCreditCard(credit_card);

  // Expect no credit card values or suggestions were added.
  EXPECT_EQ(0U, payments_data_manager().GetCreditCards().size());
}

// Tests that only the masked card is kept when deduping with a local duplicate
// of it or vice-versa. This is checked based on the value assigned during the
// for loop.
TEST_F(PaymentsDataManagerTest, DedupeCreditCardToSuggest_MaskedIsKept) {
  std::list<CreditCard*> credit_cards;

  CreditCard local_card("1141084B-72D7-4B73-90CF-3D6AC154673B",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  credit_cards.push_back(&local_card);

  // Create a masked server card that is a duplicate of a local card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "3456" /* Visa */,
                          "01", "2999", "1");
  masked_card.SetNetworkForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  PaymentsDataManager::DedupeCreditCardToSuggest(&credit_cards);
  ASSERT_EQ(1U, credit_cards.size());

  // Verify `masked_card` is returned after deduping `credit_cards` list.
  EXPECT_EQ(*credit_cards.front(), masked_card);
}

// Tests that different local and server credit cards are not deduped.
TEST_F(PaymentsDataManagerTest, DedupeCreditCardToSuggest_DifferentCards) {
  std::list<CreditCard*> credit_cards;

  CreditCard local_card("002149C1-EE28-4213-A3B9-DA243FFF021B",
                        test::kEmptyOrigin);
  local_card.set_use_count(1);
  local_card.set_use_date(AutofillClock::Now() - base::Days(1));
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "5105105105105100" /* Mastercard */, "", "", "");
  credit_cards.push_back(&local_card);

  // Create a masked server card that is different from the local card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "b456");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "0005", "12", "2999",
                          "1");
  masked_card.set_use_count(3);
  masked_card.set_use_date(AutofillClock::Now() - base::Days(15));
  credit_cards.push_back(&masked_card);

  PaymentsDataManager::DedupeCreditCardToSuggest(&credit_cards);
  EXPECT_EQ(2U, credit_cards.size());
}

// Tests case-insensitive deduping of the name field, i.e. the server card is
// kept for duplicate cards except different name casing.
TEST_F(PaymentsDataManagerTest, DedupeCreditCardToSuggest_CaseInsensitiveName) {
  std::list<CreditCard*> credit_cards;

  CreditCard local_card("1141084B-72D7-4B73-90CF-3D6AC154673B",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "homer simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  credit_cards.push_back(&local_card);

  // Create a masked server card that is a duplicate of a local card except name
  // casing.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "3456" /* Visa */,
                          "01", "2999", "1");
  masked_card.SetNetworkForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  PaymentsDataManager::DedupeCreditCardToSuggest(&credit_cards);
  ASSERT_EQ(1U, credit_cards.size());

  // Verify `masked_card` is returned after deduping `credit_cards` list.
  EXPECT_EQ(*credit_cards.front(), masked_card);
}

TEST_F(PaymentsDataManagerTest, DeleteLocalCreditCards) {
  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Alice",
                          "378282246310005" /* American Express */, "04",
                          "2020", "1");
  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "Ben",
                          "378282246310006" /* American Express */, "04",
                          "2021", "1");
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Clyde",
                          "5105105105105100" /* Mastercard */, "04", "2022",
                          "1");
  std::vector<CreditCard> cards;
  cards.push_back(credit_card1);
  cards.push_back(credit_card2);

  payments_data_manager().AddCreditCard(credit_card1);
  payments_data_manager().AddCreditCard(credit_card2);
  payments_data_manager().AddCreditCard(credit_card3);

  payments_data_manager().DeleteLocalCreditCards(cards);

  // Wait for the data to be refreshed.
  WaitForOnPaymentsDataChanged();

  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  std::unordered_set<std::u16string> expectedToRemain = {u"Clyde"};
  for (auto* card : payments_data_manager().GetCreditCards()) {
    EXPECT_NE(expectedToRemain.end(),
              expectedToRemain.find(card->GetRawInfo(CREDIT_CARD_NAME_FULL)));
  }
}

TEST_F(PaymentsDataManagerTest, DeleteAllLocalCreditCards) {
  SetUpReferenceLocalCreditCards();

  // Expect 3 local credit cards.
  EXPECT_EQ(3U, payments_data_manager().GetLocalCreditCards().size());

  payments_data_manager().DeleteAllLocalCreditCards();

  // Wait for the data to be refreshed.
  WaitForOnPaymentsDataChanged();

  // Expect the local credit cards to have been deleted.
  EXPECT_EQ(0U, payments_data_manager().GetLocalCreditCards().size());
}

TEST_F(PaymentsDataManagerTest, LogStoredCreditCardMetrics) {
  ASSERT_EQ(0U, payments_data_manager().GetCreditCards().size());

  // Helper timestamps for setting up the test data.
  base::Time now = AutofillClock::Now();
  base::Time one_month_ago = now - base::Days(30);
  base::Time::Exploded one_month_ago_exploded;
  one_month_ago.LocalExplode(&one_month_ago_exploded);

  std::vector<CreditCard> server_cards;
  server_cards.reserve(10);

  // Create in-use and in-disuse cards of each record type.
  const std::vector<CreditCard::RecordType> record_types{
      CreditCard::RecordType::kLocalCard,
      CreditCard::RecordType::kMaskedServerCard};
  for (auto record_type : record_types) {
    // Create a card that's still in active use.
    CreditCard card_in_use = test::GetRandomCreditCard(record_type);
    card_in_use.set_use_date(now - base::Days(30));
    card_in_use.set_use_count(10);

    // Create a card that's not in active use.
    CreditCard card_in_disuse = test::GetRandomCreditCard(record_type);
    card_in_disuse.SetExpirationYear(one_month_ago_exploded.year);
    card_in_disuse.SetExpirationMonth(one_month_ago_exploded.month);
    card_in_disuse.set_use_date(now - base::Days(200));
    card_in_disuse.set_use_count(10);

    // Add the cards to the personal data manager in the appropriate way.
    if (record_type == CreditCard::RecordType::kLocalCard) {
      payments_data_manager().AddCreditCard(card_in_use);
      payments_data_manager().AddCreditCard(card_in_disuse);
    } else {
      server_cards.push_back(std::move(card_in_use));
      server_cards.push_back(std::move(card_in_disuse));
    }
  }

  // Sets the virtual card enrollment state for the first server card.
  server_cards[0].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  server_cards[0].set_card_art_url(GURL("https://www.example.com/image1"));

  SetServerCards(server_cards);

  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  ASSERT_EQ(4U, payments_data_manager().GetCreditCards().size());

  // Reload the database, which will log the stored profile counts.
  base::HistogramTester histogram_tester;
  ResetPaymentsDataManager();

  ASSERT_EQ(4U, payments_data_manager().GetCreditCards().size());

  // Validate the basic count metrics for both local and server cards. Deep
  // validation of the metrics is done in:
  //    AutofillMetricsTest::LogStoredCreditCardMetrics
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Local", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Server", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount", 4, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Local", 2,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Server", 2,
                                     1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.WithVirtualCardMetadata", 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.WithCardArtImage", 1, 1);
}

// Test that setting a null sync service returns only local credit cards.
TEST_F(PaymentsDataManagerTest, GetCreditCards_NoSyncService) {
  SetUpTwoCardTypes();

  // Set no sync service.
  payments_data_manager().SetSyncServiceForTest(nullptr);
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // No sync service is the same as payments integration being disabled, i.e.
  // IsAutofillWalletImportEnabled() returning false. Only local credit
  // cards are shown.
  EXPECT_EQ(0U, payments_data_manager().GetServerCreditCards().size());
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());
}

// Sanity check that the mode where we use the regular, persistent storage for
// cards still works.
TEST_F(PaymentsDataManagerTest, UsePersistentServerStorage) {
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  SetUpTwoCardTypes();

  EXPECT_EQ(2U, payments_data_manager().GetCreditCards().size());
  EXPECT_EQ(2U, payments_data_manager().GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, payments_data_manager().GetLocalCreditCards().size());
  EXPECT_EQ(1U, payments_data_manager().GetServerCreditCards().size());
}

// Verify that PDM can switch at runtime between the different storages.
TEST_F(PaymentsDataManagerSyncTransportModeTest, SwitchServerStorages) {
  // Start with account storage.
  SetUpTwoCardTypes();

  // Check that we do have a server card, as expected.
  ASSERT_EQ(1U, payments_data_manager().GetServerCreditCards().size());

  // Switch to persistent storage.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync);
  payments_data_manager().OnStateChanged(&sync_service_);
  WaitForOnPaymentsDataChanged();

  EXPECT_EQ(0U, payments_data_manager().GetServerCreditCards().size());

  // Add a new card to the persistent storage.
  CreditCard server_card;
  test::SetCreditCardInfo(&server_card, "Server Card", "3456", "04", "2999",
                          "1");
  server_card.set_guid("00000000-0000-0000-0000-000000000007");
  server_card.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  server_card.set_server_id("server_id");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  test_api(payments_data_manager()).AddServerCreditCard(server_card);
  WaitForOnPaymentsDataChanged();

  EXPECT_EQ(1U, payments_data_manager().GetServerCreditCards().size());

  // Switch back to the account storage, and verify that we are back to the
  // original card.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  payments_data_manager().OnStateChanged(&sync_service_);
  WaitForOnPaymentsDataChanged();

  ASSERT_EQ(1U, payments_data_manager().GetServerCreditCards().size());
  EXPECT_EQ(u"3456",
            payments_data_manager().GetServerCreditCards()[0]->number());
}

// Sanity check that the mode where we use the regular, persistent storage for
// cards still works.
TEST_F(PaymentsDataManagerSyncTransportModeTest,
       UseCorrectStorageForDifferentCards) {
  // Add a server card.
  CreditCard server_card;
  test::SetCreditCardInfo(&server_card, "Server Card", "3456", "04", "2999",
                          "1");
  server_card.set_guid("00000000-0000-0000-0000-000000000007");
  server_card.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  server_card.set_server_id("server_id");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  test_api(payments_data_manager()).AddServerCreditCard(server_card);

  // Set server card metadata.
  server_card.set_use_count(15);
  payments_data_manager().UpdateServerCardsMetadata({server_card});

  WaitForOnPaymentsDataChanged();

  // Expect that the server card is stored in the account autofill table.
  std::vector<std::unique_ptr<CreditCard>> cards;
  account_autofill_table_->GetServerCreditCards(cards);
  EXPECT_EQ(1U, cards.size());
  EXPECT_EQ(server_card.LastFourDigits(), cards[0]->LastFourDigits());

  // Add a local card.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                          "4234567890123463",  // Visa
                          "08", "2999", "1");
  local_card.set_guid("00000000-0000-0000-0000-000000000009");
  local_card.set_record_type(CreditCard::RecordType::kLocalCard);
  local_card.set_use_date(AutofillClock::Now() - base::Days(5));
  payments_data_manager().AddCreditCard(local_card);

  WaitForOnPaymentsDataChanged();

  // Expect that the local card is stored in the profile autofill table.
  profile_autofill_table_->GetCreditCards(&cards);
  EXPECT_EQ(1U, cards.size());
  EXPECT_EQ(local_card.LastFourDigits(), cards[0]->LastFourDigits());
}

// Sync Transport mode is only for Win, Mac, and Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(PaymentsDataManagerSyncTransportModeTest,
       ServerCardsShowInTransportMode) {
  SetUpTwoCardTypes();

  CoreAccountInfo active_info =
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  // Opt-in to seeing server card in sync transport mode.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), active_info.account_id, true);

  // Check that the server card is available for suggestion.
  EXPECT_EQ(2U, payments_data_manager().GetCreditCards().size());
  EXPECT_EQ(2U, payments_data_manager().GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, payments_data_manager().GetLocalCreditCards().size());
  EXPECT_EQ(1U, payments_data_manager().GetServerCreditCards().size());

  // Stop Wallet sync.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());

  // Check that server cards are unavailable.
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());
  EXPECT_EQ(1U, payments_data_manager().GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, payments_data_manager().GetLocalCreditCards().size());
  EXPECT_EQ(0U, payments_data_manager().GetServerCreditCards().size());
}

// Make sure that the opt in is necessary to show server cards if the
// appropriate feature is disabled.
TEST_F(PaymentsDataManagerSyncTransportModeTest,
       ServerCardsShowInTransportMode_NeedOptIn) {
  SetUpTwoCardTypes();

  CoreAccountInfo active_info =
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  // The server card should not be available at first. The user needs to
  // accept the opt-in offer.
  EXPECT_EQ(2U, payments_data_manager().GetCreditCards().size());
  EXPECT_EQ(1U, payments_data_manager().GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, payments_data_manager().GetLocalCreditCards().size());
  EXPECT_EQ(1U, payments_data_manager().GetServerCreditCards().size());

  // Opt-in to seeing server card in sync transport mode.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), active_info.account_id, true);

  // Check that the server card is available for suggestion.
  EXPECT_EQ(2U, payments_data_manager().GetCreditCards().size());
  EXPECT_EQ(2U, payments_data_manager().GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, payments_data_manager().GetLocalCreditCards().size());
  EXPECT_EQ(1U, payments_data_manager().GetServerCreditCards().size());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// Test that ensure local data is not lost on sign-in.
// Clearing/changing the primary account is not supported on CrOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PaymentsDataManagerTest, KeepExistingLocalDataOnSignIn) {
  // Sign out.
  identity_test_env_.ClearPrimaryAccount();
  sync_service_.SetSignedOut();
  EXPECT_TRUE(sync_service_.GetAccountInfo().IsEmpty());
  EXPECT_EQ(0U, payments_data_manager().GetCreditCards().size());

  // Add local card.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                          "4234567890123463",  // Visa
                          "08", "2999", "1");
  local_card.set_guid("00000000-0000-0000-0000-000000000009");
  local_card.set_record_type(CreditCard::RecordType::kLocalCard);
  local_card.set_use_count(5);
  payments_data_manager().AddCreditCard(local_card);
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // Sign in.
  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSync);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account);
  EXPECT_TRUE(
      sync_service_.IsSyncFeatureEnabled() &&
      sync_service_.GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA));
  payments_data_manager().OnStateChanged(&sync_service_);
  ASSERT_TRUE(
      payments_data_manager().IsSyncFeatureEnabledForPaymentsServerMetrics());

  // Check saved local card should be not lost.
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());
  EXPECT_EQ(0,
            local_card.Compare(*payments_data_manager().GetCreditCards()[0]));
}
#endif

// Tests that all the non settings origins of autofill credit cards are cleared
// even if sync is disabled.
TEST_F(
    PaymentsDataManagerTest,
    SyncServiceInitializedWithAutofillDisabled_ClearCreditCardNonSettingsOrigins) {
  // Create a card with a non-settings, non-empty origin.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "https://www.example.com");
  test::SetCreditCardInfo(&credit_card, "Bob0",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  payments_data_manager().AddCreditCard(credit_card);
  WaitForOnPaymentsDataChanged();

  // Turn off payments sync.
  syncer::UserSelectableTypeSet user_selectable_type_set =
      sync_service_.GetUserSettings()->GetSelectedTypes();
  user_selectable_type_set.Remove(syncer::UserSelectableType::kPayments);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/user_selectable_type_set);

  // The credit card should still exist.
  ASSERT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // Reload the personal data manager.
  ResetPaymentsDataManager();

  // The credit card should still exist.
  ASSERT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // The card's origin should be cleared
  EXPECT_TRUE(payments_data_manager().GetCreditCards()[0]->origin().empty());
}

TEST_F(PaymentsDataManagerTest, ClearAllCvcs) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  // Add a server card and its CVC.
  CreditCard server_card = test::GetMaskedServerCard();
  const std::u16string server_cvc = u"111";
  SetServerCards({server_card});
  payments_data_manager().AddServerCvc(server_card.instrument_id(), server_cvc);
  WaitForOnPaymentsDataChanged();

  // Add a local card and its CVC.
  CreditCard local_card = test::GetCreditCard();
  const std::u16string local_cvc = u"999";
  local_card.set_cvc(local_cvc);
  payments_data_manager().AddCreditCard(local_card);
  WaitForOnPaymentsDataChanged();

  ASSERT_EQ(payments_data_manager().GetLocalCreditCards().size(), 1U);
  ASSERT_EQ(payments_data_manager().GetServerCreditCards().size(), 1U);
  EXPECT_EQ(payments_data_manager().GetServerCreditCards()[0]->cvc(),
            server_cvc);
  EXPECT_EQ(payments_data_manager().GetLocalCreditCards()[0]->cvc(), local_cvc);

  // Clear out all the CVCs (local + server).
  payments_data_manager().ClearLocalCvcs();
  payments_data_manager().ClearServerCvcs();
  WaitForOnPaymentsDataChanged();
  EXPECT_TRUE(payments_data_manager().GetServerCreditCards()[0]->cvc().empty());
  EXPECT_TRUE(payments_data_manager().GetLocalCreditCards()[0]->cvc().empty());
}

// Tests that benefit getters return expected result for active benefits.
TEST_F(PaymentsDataManagerTest, GetActiveCreditCardBenefits) {
  // Add active benefits.
  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_flat_rate_benefit =
          flat_rate_benefit.linked_card_instrument_id();
  payments_data_manager().AddCreditCardBenefitForTest(
      std::move(flat_rate_benefit));

  CreditCardCategoryBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_category_benefit =
          category_benefit.linked_card_instrument_id();
  const CreditCardCategoryBenefit::BenefitCategory
      benefit_category_for_category_benefit =
          category_benefit.benefit_category();
  payments_data_manager().AddCreditCardBenefitForTest(
      std::move(category_benefit));

  CreditCardMerchantBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_merchant_benefit =
          merchant_benefit.linked_card_instrument_id();
  url::Origin merchant_origin_for_merchant_benefit =
      *merchant_benefit.merchant_domains().begin();
  payments_data_manager().AddCreditCardBenefitForTest(
      std::move(merchant_benefit));

  // Match getter results with the search criteria.
  EXPECT_TRUE(payments_data_manager().IsAutofillPaymentMethodsEnabled());
  EXPECT_EQ(
      payments_data_manager()
          .GetFlatRateBenefitByInstrumentId(instrument_id_for_flat_rate_benefit)
          ->linked_card_instrument_id(),
      instrument_id_for_flat_rate_benefit);

  std::optional<CreditCardCategoryBenefit> category_benefit_result =
      payments_data_manager().GetCategoryBenefitByInstrumentIdAndCategory(
          instrument_id_for_category_benefit,
          benefit_category_for_category_benefit);
  EXPECT_EQ(category_benefit_result->linked_card_instrument_id(),
            instrument_id_for_category_benefit);
  EXPECT_EQ(category_benefit_result->benefit_category(),
            benefit_category_for_category_benefit);

  std::optional<CreditCardMerchantBenefit> merchant_benefit_result =
      payments_data_manager().GetMerchantBenefitByInstrumentIdAndOrigin(
          instrument_id_for_merchant_benefit,
          merchant_origin_for_merchant_benefit);
  EXPECT_EQ(merchant_benefit_result->linked_card_instrument_id(),
            instrument_id_for_merchant_benefit);
  EXPECT_TRUE(merchant_benefit_result->merchant_domains().contains(
      merchant_origin_for_merchant_benefit));

  // Disable autofill credit card pref. Check that no benefits are returned.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  EXPECT_FALSE(payments_data_manager().GetFlatRateBenefitByInstrumentId(
      instrument_id_for_flat_rate_benefit));
  EXPECT_FALSE(
      payments_data_manager().GetCategoryBenefitByInstrumentIdAndCategory(
          instrument_id_for_category_benefit,
          benefit_category_for_category_benefit));
  EXPECT_FALSE(
      payments_data_manager().GetMerchantBenefitByInstrumentIdAndOrigin(
          instrument_id_for_merchant_benefit,
          merchant_origin_for_merchant_benefit));
}

// Tests benefit getters will not return inactive benefits.
TEST_F(PaymentsDataManagerTest, GetInactiveCreditCardBenefits) {
  // Add inactive benefits.
  base::Time future_time = AutofillClock::Now() + base::Days(5);

  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  test_api(flat_rate_benefit).SetStartTime(future_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_flat_rate_benefit =
          flat_rate_benefit.linked_card_instrument_id();
  payments_data_manager().AddCreditCardBenefitForTest(
      std::move(flat_rate_benefit));

  CreditCardCategoryBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  test_api(category_benefit).SetStartTime(future_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_category_benefit =
          category_benefit.linked_card_instrument_id();
  const CreditCardCategoryBenefit::BenefitCategory
      benefit_category_for_category_benefit =
          category_benefit.benefit_category();
  payments_data_manager().AddCreditCardBenefitForTest(
      std::move(category_benefit));

  CreditCardMerchantBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  test_api(merchant_benefit).SetStartTime(future_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_merchant_benefit =
          merchant_benefit.linked_card_instrument_id();
  url::Origin merchant_origin_for_merchant_benefit =
      *merchant_benefit.merchant_domains().begin();
  payments_data_manager().AddCreditCardBenefitForTest(
      std::move(merchant_benefit));

  // Should not return any benefits as no benefit is currently active.
  EXPECT_FALSE(payments_data_manager().GetFlatRateBenefitByInstrumentId(
      instrument_id_for_flat_rate_benefit));
  EXPECT_FALSE(
      payments_data_manager().GetCategoryBenefitByInstrumentIdAndCategory(
          instrument_id_for_category_benefit,
          benefit_category_for_category_benefit));
  EXPECT_FALSE(
      payments_data_manager().GetMerchantBenefitByInstrumentIdAndOrigin(
          instrument_id_for_merchant_benefit,
          merchant_origin_for_merchant_benefit));
}

// Tests benefit getters will not return expired benefits.
TEST_F(PaymentsDataManagerTest, GetExpiredCreditCardBenefits) {
  // Add Expired benefits.
  base::Time expired_time = AutofillClock::Now() - base::Days(5);

  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  test_api(flat_rate_benefit).SetExpiryTime(expired_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_flat_rate_benefit =
          flat_rate_benefit.linked_card_instrument_id();
  payments_data_manager().AddCreditCardBenefitForTest(
      std::move(flat_rate_benefit));

  CreditCardCategoryBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  test_api(category_benefit).SetExpiryTime(expired_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_category_benefit =
          category_benefit.linked_card_instrument_id();
  const CreditCardCategoryBenefit::BenefitCategory
      benefit_category_for_category_benefit =
          category_benefit.benefit_category();
  payments_data_manager().AddCreditCardBenefitForTest(
      std::move(category_benefit));

  CreditCardMerchantBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  test_api(merchant_benefit).SetExpiryTime(expired_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_merchant_benefit =
          merchant_benefit.linked_card_instrument_id();
  url::Origin merchant_origin_for_merchant_benefit =
      *merchant_benefit.merchant_domains().begin();
  payments_data_manager().AddCreditCardBenefitForTest(
      std::move(merchant_benefit));

  // Should not return any benefits as all of the benefits are expired.
  EXPECT_FALSE(payments_data_manager().GetFlatRateBenefitByInstrumentId(
      instrument_id_for_flat_rate_benefit));
  EXPECT_FALSE(
      payments_data_manager().GetCategoryBenefitByInstrumentIdAndCategory(
          instrument_id_for_category_benefit,
          benefit_category_for_category_benefit));
  EXPECT_FALSE(
      payments_data_manager().GetMerchantBenefitByInstrumentIdAndOrigin(
          instrument_id_for_merchant_benefit,
          merchant_origin_for_merchant_benefit));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PaymentsDataManagerTest, HasMaskedBankAccounts_ExpOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  BankAccount bank_account1 = test::CreatePixBankAccount(1234L);
  BankAccount bank_account2 = test::CreatePixBankAccount(5678L);
  ASSERT_TRUE(GetServerDataTable()->SetMaskedBankAccounts(
      {bank_account1, bank_account2}));
  // Refresh the PaymentsDataManager. Under normal circumstances with the flag
  // on, this step would load the bank accounts from the WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Verify that no bank accounts are loaded into PaymentsDataManager because
  // the experiment is turned off.
  EXPECT_FALSE(payments_data_manager().HasMaskedBankAccounts());
}

TEST_F(PaymentsDataManagerTest, HasMaskedBankAccounts_PaymentMethodsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  BankAccount bank_account1 = test::CreatePixBankAccount(1234L);
  BankAccount bank_account2 = test::CreatePixBankAccount(5678L);
  ASSERT_TRUE(GetServerDataTable()->SetMaskedBankAccounts(
      {bank_account1, bank_account2}));
  // We need to call `Refresh()` to ensure that the BankAccounts are loaded
  // again from the WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Disable payment methods prefs.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Verify that no bank accounts are loaded into PaymentsDataManager because
  // the AutofillPaymentMethodsEnabled pref is set to false.
  EXPECT_FALSE(payments_data_manager().HasMaskedBankAccounts());
}

TEST_F(PaymentsDataManagerTest, HasMaskedBankAccounts_NoMaskedBankAccounts) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);

  // If the user doesn't have any masked bank accounts, or if the masked bank
  // accounts are not synced to PaymentsDatamanager, HasMaskedBankAccounts
  // should return false.
  EXPECT_FALSE(payments_data_manager().HasMaskedBankAccounts());
}

TEST_F(PaymentsDataManagerTest, HasMaskedBankAccounts_MaskedBankAccountsExist) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  BankAccount bank_account1 = test::CreatePixBankAccount(1234L);
  BankAccount bank_account2 = test::CreatePixBankAccount(5678L);
  ASSERT_TRUE(GetServerDataTable()->SetMaskedBankAccounts(
      {bank_account1, bank_account2}));

  // We need to call `Refresh()` to ensure that the BankAccounts are loaded
  // again from the WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  EXPECT_TRUE(payments_data_manager().HasMaskedBankAccounts());
}

TEST_F(PaymentsDataManagerTest, GetMaskedBankAccounts_ExpOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  BankAccount bank_account1 = test::CreatePixBankAccount(1234L);
  BankAccount bank_account2 = test::CreatePixBankAccount(5678L);
  ASSERT_TRUE(GetServerDataTable()->SetMaskedBankAccounts(
      {bank_account1, bank_account2}));
  base::span<const BankAccount> bank_accounts =
      payments_data_manager().GetMaskedBankAccounts();
  // Since the PaymentsDataManager was initialized before adding the masked
  // bank accounts to the WebDatabase, we expect GetMaskedBankAccounts to return
  // an empty list.
  EXPECT_EQ(0u, bank_accounts.size());

  // Refresh the PaymentsDataManager. Under normal circumstances with the flag
  // on, this step would load the bank accounts from the WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Verify that no bank accounts are loaded into PaymentsDataManager because
  // the experiment is turned off.
  bank_accounts = payments_data_manager().GetMaskedBankAccounts();
  EXPECT_EQ(0u, bank_accounts.size());
}

TEST_F(PaymentsDataManagerTest, GetMaskedBankAccounts_PaymentMethodsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  BankAccount bank_account1 = test::CreatePixBankAccount(1234L);
  BankAccount bank_account2 = test::CreatePixBankAccount(5678L);
  ASSERT_TRUE(GetServerDataTable()->SetMaskedBankAccounts(
      {bank_account1, bank_account2}));
  // We need to call `Refresh()` to ensure that the BankAccounts are loaded
  // again from the WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Disable payment methods prefs.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Verify that no bank accounts are loaded into PaymentsDataManager because
  // the AutofillPaymentMethodsEnabled pref is set to false.
  EXPECT_THAT(payments_data_manager().GetMaskedBankAccounts(),
              testing::IsEmpty());
}

TEST_F(PaymentsDataManagerTest, GetMaskedBankAccounts_DatabaseUpdated) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  BankAccount bank_account1 = test::CreatePixBankAccount(1234L);
  BankAccount bank_account2 = test::CreatePixBankAccount(5678L);
  ASSERT_TRUE(GetServerDataTable()->SetMaskedBankAccounts(
      {bank_account1, bank_account2}));

  // Since the PaymentsDataManager was initialized before adding the masked
  // bank accounts to the WebDatabase, we expect GetMaskedBankAccounts to return
  // an empty list.
  base::span<const BankAccount> bank_accounts =
      payments_data_manager().GetMaskedBankAccounts();
  EXPECT_EQ(0u, bank_accounts.size());

  // We need to call `Refresh()` to ensure that the BankAccounts are loaded
  // again from the WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  bank_accounts = payments_data_manager().GetMaskedBankAccounts();
  EXPECT_EQ(2u, bank_accounts.size());
}

TEST_F(PaymentsDataManagerTest,
       MaskedBankAccountsIconsFetched_DatabaseUpdated) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  MockAutofillImageFetcher mock_image_fetcher;
  test_api(payments_data_manager()).SetImageFetcher(&mock_image_fetcher);

  BankAccount bank_account1(1234L, u"nickname", GURL("http://www.example1.com"),
                            u"bank_name", u"account_number",
                            BankAccount::AccountType::kChecking);
  BankAccount bank_account2(5678L, u"nickname", GURL("http://www.example2.com"),
                            u"bank_name", u"account_number",
                            BankAccount::AccountType::kChecking);
  ASSERT_TRUE(GetServerDataTable()->SetMaskedBankAccounts(
      {bank_account1, bank_account2}));

  EXPECT_CALL(mock_image_fetcher, FetchImagesForURLs);

  // We need to call `Refresh()` to ensure that the BankAccounts are loaded
  // again from the WebDatabase which triggers the call to fetch icons from
  // image fetcher.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
}

TEST_F(PaymentsDataManagerTest, HasEwalletAccounts_ExpOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillSyncEwalletAccounts);
  sync_pb::PaymentInstrument payment_instrument_1 =
      test::CreatePaymentInstrumentWithEwalletAccount(1234L);
  sync_pb::PaymentInstrument payment_instrument_2 =
      test::CreatePaymentInstrumentWithEwalletAccount(2345L);
  ASSERT_TRUE(GetServerDataTable()->SetPaymentInstruments(
      {payment_instrument_1, payment_instrument_2}));

  // Refresh the PaymentsDataManager. Under normal circumstances with the flag
  // on, this step would load the eWallet payment instruments from the
  // WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Verify that no eWallet accounts are loaded into PaymentsDataManager because
  // the experiment is turned off.
  EXPECT_FALSE(payments_data_manager().HasEwalletAccounts());
}

TEST_F(PaymentsDataManagerTest, HasEwalletAccounts_PaymentMethodsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSyncEwalletAccounts);
  sync_pb::PaymentInstrument payment_instrument_1 =
      test::CreatePaymentInstrumentWithEwalletAccount(1234L);
  sync_pb::PaymentInstrument payment_instrument_2 =
      test::CreatePaymentInstrumentWithEwalletAccount(2345L);
  ASSERT_TRUE(GetServerDataTable()->SetPaymentInstruments(
      {payment_instrument_1, payment_instrument_2}));

  // Refresh the PaymentsDataManager. Under normal circumstances with the flag
  // on, this step would load the bank accounts from the WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Disable payment methods prefs.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Verify that no eWallet accounts are loaded into PaymentsDataManager because
  // the AutofillPaymentMethodsEnabled pref is set to false.
  EXPECT_FALSE(payments_data_manager().HasEwalletAccounts());
}

TEST_F(PaymentsDataManagerTest, HasEwalletAccounts_NoEwalletAccounts) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSyncEwalletAccounts);

  // If the user doesn't have any eWallet accounts, or if the eWallet accounts
  // are not synced to PaymentsDatamanager, HasEwalletAccounts should return
  // false.
  EXPECT_FALSE(payments_data_manager().HasEwalletAccounts());
}

TEST_F(PaymentsDataManagerTest, HasEwalletAccounts_EwalletAccountsExist) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSyncEwalletAccounts);
  sync_pb::PaymentInstrument payment_instrument_1 =
      test::CreatePaymentInstrumentWithEwalletAccount(1234L);
  sync_pb::PaymentInstrument payment_instrument_2 =
      test::CreatePaymentInstrumentWithEwalletAccount(2345L);
  ASSERT_TRUE(GetServerDataTable()->SetPaymentInstruments(
      {payment_instrument_1, payment_instrument_2}));

  // Refresh the PaymentsDataManager. Under normal circumstances with the flag
  // on, this step would load the bank accounts from the WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  EXPECT_TRUE(payments_data_manager().HasEwalletAccounts());
}

TEST_F(PaymentsDataManagerTest, GetEwalletAccounts_ExpOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillSyncEwalletAccounts);
  sync_pb::PaymentInstrument payment_instrument_1 =
      test::CreatePaymentInstrumentWithEwalletAccount(1234L);
  sync_pb::PaymentInstrument payment_instrument_2 =
      test::CreatePaymentInstrumentWithEwalletAccount(2345L);
  ASSERT_TRUE(GetServerDataTable()->SetPaymentInstruments(
      {payment_instrument_1, payment_instrument_2}));

  base::span<const Ewallet> ewallet_accounts =
      payments_data_manager().GetEwalletAccounts();
  // Since the PaymentsDataManager was initialized before adding the eWallet
  // payment instruments to the WebDatabase, we expect GetEwalletAccounts to
  // return an empty list.
  EXPECT_EQ(0u, ewallet_accounts.size());

  // Refresh the PaymentsDataManager. Under normal circumstances with the flag
  // on, this step would load the bank accounts from the WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Verify that no eWallet accounts are loaded into PaymentsDataManager because
  // the experiment is turned off.
  ewallet_accounts = payments_data_manager().GetEwalletAccounts();
  EXPECT_EQ(0u, ewallet_accounts.size());
}

TEST_F(PaymentsDataManagerTest, GetEwalletAccounts_PaymentMethodsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSyncEwalletAccounts);
  sync_pb::PaymentInstrument payment_instrument_1 =
      test::CreatePaymentInstrumentWithEwalletAccount(1234L);
  sync_pb::PaymentInstrument payment_instrument_2 =
      test::CreatePaymentInstrumentWithEwalletAccount(2345L);
  ASSERT_TRUE(GetServerDataTable()->SetPaymentInstruments(
      {payment_instrument_1, payment_instrument_2}));

  // We need to call `Refresh()` to ensure that the eWallet payment instruments
  // are loaded again from the WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Disable payment methods prefs.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Verify that no eWallet accounts are loaded into PaymentsDataManager because
  // the AutofillPaymentMethodsEnabled pref is set to false.
  EXPECT_THAT(payments_data_manager().GetEwalletAccounts(), testing::IsEmpty());
}

TEST_F(PaymentsDataManagerTest, GetEwalletAccounts_DatabaseUpdated) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSyncEwalletAccounts);
  sync_pb::PaymentInstrument payment_instrument_1 =
      test::CreatePaymentInstrumentWithEwalletAccount(1234L);
  sync_pb::PaymentInstrument payment_instrument_2 =
      test::CreatePaymentInstrumentWithEwalletAccount(2345L);
  ASSERT_TRUE(GetServerDataTable()->SetPaymentInstruments(
      {payment_instrument_1, payment_instrument_2}));

  // Since the PaymentsDataManager was initialized before adding the eWallet
  // payment instruments to the WebDatabase, we expect GetEwalletAccounts to
  // return an empty list.
  base::span<const Ewallet> ewallet_accounts =
      payments_data_manager().GetEwalletAccounts();
  EXPECT_EQ(0u, ewallet_accounts.size());

  // We need to call `Refresh()` to ensure that the eWallet payment instruments
  // are loaded again from the WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  ewallet_accounts = payments_data_manager().GetEwalletAccounts();
  EXPECT_EQ(2u, ewallet_accounts.size());
}

TEST_F(PaymentsDataManagerTest, GetEwalletAccounts_VerifyFields) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSyncEwalletAccounts);
  sync_pb::PaymentInstrument payment_instrument =
      test::CreatePaymentInstrumentWithEwalletAccount(1234L);
  payment_instrument.mutable_ewallet_details()->add_supported_payment_link_uris(
      "supported_payment_link_uri_2");
  ASSERT_TRUE(
      GetServerDataTable()->SetPaymentInstruments({payment_instrument}));

  // Since the PaymentsDataManager was initialized before adding the eWallet
  // payment instruments to the WebDatabase, we expect GetEwalletAccounts to
  // return an empty list.
  base::span<const Ewallet> ewallet_accounts =
      payments_data_manager().GetEwalletAccounts();
  EXPECT_EQ(0u, ewallet_accounts.size());

  // We need to call `Refresh()` to ensure that the eWallet payment instruments
  // are loaded again from the WebDatabase.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  ewallet_accounts = payments_data_manager().GetEwalletAccounts();
  EXPECT_EQ(1u, ewallet_accounts.size());

  const Ewallet ewallet_account = ewallet_accounts.front();
  EXPECT_EQ(ewallet_account.payment_instrument().instrument_id(),
            payment_instrument.instrument_id());
  EXPECT_EQ(ewallet_account.payment_instrument().instrument_id(),
            payment_instrument.instrument_id());
  EXPECT_EQ(ewallet_account.payment_instrument().nickname(),
            base::UTF8ToUTF16(payment_instrument.nickname()));
  EXPECT_EQ(ewallet_account.payment_instrument().display_icon_url().spec(),
            payment_instrument.display_icon_url());
  EXPECT_EQ(ewallet_account.payment_instrument().is_fido_enrolled(),
            payment_instrument.device_details().is_fido_enrolled());
  EXPECT_EQ(
      ewallet_account.ewallet_name(),
      base::UTF8ToUTF16(payment_instrument.ewallet_details().ewallet_name()));
  EXPECT_EQ(ewallet_account.account_display_name(),
            base::UTF8ToUTF16(
                payment_instrument.ewallet_details().account_display_name()));
  EXPECT_EQ(ewallet_account.supported_payment_link_uris().size(),
            static_cast<size_t>(payment_instrument.ewallet_details()
                                    .supported_payment_link_uris()
                                    .size()));
}

TEST_F(PaymentsDataManagerTest, EwalletAccountsIconsFetched_DatabaseUpdated) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillSyncEwalletAccounts);
  MockAutofillImageFetcher mock_image_fetcher;
  test_api(payments_data_manager()).SetImageFetcher(&mock_image_fetcher);

  sync_pb::PaymentInstrument payment_instrument;
  payment_instrument.set_instrument_id(1234L);
  payment_instrument.set_display_icon_url("http://www.example1.com");
  payment_instrument.mutable_ewallet_details();
  ASSERT_TRUE(
      GetServerDataTable()->SetPaymentInstruments({payment_instrument}));

  EXPECT_CALL(mock_image_fetcher, FetchImagesForURLs);

  // We need to call `Refresh()` to ensure that the eWallet payment instruments
  // are loaded again from the WebDatabase which triggers the call to fetch
  // icons from image fetcher.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(PaymentsDataManagerTest,
       OnBenefitsPrefChange_PrefIsOn_LoadsCardBenefits) {
  // Add the card benefits to the web database.
  std::vector<CreditCardBenefit> card_benefits;
  card_benefits.push_back(test::GetActiveCreditCardFlatRateBenefit());
  card_benefits.push_back(test::GetActiveCreditCardCategoryBenefit());
  card_benefits.push_back(test::GetActiveCreditCardMerchantBenefit());
  SetCreditCardBenefits(card_benefits);

  // Verify that the card benefits are not loaded from the web database.
  ASSERT_EQ(0U, test_api(payments_data_manager()).GetCreditCardBenefitsCount());

  prefs::SetPaymentCardBenefits(prefs_.get(), true);
  WaitForOnPaymentsDataChanged();

  // Verify that the card benefits are loaded from the web database.
  ASSERT_EQ(card_benefits.size(),
            test_api(payments_data_manager()).GetCreditCardBenefitsCount());
}

TEST_F(PaymentsDataManagerTest,
       OnAutofillPaymentsCardBenefitsPrefChange_PrefIsOff_ClearsCardBenefits) {
  // Enable card benefits sync flag.
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableCardBenefitsSync);
  // Add the card benefits to the web database.
  std::vector<CreditCardBenefit> card_benefits;
  card_benefits.push_back(test::GetActiveCreditCardFlatRateBenefit());
  card_benefits.push_back(test::GetActiveCreditCardCategoryBenefit());
  card_benefits.push_back(test::GetActiveCreditCardMerchantBenefit());
  SetCreditCardBenefits(card_benefits);
  // Refresh to load the card benefits from the web database. It requires
  // enabling card benefits sync flag.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  ASSERT_EQ(card_benefits.size(),
            test_api(payments_data_manager()).GetCreditCardBenefitsCount());

  // Disable autofill payment card benefits pref and check that no benefits
  // are returned.
  prefs::SetPaymentCardBenefits(prefs_.get(), false);
  ASSERT_EQ(0U, test_api(payments_data_manager()).GetCreditCardBenefitsCount());
}

// Tests that card benefits are not saved in PaymentsDataManager if the card
// benefits pref is disabled.
TEST_F(PaymentsDataManagerTest,
       OnAutofillPaymentsCardBenefits_PrefIsOff_BenefitsAreNotReturned) {
  // Enable card benefits sync flag.
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableCardBenefitsSync);
  prefs::SetPaymentCardBenefits(prefs_.get(), false);

  // Add the card benefits to the web database.
  std::vector<CreditCardBenefit> card_benefits;
  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  CreditCardCategoryBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  CreditCardMerchantBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  card_benefits.push_back(flat_rate_benefit);
  card_benefits.push_back(category_benefit);
  card_benefits.push_back(merchant_benefit);
  SetCreditCardBenefits(card_benefits);

  // Refresh to load the card benefits from the web database. Make sure no card
  // benefits are saved to PaymentsDataManager.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  ASSERT_EQ(0u, test_api(payments_data_manager()).GetCreditCardBenefitsCount());

  // Ensure no card benefits are returned.
  EXPECT_EQ(std::nullopt,
            payments_data_manager().GetFlatRateBenefitByInstrumentId(
                flat_rate_benefit.linked_card_instrument_id()));
  EXPECT_EQ(std::nullopt,
            payments_data_manager().GetMerchantBenefitByInstrumentIdAndOrigin(
                merchant_benefit.linked_card_instrument_id(),
                *merchant_benefit.merchant_domains().begin()));
  EXPECT_EQ(std::nullopt,
            payments_data_manager().GetCategoryBenefitByInstrumentIdAndCategory(
                category_benefit.linked_card_instrument_id(),
                category_benefit.benefit_category()));
}

// Tests that card benefits are not saved in PaymentsDataManager if the card
// benefits sync flag is disabled.
TEST_F(PaymentsDataManagerTest,
       OnAutofillPaymentsCardBenefits_SyncFlagIsOff_BenefitsAreNotReturned) {
  // Disable card benefits sync flag.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillEnableCardBenefitsSync);
  // Set card benefits preference to true. This will immediately trigger
  // an attempt to load Benefits due to the listener, so wait for PayDM.
  prefs::SetPaymentCardBenefits(prefs_.get(), true);
  WaitForOnPaymentsDataChanged();
  // Add the card benefits to the web database.
  std::vector<CreditCardBenefit> card_benefits;
  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  CreditCardCategoryBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  CreditCardMerchantBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  card_benefits.push_back(flat_rate_benefit);
  card_benefits.push_back(category_benefit);
  card_benefits.push_back(merchant_benefit);
  SetCreditCardBenefits(card_benefits);

  // Refresh to load the card benefits from the web database. Make sure no card
  // benefits are saved to PaymentsDataManager.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  ASSERT_EQ(0u, test_api(payments_data_manager()).GetCreditCardBenefitsCount());

  // Ensure no card benefits are returned.
  EXPECT_EQ(std::nullopt,
            payments_data_manager().GetFlatRateBenefitByInstrumentId(
                flat_rate_benefit.linked_card_instrument_id()));
  EXPECT_EQ(std::nullopt,
            payments_data_manager().GetMerchantBenefitByInstrumentIdAndOrigin(
                merchant_benefit.linked_card_instrument_id(),
                *merchant_benefit.merchant_domains().begin()));
  EXPECT_EQ(std::nullopt,
            payments_data_manager().GetCategoryBenefitByInstrumentIdAndCategory(
                category_benefit.linked_card_instrument_id(),
                category_benefit.benefit_category()));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(PaymentsDataManagerTest, AddAndGetCreditCardArtImage) {
  gfx::Image expected_image = gfx::test::CreateImage(40, 24);
  std::unique_ptr<CreditCardArtImage> credit_card_art_image =
      std::make_unique<CreditCardArtImage>(GURL("https://www.example.com"),
                                           expected_image);
  std::vector<std::unique_ptr<CreditCardArtImage>> images;
  images.push_back(std::move(credit_card_art_image));
  test_api(payments_data_manager()).OnCardArtImagesFetched(std::move(images));

  gfx::Image* actual_image =
      payments_data_manager().GetCreditCardArtImageForUrl(
          GURL("https://www.example.com"));
  ASSERT_TRUE(actual_image);
  EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, *actual_image));

  // TODO(crbug.com/40210242): Look into integrating with
  // PaymentsDataManagerMock and checking that
  // PaymentsDataManager::FetchImagesForUrls() does not get triggered when
  // PaymentsDataManager::GetCachedCardArtImageForUrl() is called.
  gfx::Image* cached_image =
      payments_data_manager().GetCachedCardArtImageForUrl(
          GURL("https://www.example.com"));
  ASSERT_TRUE(cached_image);
  EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, *cached_image));
}

TEST_F(PaymentsDataManagerTest,
       TestNoImageFetchingAttemptForCardsWithInvalidCardArtUrls) {
  base::HistogramTester histogram_tester;

  gfx::Image* actual_image =
      payments_data_manager().GetCreditCardArtImageForUrl(GURL());
  EXPECT_FALSE(actual_image);
  EXPECT_EQ(0, histogram_tester.GetTotalSum("Autofill.ImageFetcher.Result"));
}

TEST_F(PaymentsDataManagerTest, ProcessCardArtUrlChanges) {
  MockAutofillImageFetcher mock_image_fetcher;
  test_api(payments_data_manager()).SetImageFetcher(&mock_image_fetcher);
  auto wait_for_fetch_images_for_url = [&] {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_image_fetcher, FetchImagesForURLs)
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
    run_loop.Run();
  };

  CreditCard card = test::GetMaskedServerCardVisa();
  card.set_server_id("card_server_id");
  test_api(payments_data_manager()).AddServerCreditCard(card);
  WaitForOnPaymentsDataChanged();

  card.set_server_id("card_server_id");
  card.set_card_art_url(GURL("https://www.example.com/card1"));
  std::vector<GURL> updated_urls;
  updated_urls.emplace_back("https://www.example.com/card1");

  test_api(payments_data_manager()).AddServerCreditCard(card);
  wait_for_fetch_images_for_url();

  card.set_card_art_url(GURL("https://www.example.com/card2"));
  updated_urls.clear();
  updated_urls.emplace_back("https://www.example.com/card2");

  test_api(payments_data_manager()).AddServerCreditCard(card);
  wait_for_fetch_images_for_url();
}
#endif

// Params:
// 1. Whether the benefits toggle is turned on or off.
// 2. Whether the American Express benefits flag is enabled.
// 3. Whether the Capital One benefits flag is enabled.
class PaymentsDataManagerStartupBenefitsTest
    : public PaymentsDataManagerHelper,
      public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  PaymentsDataManagerStartupBenefitsTest() {
    feature_list_.InitWithFeatureStates(
        /*feature_states=*/
        {{features::kAutofillEnableCardBenefitsForAmericanExpress,
          AreAmericanExpressBenefitsEnabled()},
         {features::kAutofillEnableCardBenefitsForCapitalOne,
          AreCapitalOneBenefitsEnabled()}});
    SetUpTest();
  }

  ~PaymentsDataManagerStartupBenefitsTest() override = default;

  bool IsBenefitsPrefTurnedOn() const { return std::get<0>(GetParam()); }
  bool AreAmericanExpressBenefitsEnabled() const {
    return std::get<1>(GetParam());
  }
  bool AreCapitalOneBenefitsEnabled() const { return std::get<2>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         PaymentsDataManagerStartupBenefitsTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

// Tests that on startup we log the value of the card benefits pref.
TEST_P(PaymentsDataManagerStartupBenefitsTest,
       LogIsCreditCardBenefitsEnabledAtStartup) {
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), true);
  prefs::SetPaymentCardBenefits(prefs_.get(), IsBenefitsPrefTurnedOn());
  base::HistogramTester histogram_tester;
  ResetPaymentsDataManager();
  if (!AreAmericanExpressBenefitsEnabled() && !AreCapitalOneBenefitsEnabled()) {
    histogram_tester.ExpectTotalCount(
        "Autofill.PaymentMethods.CardBenefitsIsEnabled.Startup", 0);
  } else {
    histogram_tester.ExpectUniqueSample(
        "Autofill.PaymentMethods.CardBenefitsIsEnabled.Startup",
        IsBenefitsPrefTurnedOn(), 1);
  }
}

// Tests that on startup if payment methods are disabled we don't log if
// benefits are enabled/disabled.
TEST_F(PaymentsDataManagerTest,
       LogIsCreditCardBenefitsEnabledAtStartup_PaymentMethodsDisabled) {
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  base::HistogramTester histogram_tester;
  ResetPaymentsDataManager();
  histogram_tester.ExpectTotalCount(
      "Autofill.PaymentMethods.CardBenefitsIsEnabled.Startup", 0);
}

// Tests that on startup if there is no pref service for the PaymentsDataManager
// we don't log if benefits are enabled/disabled.
TEST_F(PaymentsDataManagerTest,
       LogIsCreditCardBenefitsEnabledAtStartup_NullPrefService) {
  base::HistogramTester histogram_tester;
  PaymentsDataManager payments_data_manager = PaymentsDataManager(
      /*profile_database=*/nullptr,
      /*account_database=*/nullptr,
      /*image_fetcher=*/nullptr,
      /*shared_storage_handler=*/nullptr,
      /*pref_service=*/nullptr,
      /*sync_service=*/nullptr,
      /*identity_manager=*/nullptr,
      /*variations_country_code=*/GeoIpCountryCode("US"),
      /*app-locale=*/"en-US");

  histogram_tester.ExpectTotalCount(
      "Autofill.PaymentMethods.CardBenefitsIsEnabled.Startup", 0);
}

// Ensure that verified credit cards can be saved via
// OnAcceptedLocalCreditCardSave.
TEST_F(PaymentsDataManagerTest, OnAcceptedLocalCreditCardSaveWithVerifiedData) {
  // Start with a verified credit card.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  EXPECT_TRUE(credit_card.IsVerified());

  // Add the credit card to the database.
  payments_data_manager().AddCreditCard(credit_card);

  // Make sure everything is set up correctly.
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  CreditCard new_verified_card = credit_card;
  new_verified_card.set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  new_verified_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"B. Small");
  EXPECT_TRUE(new_verified_card.IsVerified());

  payments_data_manager().OnAcceptedLocalCreditCardSave(new_verified_card);

  WaitForOnPaymentsDataChanged();

  // Expect that the saved credit card is updated.
  const std::vector<CreditCard*>& results =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(u"B. Small", results[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
}

// Ensure that new IBANs can be updated and saved via
// `OnAcceptedLocalIbanSave()`.
TEST_F(PaymentsDataManagerTest, OnAcceptedLocalIbanSave) {
  // Start with a new IBAN.
  Iban iban0;
  iban0.set_value(std::u16string(test::kIbanValue16));
  // Add the IBAN to the database.
  std::string guid = payments_data_manager().OnAcceptedLocalIbanSave(iban0);
  iban0.set_identifier(Iban::Guid(guid));
  iban0.set_record_type(Iban::kLocalIban);

  // Make sure everything is set up correctly.
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(1U, payments_data_manager().GetLocalIbans().size());

  // Creates a new IBAN and call `OnAcceptedLocalIbanSave()` and verify that
  // the new IBAN is saved.
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  guid = payments_data_manager().OnAcceptedLocalIbanSave(iban1);
  WaitForOnPaymentsDataChanged();
  iban1.set_identifier(Iban::Guid(guid));
  iban1.set_record_type(Iban::kLocalIban);

  // Expect that the new IBAN is added.
  ASSERT_EQ(2U, payments_data_manager().GetLocalIbans().size());

  std::vector<const Iban*> ibans;
  ibans.push_back(&iban0);
  ibans.push_back(&iban1);
  // Verify that we've loaded the IBAN from the web database.
  ExpectSameElements(ibans, payments_data_manager().GetLocalIbans());

  // Creates a new `iban2` which has the same value as `iban0` but with
  // different nickname and call `OnAcceptedLocalIbanSave()`.
  Iban iban2 = iban0;
  iban2.set_nickname(u"Nickname 2");
  payments_data_manager().OnAcceptedLocalIbanSave(iban2);
  WaitForOnPaymentsDataChanged();
  // Updates the nickname for `iban1` and call `OnAcceptedLocalIbanSave()`.
  iban1.set_nickname(u"Nickname 1 updated");
  payments_data_manager().OnAcceptedLocalIbanSave(iban1);
  WaitForOnPaymentsDataChanged();

  ibans.clear();
  ibans.push_back(&iban1);
  ibans.push_back(&iban2);
  // Expect that the existing IBANs are updated.
  ASSERT_EQ(2U, payments_data_manager().GetLocalIbans().size());

  // Verify that we've loaded the IBANs from the web database.
  ExpectSameElements(ibans, payments_data_manager().GetLocalIbans());

  // Call `OnAcceptedLocalIbanSave()` with the same iban1, verify that nothing
  // changes.
  payments_data_manager().OnAcceptedLocalIbanSave(iban1);
  ExpectSameElements(ibans, payments_data_manager().GetLocalIbans());

  // Reset the PaymentsDataManager. This tests that the IBANs are persisted
  // in the local web database even if the browser is re-loaded, ensuring that
  // the user can load the IBANs from the local web database on browser startup.
  ResetPaymentsDataManager();
  ExpectSameElements(ibans, payments_data_manager().GetLocalIbans());
}

TEST_F(PaymentsDataManagerTest, IsKnownCard_MatchesMaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "2110" /* last 4 digits */, "12", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(payments_data_manager().IsKnownCard(cardToCompare));
}

TEST_F(PaymentsDataManagerTest, IsKnownCard_MatchesLocalCard) {
  // Add a local card.
  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "4234 5678 9012 2110" /* Visa */, "04", "2999", "1");
  payments_data_manager().AddCreditCard(credit_card0);

  // Make sure everything is set up correctly.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234567890122110" /* Visa */);
  ASSERT_TRUE(payments_data_manager().IsKnownCard(cardToCompare));
}

TEST_F(PaymentsDataManagerTest, IsKnownCard_TypeDoesNotMatch) {
  // Add a local card.
  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "4234 5678 9012 2110" /* Visa */, "04", "2999", "1");
  payments_data_manager().AddCreditCard(credit_card0);

  // Make sure everything is set up correctly.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"5105 1051 0510 2110" /* American Express */);
  ASSERT_FALSE(payments_data_manager().IsKnownCard(cardToCompare));
}

TEST_F(PaymentsDataManagerTest, IsKnownCard_LastFourDoesNotMatch) {
  // Add a local card.
  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "4234 5678 9012 2110" /* Visa */, "04", "2999", "1");
  payments_data_manager().AddCreditCard(credit_card0);

  // Make sure everything is set up correctly.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 0000" /* Visa */);
  ASSERT_FALSE(payments_data_manager().IsKnownCard(cardToCompare));
}

TEST_F(PaymentsDataManagerTest, IsServerCard_DuplicateOfMaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "2110" /* last 4 digits */, "12", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCards(server_cards);

  // Add a dupe local card of the masked server card.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Emmet Dalton",
                          "4234 5678 9012 2110" /* Visa */, "12", "2999", "1");
  payments_data_manager().AddCreditCard(local_card);

  // Make sure everything is set up correctly.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(2U, payments_data_manager().GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(payments_data_manager().IsServerCard(&cardToCompare));
  ASSERT_TRUE(payments_data_manager().IsServerCard(&local_card));
}

TEST_F(PaymentsDataManagerTest, IsServerCard_AlreadyServerCard) {
  std::vector<CreditCard> server_cards;
  // Create a masked server card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "2110" /* Visa */,
                          "01", "2999", "1");
  masked_card.SetNetworkForMaskedCard(kVisaCard);
  server_cards.push_back(masked_card);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  ASSERT_TRUE(payments_data_manager().IsServerCard(&masked_card));
}

TEST_F(PaymentsDataManagerTest, IsServerCard_UniqueLocalCard) {
  // Add a unique local card.
  CreditCard local_card("1141084B-72D7-4B73-90CF-3D6AC154673B",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  payments_data_manager().AddCreditCard(local_card);

  // Make sure everything is set up correctly.
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  ASSERT_FALSE(payments_data_manager().IsServerCard(&local_card));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PaymentsDataManagerSyncTransportModeTest,
       ShouldShowCardsFromAccountOption_FlagOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillRemovePaymentsButterDropdown);
  // The method should return false if one of these is not respected:
  //   * The sync_service is not null
  //   * The sync feature is not enabled
  //   * The user has server cards
  //   * The user has not opted-in to seeing their account cards
  // Start by setting everything up, then making each of these conditions false
  // independently, one by one.

  // Set everything up so that the proposition should be shown.

  // Set a server credit card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "0005" /* American Express */, "04", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kAmericanExpressCard);
  SetServerCards(server_cards);
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Make sure the function returns true.
  EXPECT_TRUE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Set that the user already opted-in. Check that the function now returns
  // false.
  CoreAccountId account_id =
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin);
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(prefs_.get(), account_id,
                                                       true);
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Re-opt the user out. Check that the function now returns true.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(prefs_.get(), account_id,
                                                       false);
  EXPECT_TRUE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Set that the user has no server cards. Check that the function now returns
  // false.
  SetServerCards({});
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Re-set some server cards. Check that the function now returns true.
  SetServerCards(server_cards);
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  EXPECT_TRUE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Set that the user enabled the sync feature. Check that the function now
  // returns false.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync);
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Re-disable the sync feature. Check that the function now returns true.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  EXPECT_TRUE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Set a null sync service. Check that the function now returns false.
  payments_data_manager().SetSyncServiceForTest(nullptr);
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());
}

TEST_F(PaymentsDataManagerSyncTransportModeTest,
       ShouldShowCardsFromAccountOption_FlagOn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillRemovePaymentsButterDropdown);
  // Set up a new, non-sync-consented account, with a card, in transport mode.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "0005" /* American Express */, "04", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kAmericanExpressCard);
  SetServerCards(server_cards);
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // The function should returns false because the
  // kAutofillRemovePaymentsButterDropdown flag is enabled.
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());
}

TEST_F(PaymentsDataManagerSyncTransportModeTest,
       ShouldSuggestServerPaymentMethods_FlagOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillRemovePaymentsButterDropdown);

  // Set up a new, non-sync-consented account in transport mode.
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(sync_service_.HasSyncConsent());
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill,
                 syncer::UserSelectableType::kPayments});

  // Server payment methods should not be suggested because the user has not
  // acknowledged the notice to begin seeing them.
  EXPECT_FALSE(
      test_api(payments_data_manager()).ShouldSuggestServerPaymentMethods());
}

TEST_F(PaymentsDataManagerSyncTransportModeTest,
       ShouldSuggestServerPaymentMethods_FlagOn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillRemovePaymentsButterDropdown);

  // Set up a new, non-sync-consented account in transport mode.
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(sync_service_.HasSyncConsent());
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill,
                 syncer::UserSelectableType::kPayments});

  // Server payment methods should be suggested because the flag is enabled.
  EXPECT_TRUE(
      test_api(payments_data_manager()).ShouldSuggestServerPaymentMethods());
}

#else   // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) &&
        // !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PaymentsDataManagerSyncTransportModeTest,
       ShouldShowCardsFromAccountOption) {
  // The method should return false if one of these is not respected:
  //   * The sync_service is not null
  //   * The sync feature is not enabled
  //   * The user has server cards
  //   * The user has not opted-in to seeing their account cards
  // Start by setting everything up, then making each of these conditions false
  // independently, one by one.

  // Set everything up so that the proposition should be shown on Desktop.

  // Set a server credit card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "0005" /* American Express */, "04", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kMasterCard);
  SetServerCards(server_cards);
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();

  // Make sure the function returns false.
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Set that the user already opted-in. Check that the function still returns
  // false.
  CoreAccountId account_id =
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin);
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(prefs_.get(), account_id,
                                                       true);
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Re-opt the user out. Check that the function now returns true.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(prefs_.get(), account_id,
                                                       false);
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Set that the user has no server cards. Check that the function still
  // returns false.
  SetServerCards({});
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Re-set some server cards. Check that the function still returns false.
  SetServerCards(server_cards);
  payments_data_manager().Refresh();
  WaitForOnPaymentsDataChanged();
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Set that the user enabled the sync feature. Check that the function still
  // returns false.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync);
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Re-disable the sync feature. Check that the function still returns false.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());

  // Set a null sync service. Check that the function still returns false.
  payments_data_manager().SetSyncServiceForTest(nullptr);
  EXPECT_FALSE(payments_data_manager().ShouldShowCardsFromAccountOption());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) &&
        // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PaymentsDataManagerSyncTransportModeTest,
       GetPaymentsSigninStateForMetrics) {
  // Make sure a non-sync-consented account is available for the first tests.
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(sync_service_.HasSyncConsent());
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill,
                 syncer::UserSelectableType::kPayments});

  EXPECT_EQ(AutofillMetrics::PaymentsSigninState::
                kSignedInAndWalletSyncTransportEnabled,
            payments_data_manager().GetPaymentsSigninStateForMetrics());

  // Check that the sync state is |SignedIn| if the sync service does not have
  // wallet data active.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet(
          {syncer::UserSelectableType::kAutofill}));
  EXPECT_EQ(AutofillMetrics::PaymentsSigninState::kSignedIn,
            payments_data_manager().GetPaymentsSigninStateForMetrics());

  // Nothing should change if |kAutofill| is also removed.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_EQ(AutofillMetrics::PaymentsSigninState::kSignedIn,
            payments_data_manager().GetPaymentsSigninStateForMetrics());

// ClearPrimaryAccount is not supported on CrOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Check that the sync state is |SignedOut| when the account info is empty.
  {
    identity_test_env_.ClearPrimaryAccount();
    sync_service_.SetSignedOut();
    EXPECT_EQ(AutofillMetrics::PaymentsSigninState::kSignedOut,
              payments_data_manager().GetPaymentsSigninStateForMetrics());
  }
#endif

  // Simulate that the user has enabled the sync feature.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // MakePrimaryAccountAvailable is not supported on CrOS.
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync);
#else
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "syncuser@example.com", signin::ConsentLevel::kSync);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account_info);
#endif

  // Check that the sync state is |SignedInAndSyncFeature| if the sync feature
  // is enabled.
  EXPECT_EQ(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled,
      payments_data_manager().GetPaymentsSigninStateForMetrics());
}

// On mobile, no dedicated opt-in is required for WalletSyncTransport - the
// user is always considered opted-in and thus this test doesn't make sense.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PaymentsDataManagerSyncTransportModeTest, OnUserAcceptedUpstreamOffer) {
  ///////////////////////////////////////////////////////////
  // kSignedInAndWalletSyncTransportEnabled
  ///////////////////////////////////////////////////////////
  // Make sure a primary account with no sync consent is available so
  // AUTOFILL_WALLET_DATA can run in sync-transport mode.
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  CoreAccountInfo active_info =
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, active_info);

  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill,
                 syncer::UserSelectableType::kPayments});
  // Make sure there are no opt-ins recorded yet.
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  // Account wallet storage only makes sense together with support for
  // unconsented primary accounts, i.e. on Win/Mac/Linux.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(
      !sync_service_.IsSyncFeatureEnabled() &&
      sync_service_.GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA));

  // Make sure an opt-in gets recorded if the user accepted an Upstream offer.
  payments_data_manager().OnUserAcceptedUpstreamOffer();
  EXPECT_TRUE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                      active_info.account_id));

  // Clear the prefs.
  prefs::ClearSyncTransportOptIns(prefs_.get());
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  ///////////////////////////////////////////////////////////
  // kSignedIn
  ///////////////////////////////////////////////////////////
  // Disable the wallet data type. kSignedInAndWalletSyncTransportEnabled
  // shouldn't be available.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_TRUE(!sync_service_.GetAccountInfo().IsEmpty());

  // Make sure an opt-in does not get recorded even if the user accepted an
  // Upstream offer.
  payments_data_manager().OnUserAcceptedUpstreamOffer();
  EXPECT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  // Clear the prefs.
  prefs::ClearSyncTransportOptIns(prefs_.get());
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  ///////////////////////////////////////////////////////////
  // kSignedOut
  ///////////////////////////////////////////////////////////
  identity_test_env_.ClearPrimaryAccount();
  sync_service_.SetSignedOut();
  {
    EXPECT_TRUE(sync_service_.GetAccountInfo().IsEmpty());

    // Make sure an opt-in does not get recorded even if the user accepted an
    // Upstream offer.
    payments_data_manager().OnUserAcceptedUpstreamOffer();
    EXPECT_FALSE(prefs::IsUserOptedInWalletSyncTransport(
        prefs_.get(), active_info.account_id));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  ///////////////////////////////////////////////////////////
  // kSignedInAndSyncFeature
  ///////////////////////////////////////////////////////////
  identity_test_env_.MakePrimaryAccountAvailable(active_info.email,
                                                 signin::ConsentLevel::kSync);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, active_info);
  {
    EXPECT_TRUE(sync_service_.IsSyncFeatureEnabled());

    // Make sure an opt-in does not get recorded even if the user accepted an
    // Upstream offer.
    payments_data_manager().OnUserAcceptedUpstreamOffer();
    EXPECT_FALSE(prefs::IsUserOptedInWalletSyncTransport(
        prefs_.get(), active_info.account_id));
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
TEST_F(PaymentsDataManagerTest,
       AutofillPaymentMethodsMandatoryReauthAlwaysEnabledOnAutomotive) {
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should only run on automotive.";
  }

  EXPECT_TRUE(payments_data_manager().IsPaymentMethodsMandatoryReauthEnabled());

  EXPECT_CHECK_DEATH_WITH(
      {
        payments_data_manager().SetPaymentMethodsMandatoryReauthEnabled(false);
      },
      "This feature should not be able to be turned off on automotive "
      "devices.");

  EXPECT_TRUE(payments_data_manager().IsPaymentMethodsMandatoryReauthEnabled());
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// Test that setting the `kAutofillEnablePaymentsMandatoryReauth` pref works
// correctly.
TEST_F(PaymentsDataManagerTest, AutofillPaymentMethodsMandatoryReauthEnabled) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  EXPECT_FALSE(
      payments_data_manager().IsPaymentMethodsMandatoryReauthEnabled());
  payments_data_manager().SetPaymentMethodsMandatoryReauthEnabled(true);
  EXPECT_TRUE(payments_data_manager().IsPaymentMethodsMandatoryReauthEnabled());
  payments_data_manager().SetPaymentMethodsMandatoryReauthEnabled(false);
  EXPECT_FALSE(
      payments_data_manager().IsPaymentMethodsMandatoryReauthEnabled());
}

// Test that
// `PaymentsDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo()`
// only returns that we should show the promo when we are below the max counter
// limit for showing the promo.
TEST_F(
    PaymentsDataManagerTest,
    ShouldShowPaymentMethodsMandatoryReauthPromo_MaxValueForPromoShownCounterReached) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester;
  for (int i = 0; i < prefs::kMaxValueForMandatoryReauthPromoShownCounter;
       i++) {
    // This also verifies that ShouldShowPaymentMethodsMandatoryReauthPromo()
    // works as expected when below the max cap.
    EXPECT_TRUE(
        payments_data_manager().ShouldShowPaymentMethodsMandatoryReauthPromo());
    payments_data_manager()
        .IncrementPaymentMethodsMandatoryReauthPromoShownCounter();
  }

  EXPECT_FALSE(
      payments_data_manager().ShouldShowPaymentMethodsMandatoryReauthPromo());
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.MandatoryReauth.CheckoutFlow."
      "ReauthOfferOptInDecision2",
      autofill_metrics::MandatoryReauthOfferOptInDecision::
          kBlockedByStrikeDatabase,
      1);
}

// Test that
// `PaymentsDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo()`
// returns that we should not show the promo if the user already opted in.
TEST_F(PaymentsDataManagerTest,
       ShouldShowPaymentMethodsMandatoryReauthPromo_UserOptedInAlready) {
#if BUILDFLAG(IS_ANDROID)
  // Opt-in prompts are not shown on automotive as mandatory reauth is always
  // enabled.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester;
  // Simulate user is already opted in.
  payments_data_manager().SetPaymentMethodsMandatoryReauthEnabled(true);

  EXPECT_FALSE(
      payments_data_manager().ShouldShowPaymentMethodsMandatoryReauthPromo());
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.MandatoryReauth.CheckoutFlow."
      "ReauthOfferOptInDecision2",
      autofill_metrics::MandatoryReauthOfferOptInDecision::kAlreadyOptedIn, 1);
}

// Test that
// `PaymentsDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo()`
// returns that we should not show the promo if the user has already opted out.
TEST_F(PaymentsDataManagerTest,
       ShouldShowPaymentMethodsMandatoryReauthPromo_UserOptedOut) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester;
  // Simulate user is already opted out.
  payments_data_manager().SetPaymentMethodsMandatoryReauthEnabled(false);

  EXPECT_FALSE(
      payments_data_manager().ShouldShowPaymentMethodsMandatoryReauthPromo());
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.MandatoryReauth.CheckoutFlow."
      "ReauthOfferOptInDecision2",
      autofill_metrics::MandatoryReauthOfferOptInDecision::kAlreadyOptedOut, 1);
}

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

TEST_F(PaymentsDataManagerTest, SaveCardLocallyIfNewWithNewCard) {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Sunraku Emul",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");

  EXPECT_EQ(0U, payments_data_manager().GetCreditCards().size());

  // Add the credit card to the database.
  bool is_saved = payments_data_manager().SaveCardLocallyIfNew(credit_card);
  WaitForOnPaymentsDataChanged();

  // Expect that the credit card was saved.
  EXPECT_TRUE(is_saved);
  std::vector<CreditCard> saved_credit_cards;
  for (auto* result : payments_data_manager().GetCreditCards()) {
    saved_credit_cards.push_back(*result);
  }

  EXPECT_THAT(saved_credit_cards, testing::ElementsAre(credit_card));
}

TEST_F(PaymentsDataManagerTest, SaveCardLocallyIfNewWithExistingCard) {
  const char* credit_card_number = "4111 1111 1111 1111" /* Visa */;
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Sunraku Emul", credit_card_number,
                          "01", "2999", "");

  // Add the credit card to the database.
  payments_data_manager().AddCreditCard(credit_card);
  WaitForOnPaymentsDataChanged();
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // Create a new credit card with the same card number but different detailed
  // information.
  CreditCard similar_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), kSettingsOrigin);
  test::SetCreditCardInfo(&similar_credit_card, "Sunraku Emul",
                          credit_card_number, "02", "3999",
                          "Different billing address");
  // Try to add the similar credit card to the database.
  bool is_saved =
      payments_data_manager().SaveCardLocallyIfNew(similar_credit_card);

  // Expect that the saved credit card was not updated.
  EXPECT_FALSE(is_saved);
  std::vector<CreditCard> saved_credit_cards;
  for (auto* result : payments_data_manager().GetCreditCards()) {
    saved_credit_cards.push_back(*result);
  }

  EXPECT_THAT(saved_credit_cards, testing::ElementsAre(credit_card));
}

TEST_F(PaymentsDataManagerTest, GetAccountInfoForPaymentsServer) {
  // Make the IdentityManager return a non-empty AccountInfo when
  // GetPrimaryAccountInfo() is called.
  std::string sync_account_email =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
          .email;
  ASSERT_FALSE(sync_account_email.empty());

  // Make the sync service returns consistent AccountInfo when GetAccountInfo()
  // is called.
  ASSERT_EQ(sync_service_.GetAccountInfo().email, sync_account_email);

  // The Active Sync AccountInfo should be returned.
  EXPECT_EQ(sync_account_email,
            payments_data_manager().GetAccountInfoForPaymentsServer().email);
}

TEST_F(PaymentsDataManagerTest, OnAccountsCookieDeletedByUserAction) {
  // Set up some sync transport opt-ins in the prefs.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), CoreAccountId::FromGaiaId("account1"), true);
  EXPECT_FALSE(prefs_->GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Simulate that the cookies get cleared by the user.
  payments_data_manager().OnAccountsCookieDeletedByUserAction();

  // Make sure the pref is now empty.
  EXPECT_TRUE(prefs_->GetDict(prefs::kAutofillSyncTransportOptIn).empty());
}

}  // namespace
}  // namespace autofill
