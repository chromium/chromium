// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/local_card_migration_manager.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/test_local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_manager.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/test_sync_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using testing::_;

namespace autofill {

class LocalCardMigrationManagerTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_.SetPrefService(autofill_client_.GetPrefs());
    personal_data_.SetSyncServiceForTest(&sync_service_);
    autofill_driver_.reset(new TestAutofillDriver());
    payments_client_ = new payments::TestPaymentsClient(
        autofill_driver_->GetURLLoaderFactory(),
        autofill_client_.GetIdentityManager(), &personal_data_);
    autofill_client_.set_test_payments_client(
        std::unique_ptr<payments::TestPaymentsClient>(payments_client_));
    credit_card_save_manager_ =
        new TestCreditCardSaveManager(autofill_driver_.get(), &autofill_client_,
                                      payments_client_, &personal_data_);
    credit_card_save_manager_->SetCreditCardUploadEnabled(true);
    local_card_migration_manager_ = new TestLocalCardMigrationManager(
        autofill_driver_.get(), &autofill_client_, payments_client_,
        &personal_data_);
    std::unique_ptr<TestStrikeDatabase> test_strike_database =
        std::make_unique<TestStrikeDatabase>();
    strike_database_ = test_strike_database.get();
    autofill_client_.set_test_strike_database(std::move(test_strike_database));
    autofill::TestFormDataImporter* test_form_data_importer =
        new TestFormDataImporter(
            &autofill_client_, payments_client_,
            std::unique_ptr<CreditCardSaveManager>(credit_card_save_manager_),
            &personal_data_, "en-US",
            std::unique_ptr<LocalCardMigrationManager>(
                local_card_migration_manager_));
    autofill_client_.set_test_form_data_importer(
        std::unique_ptr<TestFormDataImporter>(test_form_data_importer));
    autofill_manager_.reset(new TestAutofillManager(
        autofill_driver_.get(), &autofill_client_, &personal_data_,
        &autocomplete_history_manager_));
    autofill_manager_->SetExpectedObservedSubmission(true);
  }

  void TearDown() override {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_manager_.reset();
    autofill_driver_.reset();

    personal_data_.SetPrefService(nullptr);
    personal_data_.ClearCreditCards();
  }

  void FormsSeen(const std::vector<FormData>& forms) {
    autofill_manager_->OnFormsSeen(forms, base::TimeTicks());
  }

  void FormSubmitted(const FormData& form) {
    autofill_manager_->OnFormSubmitted(
        form, false, mojom::SubmissionSource::FORM_SUBMISSION);
  }

  void EditCreditCardFrom(FormData& credit_card_form,
                          const char* name_on_card,
                          const char* card_number,
                          const char* expiration_month,
                          const char* expiration_year,
                          const char* cvc) {
    DCHECK(credit_card_form.fields.size() >= 5);
    credit_card_form.fields[0].value = ASCIIToUTF16(name_on_card);
    credit_card_form.fields[1].value = ASCIIToUTF16(card_number);
    credit_card_form.fields[2].value = ASCIIToUTF16(expiration_month);
    credit_card_form.fields[3].value = ASCIIToUTF16(expiration_year);
    credit_card_form.fields[4].value = ASCIIToUTF16(cvc);
  }

  void AddLocalCreditCard(TestPersonalDataManager& personal_data,
                          const char* name_on_card,
                          const char* card_number,
                          const char* expiration_month,
                          const char* expiration_year,
                          const std::string& billing_address_id,
                          const std::string& guid) {
    CreditCard local_card;
    test::SetCreditCardInfo(&local_card, name_on_card, card_number,
                            expiration_month, expiration_year,
                            billing_address_id);
    local_card.set_record_type(CreditCard::LOCAL_CARD);
    local_card.set_guid(guid);
    personal_data.AddCreditCard(local_card);
  }

  // Set the parsed response |result| for the provided |guid|.
  void SetUpMigrationResponseForGuid(const std::string& guid,
                                     const std::string& result) {
    std::unique_ptr<std::unordered_map<std::string, std::string>> save_result =
        std::make_unique<std::unordered_map<std::string, std::string>>();
    save_result->insert(std::make_pair(guid, result));
    payments_client_->SetSaveResultForCardsMigration(std::move(save_result));
  }

  // Verify that the correct histogram entry (and only that) was logged.
  void ExpectUniqueLocalCardMigrationDecision(
      const base::HistogramTester& histogram_tester,
      AutofillMetrics::LocalCardMigrationDecisionMetric metric) {
    histogram_tester.ExpectUniqueSample("Autofill.LocalCardMigrationDecision",
                                        metric, 1);
  }

  void UseNewCardWithLocalCardsOnFile() {
    // Set the billing_customer_number to designate existence of a Payments
    // account.
    personal_data_.SetPaymentsCustomerData(
        std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

    // Add a local credit card (but it will not match what we will enter below).
    AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                       test::NextYear().c_str(), "1", "guid1");
    // Add another local credit card (but it will not match what we will enter
    // below).
    AddLocalCreditCard(personal_data_, "Flo Master", "4444333322221111", "11",
                       test::NextYear().c_str(), "1", "guid2");

    // Set up our credit card form data.
    FormData credit_card_form;
    test::CreateTestCreditCardFormData(&credit_card_form, true, false);
    FormsSeen(std::vector<FormData>(1, credit_card_form));

    // Edit the data, and submit.
    EditCreditCardFrom(credit_card_form, "Flo Master", "5555555555554444", "11",
                       test::NextYear().c_str(), "123");
    FormSubmitted(credit_card_form);
  }

  void UseLocalCardWithOtherLocalCardsOnFile() {
    // Set the billing_customer_number to designate existence of a Payments
    // account.
    personal_data_.SetPaymentsCustomerData(
        std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

    // Add a local credit card whose |TypeAndLastFourDigits| matches what we
    // will enter below.
    AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                       test::NextYear().c_str(), "1", "guid1");
    // Add another local credit card.
    AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                       test::NextYear().c_str(), "1", "guid2");

    // Set up our credit card form data.
    FormData credit_card_form;
    test::CreateTestCreditCardFormData(&credit_card_form, true, false);
    FormsSeen(std::vector<FormData>(1, credit_card_form));

    // Edit the data, and submit.
    EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                       test::NextYear().c_str(), "123");
    FormSubmitted(credit_card_form);
  }

  void UseLocalCardWithInvalidLocalCardsOnFile() {
    // Set the billing_customer_number to designate existence of a Payments
    // account.
    personal_data_.SetPaymentsCustomerData(
        std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

    // Add a local credit card whose |TypeAndLastFourDigits| matches what we
    // will enter below.
    AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                       test::NextYear().c_str(), "1", "guid1");
    // Add other invalid local credit cards (invalid card number or expired), so
    // it will not trigger migration.
    AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111112", "11",
                       test::NextYear().c_str(), "1", "guid2");
    AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                       test::LastYear().c_str(), "1", "guid3");

    // Set up our credit card form data.
    FormData credit_card_form;
    test::CreateTestCreditCardFormData(&credit_card_form, true, false);
    FormsSeen(std::vector<FormData>(1, credit_card_form));

    // Edit the data, and submit.
    EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                       test::NextYear().c_str(), "123");
    FormSubmitted(credit_card_form);
  }

  void UseServerCardWithOtherValidLocalCardsOnFile() {
    // Set the billing_customer_number to designate existence of a Payments
    // account.
    personal_data_.SetPaymentsCustomerData(
        std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

    // Add a masked server credit card whose |TypeAndLastFourDigits| matches
    // what we will enter below.
    CreditCard credit_card(CreditCard::MASKED_SERVER_CARD,
                           /*server_id=*/"a123");
    test::SetCreditCardInfo(&credit_card, "Flo Master", "1111", "11",
                            test::NextYear().c_str(), "1");
    credit_card.SetNetworkForMaskedCard(kVisaCard);
    personal_data_.AddServerCreditCard(credit_card);
    // Add one valid local credit card, so it will trigger migration
    AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                       test::NextYear().c_str(), "1", "guid1");

    // Set up our credit card form data.
    FormData credit_card_form;
    test::CreateTestCreditCardFormData(&credit_card_form, true, false);
    FormsSeen(std::vector<FormData>(1, credit_card_form));

    // Edit the data, and submit.
    EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                       test::NextYear().c_str(), "123");
    FormSubmitted(credit_card_form);
  }

  void UseServerCardWithInvalidLocalCardsOnFile() {
    // Set the billing_customer_number to designate existence of a Payments
    // account.
    personal_data_.SetPaymentsCustomerData(
        std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

    // Add a masked credit card whose |TypeAndLastFourDigits| matches what we
    // will enter below.
    CreditCard credit_card(CreditCard::MASKED_SERVER_CARD,
                           /*server_id=*/"a123");
    test::SetCreditCardInfo(&credit_card, "Flo Master", "1111", "11",
                            test::NextYear().c_str(), "1");
    credit_card.SetNetworkForMaskedCard(kVisaCard);
    personal_data_.AddServerCreditCard(credit_card);
    // Add other invalid local credit cards (invalid card number or expired), so
    // it will not trigger migration.
    AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111112", "11",
                       test::NextYear().c_str(), "1", "guid1");
    AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                       test::LastYear().c_str(), "1", "guid2");

    // Set up our credit card form data.
    FormData credit_card_form;
    test::CreateTestCreditCardFormData(&credit_card_form, true, false);
    FormsSeen(std::vector<FormData>(1, credit_card_form));

    // Edit the data, and submit.
    EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                       test::NextYear().c_str(), "123");
    FormSubmitted(credit_card_form);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  std::unique_ptr<TestAutofillManager> autofill_manager_;
  TestPersonalDataManager personal_data_;
  MockAutocompleteHistoryManager autocomplete_history_manager_;
  syncer::TestSyncService sync_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  // Ends up getting owned (and destroyed) by TestAutofillClient:
  TestStrikeDatabase* strike_database_;
  // Ends up getting owned (and destroyed) by TestFormDataImporter:
  TestCreditCardSaveManager* credit_card_save_manager_;
  // Ends up getting owned (and destroyed) by TestFormDataImporter:
  TestLocalCardMigrationManager* local_card_migration_manager_;
  // Ends up getting owned (and destroyed) by TestAutofillClient:
  payments::TestPaymentsClient* payments_client_;
};

// Having one local card on file and using it will not trigger migration.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_UseLocalCardWithOneLocal) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a local credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");

  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(local_card_migration_manager_->LocalCardMigrationWasTriggered());
}

// Having any number of local cards on file and using a new card will not
// trigger migration.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_UseNewCardWithAnyLocal) {
  UseNewCardWithLocalCardsOnFile();

  EXPECT_FALSE(local_card_migration_manager_->LocalCardMigrationWasTriggered());
}

// Use one local card with more valid local cards available, will trigger
// migration.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_UseLocalCardWithMoreLocal) {
  // Use one local card with more valid local cards available.
  UseLocalCardWithOtherLocalCardsOnFile();

  EXPECT_TRUE(local_card_migration_manager_->LocalCardMigrationWasTriggered());
}

// Using a local card will not trigger migration even if there are other local
// cards as long as the other local cards are not eligible for migration.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_UseLocalCardWithInvalidLocal) {
  UseLocalCardWithInvalidLocalCardsOnFile();

  EXPECT_FALSE(local_card_migration_manager_->LocalCardMigrationWasTriggered());
}

// Using a server card when any number of local cards are eligible for migration
// will trigger migration.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_UseServerCardWithOneValidLocal) {
  // Use one server card with more valid local cards available.
  UseServerCardWithOtherValidLocalCardsOnFile();

  EXPECT_TRUE(local_card_migration_manager_->LocalCardMigrationWasTriggered());
}

// Using a server card will not trigger migration even if there are other local
// cards as long as the other local cards are not eligible for migration.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_UseServerCardWithNoneValidLocal) {
  UseServerCardWithInvalidLocalCardsOnFile();

  EXPECT_FALSE(local_card_migration_manager_->LocalCardMigrationWasTriggered());
}

// Trigger migration if user only signs in.
TEST_F(LocalCardMigrationManagerTest, MigrateCreditCard_SignInOnly) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillEnableAccountWalletStorage);

  // Mock Chrome Sync is disabled.
  local_card_migration_manager_->ResetSyncState(
      AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled);

  // Use one local card with more valid local cards available.
  UseLocalCardWithOtherLocalCardsOnFile();

  EXPECT_TRUE(local_card_migration_manager_->LocalCardMigrationWasTriggered());
}

// Use one local card with more valid local cards available but billing customer
// number is blank, will not trigger migration.
TEST_F(LocalCardMigrationManagerTest, MigrateCreditCard_NoPaymentsAccount) {
  // Add a local credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");
  // Add another local credit card.
  AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid2");

  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(local_card_migration_manager_->LocalCardMigrationWasTriggered());
}

// Tests that local cards that match masked server cards do not count as
// migratable.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_LocalCardMatchMaskedServerCard) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a masked server card whose |TypeAndLastFourDigits| matches a local
  // card.
  CreditCard server_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&server_card, "Flo Master", "1111", "11",
                          test::NextYear().c_str(), "1");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_.AddServerCreditCard(server_card);
  // Add a local card whose |TypeAndLastFourDigits| matches a masked server
  // card.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");
  // Add another local credit card
  AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid2");

  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(local_card_migration_manager_->LocalCardMigrationWasTriggered());
}

// Tests that local cards that match full server cards do not count as
// migratable.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_LocalCardMatchFullServerCard) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a full server card whose number matches a local card.
  CreditCard server_card(CreditCard::FULL_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&server_card, "Flo Master", "4111111111111111", "11",
                          test::NextYear().c_str(), "1");
  personal_data_.AddServerCreditCard(server_card);
  // Add a local credit card whose number matches a full server card.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");
  // Add another local credit card
  AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid2");

  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(local_card_migration_manager_->LocalCardMigrationWasTriggered());
}

// GetDetectedValues() should includes cardholder name if all cards have it.
TEST_F(LocalCardMigrationManagerTest, GetDetectedValues_AllWithCardHolderName) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a local credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");
  // Add another local credit card with a different cardholder name.
  AddLocalCreditCard(personal_data_, "John Smith", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid2");

  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(local_card_migration_manager_->LocalCardMigrationWasTriggered());
  EXPECT_TRUE(local_card_migration_manager_->GetDetectedValues() &
              CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME);
}

// GetDetectedValues() should not include cardholder name if not all cards have
// a cardholder name.
TEST_F(LocalCardMigrationManagerTest,
       GetDetectedValues_OneCardWithoutCardHolderName) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a local credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");
  // Add another local credit card without card holder name.
  AddLocalCreditCard(personal_data_, "", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid2");
  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(local_card_migration_manager_->LocalCardMigrationWasTriggered());
  EXPECT_FALSE(local_card_migration_manager_->GetDetectedValues() &
               CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME);
}

// GetDetectedValues() should include the existence of a Google Payments
// account.
TEST_F(LocalCardMigrationManagerTest,
       GetDetectedValues_IncludeGooglePaymentsAccount) {
  // Use one local card with more valid local cards available.
  UseLocalCardWithOtherLocalCardsOnFile();

  EXPECT_TRUE(
      local_card_migration_manager_->GetDetectedValues() &
      CreditCardSaveManager::DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT);
}

TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_ShouldAddMigrateCardsBillableServiceNumberInRequest) {
  // Use one local card with more valid local cards available.
  UseLocalCardWithOtherLocalCardsOnFile();

  // Confirm that the preflight request contained
  // kMigrateCardsBillableServiceNumber in the request.
  EXPECT_EQ(payments::kMigrateCardsBillableServiceNumber,
            payments_client_->billable_service_number_in_request());
}

TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_ShouldAddUploadCardSourceInRequest_CheckoutFlow) {
  // Use one local card with more valid local cards available.
  UseLocalCardWithOtherLocalCardsOnFile();

  // Confirm that the preflight request contained the correct UploadCardSource.
  EXPECT_EQ(payments::PaymentsClient::UploadCardSource::
                LOCAL_CARD_MIGRATION_CHECKOUT_FLOW,
            payments_client_->upload_card_source_in_request());
}

TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_ShouldAddUploadCardSourceInRequest_SettingsPage) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a local credit card. One migratable credit card will still trigger
  // migration on settings page.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");

  // Do the same operation as we bridge back from the settings page.
  local_card_migration_manager_->GetMigratableCreditCards();
  local_card_migration_manager_->AttemptToOfferLocalCardMigration(true);

  EXPECT_FALSE(local_card_migration_manager_->IntermediatePromptWasShown());
  EXPECT_TRUE(local_card_migration_manager_->MainPromptWasShown());

  // Confirm that the preflight request contained the correct UploadCardSource.
  EXPECT_EQ(payments::PaymentsClient::UploadCardSource::
                LOCAL_CARD_MIGRATION_SETTINGS_PAGE,
            payments_client_->upload_card_source_in_request());
}

// Verify that when triggering from settings page, intermediate prompt will not
// be triggered.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_TriggerFromSettingsPage) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a local credit card. One migratable credit card will still trigger
  // migration on settings page.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");

  // Do the same operation as we bridge back from the settings page.
  local_card_migration_manager_->GetMigratableCreditCards();
  local_card_migration_manager_->AttemptToOfferLocalCardMigration(true);

  EXPECT_FALSE(local_card_migration_manager_->IntermediatePromptWasShown());
  EXPECT_TRUE(local_card_migration_manager_->MainPromptWasShown());
}

// Verify that when triggering from submitted form, intermediate prompt and main
// prompt are both triggered.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_TriggerFromSubmittedForm) {
  // Use one local card with more valid local cards available.
  UseLocalCardWithOtherLocalCardsOnFile();

  EXPECT_TRUE(local_card_migration_manager_->IntermediatePromptWasShown());
  EXPECT_TRUE(local_card_migration_manager_->MainPromptWasShown());
}

// Verify that given the parsed response from the payments client, the migration
// status is correctly set.
TEST_F(LocalCardMigrationManagerTest, MigrateCreditCard_MigrationSuccess) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a local credit card for migration.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");

  // Verify that it exists in the local database.
  EXPECT_TRUE(personal_data_.GetCreditCardByNumber("4111111111111111"));

  // Get the migratable credit cards.
  local_card_migration_manager_->GetMigratableCreditCards();

  // Set the parsed response to success.
  SetUpMigrationResponseForGuid(
      local_card_migration_manager_->migratable_credit_cards_[0]
          .credit_card()
          .guid(),
      autofill::kMigrationResultSuccess);

  EXPECT_EQ(local_card_migration_manager_->migratable_credit_cards_[0]
                .migration_status(),
            autofill::MigratableCreditCard::MigrationStatus::UNKNOWN);

  local_card_migration_manager_->AttemptToOfferLocalCardMigration(true);

  EXPECT_EQ(local_card_migration_manager_->migratable_credit_cards_[0]
                .migration_status(),
            autofill::MigratableCreditCard::MigrationStatus::SUCCESS_ON_UPLOAD);

  // Local card should *not* be present as it is migrated already.
  EXPECT_FALSE(personal_data_.GetCreditCardByNumber("4111111111111111"));
}

// Verify that given the parsed response from the payments client, the migration
// status is correctly set.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_MigrationTemporaryFailure) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a local credit card. One migratable credit card will still trigger
  // migration on settings page.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");

  // Verify that it exists in local database.
  EXPECT_TRUE(personal_data_.GetCreditCardByNumber("4111111111111111"));

  // Get the migratable credit cards.
  local_card_migration_manager_->GetMigratableCreditCards();

  // Set the parsed response to temporary failure.
  SetUpMigrationResponseForGuid(
      local_card_migration_manager_->migratable_credit_cards_[0]
          .credit_card()
          .guid(),
      autofill::kMigrationResultTemporaryFailure);

  EXPECT_EQ(local_card_migration_manager_->migratable_credit_cards_[0]
                .migration_status(),
            autofill::MigratableCreditCard::MigrationStatus::UNKNOWN);

  // Start the migration.
  local_card_migration_manager_->AttemptToOfferLocalCardMigration(true);

  EXPECT_EQ(local_card_migration_manager_->migratable_credit_cards_[0]
                .migration_status(),
            autofill::MigratableCreditCard::MigrationStatus::FAILURE_ON_UPLOAD);

  // Local card should be present as it is not migrated.
  EXPECT_TRUE(personal_data_.GetCreditCardByNumber("4111111111111111"));
}

// Verify that given the parsed response from the payments client, the migration
// status is correctly set.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_MigrationPermanentFailure) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a local credit card. One migratable credit card will still trigger
  // migration on settings page.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");

  // Verify that it exists in local database.
  EXPECT_TRUE(personal_data_.GetCreditCardByNumber("4111111111111111"));

  // Get the migratable credit cards.
  local_card_migration_manager_->GetMigratableCreditCards();

  // Set the parsed response to permanent failure.
  SetUpMigrationResponseForGuid(
      local_card_migration_manager_->migratable_credit_cards_[0]
          .credit_card()
          .guid(),
      autofill::kMigrationResultPermanentFailure);

  EXPECT_EQ(local_card_migration_manager_->migratable_credit_cards_[0]
                .migration_status(),
            autofill::MigratableCreditCard::MigrationStatus::UNKNOWN);

  // Start the migration.
  local_card_migration_manager_->AttemptToOfferLocalCardMigration(true);

  EXPECT_EQ(local_card_migration_manager_->migratable_credit_cards_[0]
                .migration_status(),
            autofill::MigratableCreditCard::MigrationStatus::FAILURE_ON_UPLOAD);

  // Local card should be present as it is not migrated.
  EXPECT_TRUE(personal_data_.GetCreditCardByNumber("4111111111111111"));
}

// Verify selected cards are correctly passed to manager.
TEST_F(LocalCardMigrationManagerTest, MigrateCreditCard_ToggleIsChosen) {
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");
  AddLocalCreditCard(personal_data_, "Flo Master", "5454545454545454", "11",
                     test::NextYear().c_str(), "1", "guid2");
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  local_card_migration_manager_->GetMigratableCreditCards();

  autofill_client_.set_migration_card_selections(
      std::vector<std::string>{"guid1"});
  local_card_migration_manager_->AttemptToOfferLocalCardMigration(true);

  EXPECT_EQ(static_cast<int>(
                local_card_migration_manager_->migratable_credit_cards_.size()),
            1);
  EXPECT_EQ(local_card_migration_manager_->migratable_credit_cards_[0]
                .credit_card()
                .guid(),
            "guid1");
}

TEST_F(LocalCardMigrationManagerTest, DeleteLocalCardViaMigrationDialog) {
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");

  EXPECT_TRUE(personal_data_.GetCreditCardWithGUID("guid1"));

  local_card_migration_manager_->OnUserDeletedLocalCardViaMigrationDialog(
      "guid1");

  EXPECT_FALSE(personal_data_.GetCreditCardWithGUID("guid1"));
}

// Use one local card with more valid local cards available, don't show prompt
// if max strikes reached.
TEST_F(LocalCardMigrationManagerTest,
       MigrateLocalCreditCard_MaxStrikesReached) {
  LocalCardMigrationStrikeDatabase local_card_migration_strike_database =
      LocalCardMigrationStrikeDatabase(strike_database_);
  local_card_migration_strike_database.AddStrikes(
      local_card_migration_strike_database.GetMaxStrikesLimit());

  EXPECT_EQ(local_card_migration_strike_database.GetStrikes(),
            local_card_migration_strike_database.GetMaxStrikesLimit());

  base::HistogramTester histogram_tester;
  // Use one local card with more valid local cards available.
  UseLocalCardWithOtherLocalCardsOnFile();

  // Local card migration not triggered since max strikes have been reached.
  EXPECT_FALSE(local_card_migration_manager_->LocalCardMigrationWasTriggered());

  // Verify that the correct histogram entry was logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.LocalCardMigrationNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::LOCAL, 1);
}

// Use one server card with more valid local cards available, don't show prompt
// if max strikes reached.
TEST_F(LocalCardMigrationManagerTest,
       MigrateServerCreditCard_MaxStrikesReached) {
  LocalCardMigrationStrikeDatabase local_card_migration_strike_database =
      LocalCardMigrationStrikeDatabase(strike_database_);
  local_card_migration_strike_database.AddStrikes(
      local_card_migration_strike_database.GetMaxStrikesLimit());

  EXPECT_EQ(local_card_migration_strike_database.GetStrikes(),
            local_card_migration_strike_database.GetMaxStrikesLimit());

  base::HistogramTester histogram_tester;
  // Use one server card with more valid local cards available.
  UseServerCardWithOtherValidLocalCardsOnFile();

  // Local card migration not triggered since max strikes have been reached.
  EXPECT_FALSE(local_card_migration_manager_->LocalCardMigrationWasTriggered());

  // Verify that the correct histogram entry was logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.LocalCardMigrationNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::SERVER, 1);
}

// When local card migration is accepted, UMA metrics for LocalCardMigration
// strike count is logged.
TEST_F(LocalCardMigrationManagerTest, MigrateCreditCard_StrikeCountUMALogged) {
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");
  AddLocalCreditCard(personal_data_, "Flo Master", "5454545454545454", "11",
                     test::NextYear().c_str(), "1", "guid2");
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));
  local_card_migration_manager_->GetMigratableCreditCards();

  // Add 4 LocalCardMigration strikes.
  LocalCardMigrationStrikeDatabase local_card_migration_strike_database =
      LocalCardMigrationStrikeDatabase(strike_database_);
  local_card_migration_strike_database.AddStrikes(4);
  EXPECT_EQ(local_card_migration_strike_database.GetStrikes(), 4);

  base::HistogramTester histogram_tester;

  // Select the cards.
  autofill_client_.set_migration_card_selections(
      std::vector<std::string>{"guid1", "guid2"});
  local_card_migration_manager_->AttemptToOfferLocalCardMigration(true);

  // Verify that the strike count was logged when card migration accepted.
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.StrikesPresentWhenLocalCardMigrationAccepted", 4,
      1);
}

// Use one unsupported local card with more supported local cards will not
// show intermediate prompt.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_MigrationAbortWhenUseUnsupportedLocalCard) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a local credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");
  // Add another local credit card.
  AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid2");

  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Set up the supported card bin ranges so that the used local card is not
  // supported but the one left is supported.
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(300, 305), std::make_pair(555, 555)};
  payments_client_->SetSupportedBINRanges(supported_card_bin_ranges);

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(local_card_migration_manager_->IntermediatePromptWasShown());
}

// Use one supported local card with more unsupported local cards available
// will show intermediate prompt with the only supported local card.
TEST_F(LocalCardMigrationManagerTest,
       MigrateCreditCard_MigrateWhenHasSupportedLocalCard) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a local credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");
  // Add another local credit card.
  AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid2");

  // Set up the supported card bin ranges so that the used local card is the
  // only supported card.
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(300, 305), std::make_pair(411, 412)};
  payments_client_->SetSupportedBINRanges(supported_card_bin_ranges);

  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);

  EXPECT_EQ(static_cast<int>(
                local_card_migration_manager_->migratable_credit_cards_.size()),
            1);
  EXPECT_EQ(local_card_migration_manager_->migratable_credit_cards_[0]
                .credit_card()
                .number(),
            base::ASCIIToUTF16("4111111111111111"));
  EXPECT_TRUE(local_card_migration_manager_->IntermediatePromptWasShown());
}

// Use one unsupported server card with more supported local cards available
// will still show intermediate prompt.
TEST_F(
    LocalCardMigrationManagerTest,
    MigrateCreditCard_MigrateWhenUseUnsupportedServerCardWithSupportedLocalCard) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a masked server credit card whose |TypeAndLastFourDigits| matches what
  // we will enter below.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Flo Master", "1111", "11",
                          test::NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_.AddServerCreditCard(credit_card);
  // Add one valid local credit card, so it will trigger migration
  AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid1");

  // Set up the supported card bin ranges so that the used server card is
  // unsupported but the one left is supported.
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(300, 305), std::make_pair(555, 555)};
  payments_client_->SetSupportedBINRanges(supported_card_bin_ranges);

  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);

  EXPECT_TRUE(local_card_migration_manager_->IntermediatePromptWasShown());
}

// Use one supported server card with more unsupported local cards will not show
// intermediate prompt.
TEST_F(
    LocalCardMigrationManagerTest,
    MigrateCreditCard_MigrateAbortWhenUseSupportedServerCardWithUnsupportedLocalCard) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a masked server credit card whose |TypeAndLastFourDigits| matches what
  // we will enter below.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Flo Master", "1111", "11",
                          test::NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_.AddServerCreditCard(credit_card);
  // Add one valid local credit card, so it will trigger migration
  AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid1");

  // Set up the supported card bin ranges so that the used server card is
  // supported while the one left is unsupported.
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(300, 305), std::make_pair(411, 411)};
  payments_client_->SetSupportedBINRanges(supported_card_bin_ranges);

  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);

  EXPECT_FALSE(local_card_migration_manager_->IntermediatePromptWasShown());
}

// All migration requirements are met but GetUploadDetails rpc fails. Verified
// that the intermediate prompt was not shown.
TEST_F(LocalCardMigrationManagerTest, MigrateCreditCard_GetUploadDetailsFails) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  local_card_migration_manager_->SetAppLocaleForTesting("pt-BR");

  UseLocalCardWithOtherLocalCardsOnFile();

  EXPECT_FALSE(local_card_migration_manager_->IntermediatePromptWasShown());
}

// Use one local card with more valid local cards available, will log to
// UseOfLocalCard sub-histogram.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationOrigin_UseLocalCardWithMoreLocal) {
  base::HistogramTester histogram_tester;
  // Use one local card with more valid local cards available.
  UseLocalCardWithOtherLocalCardsOnFile();

  // Verify that metrics are correctly logged to the UseOfLocalCard
  // sub-histogram.
  histogram_tester.ExpectBucketCount(
      "Autofill.LocalCardMigrationOrigin.UseOfLocalCard",
      AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LocalCardMigrationOrigin.UseOfLocalCard",
      AutofillMetrics::INTERMEDIATE_BUBBLE_ACCEPTED, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LocalCardMigrationOrigin.UseOfLocalCard",
      AutofillMetrics::MAIN_DIALOG_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LocalCardMigrationOrigin.UseOfLocalCard",
      AutofillMetrics::MAIN_DIALOG_ACCEPTED, 1);
}

// Using a server card when any number of local cards are eligible for migration
// will log to UseOfServerCard sub-histogram.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationOrigin_UseServerCardWithOneValidLocal) {
  base::HistogramTester histogram_tester;
  // Use one server card with more valid local cards available.
  UseServerCardWithOtherValidLocalCardsOnFile();

  // Verify that metrics are correctly logged to the UseOfServerCard
  // sub-histogram.
  histogram_tester.ExpectBucketCount(
      "Autofill.LocalCardMigrationOrigin.UseOfServerCard",
      AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LocalCardMigrationOrigin.UseOfServerCard",
      AutofillMetrics::INTERMEDIATE_BUBBLE_ACCEPTED, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LocalCardMigrationOrigin.UseOfServerCard",
      AutofillMetrics::MAIN_DIALOG_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LocalCardMigrationOrigin.UseOfServerCard",
      AutofillMetrics::MAIN_DIALOG_ACCEPTED, 1);
}

// Using a server card will not trigger migration even if there are other local
// cards as long as the other local cards are not eligible for migration. Verify
// that it will not log to UseOfServerCard sub-histogram.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationOrigin_UseServerCardWithNoneValidLocal) {
  base::HistogramTester histogram_tester;
  UseServerCardWithInvalidLocalCardsOnFile();

  // Verify that metrics are correctly logged to the UseOfServerCard
  // sub-histogram.
  histogram_tester.ExpectTotalCount(
      "Autofill.LocalCardMigrationOrigin.UseOfServerCard", 0);
}

// Verify that triggering from settings page will log to SettingsPage
// sub-histogram.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationOrigin_TriggerFromSettingsPage) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a local credit card. One migratable credit card will still trigger
  // migration on settings page.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");

  base::HistogramTester histogram_tester;
  // Do the same operation as we bridge back from the settings page.
  local_card_migration_manager_->GetMigratableCreditCards();
  local_card_migration_manager_->AttemptToOfferLocalCardMigration(true);

  // Verify that metrics are correctly logged to the SettingsPage sub-histogram.
  // Triggering from settings page won't show intermediate bubble.
  histogram_tester.ExpectBucketCount(
      "Autofill.LocalCardMigrationOrigin.SettingsPage",
      AutofillMetrics::INTERMEDIATE_BUBBLE_SHOWN, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.LocalCardMigrationOrigin.SettingsPage",
      AutofillMetrics::INTERMEDIATE_BUBBLE_ACCEPTED, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.LocalCardMigrationOrigin.SettingsPage",
      AutofillMetrics::MAIN_DIALOG_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.LocalCardMigrationOrigin.SettingsPage",
      AutofillMetrics::MAIN_DIALOG_ACCEPTED, 1);
}

// Use new card when submit so migration was not offered. Verify the migration
// decision metric is logged as new card used.
TEST_F(LocalCardMigrationManagerTest, LogMigrationDecisionMetric_UseNewCard) {
  base::HistogramTester histogram_tester;
  UseNewCardWithLocalCardsOnFile();

  ExpectUniqueLocalCardMigrationDecision(
      histogram_tester, AutofillMetrics::LocalCardMigrationDecisionMetric::
                            NOT_OFFERED_USE_NEW_CARD);
}

// Use one local card with more valid local cards available but billing customer
// number is blank, will not trigger migration. Verify the migration decision
// metric is logged as failed enablement prerequisites.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationDecisionMetric_FailedEnablementPrerequisites) {
  base::HistogramTester histogram_tester;
  // Add a local credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");
  // Add another local credit card.
  AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid2");

  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);

  ExpectUniqueLocalCardMigrationDecision(
      histogram_tester, AutofillMetrics::LocalCardMigrationDecisionMetric::
                            NOT_OFFERED_FAILED_PREREQUISITES);
}

// All migration requirements are met but max strikes reached. Verify the
// migration decision metric is logged as max strikes reached.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationDecisionMetric_MaxStrikesReached) {
  LocalCardMigrationStrikeDatabase local_card_migration_strike_database =
      LocalCardMigrationStrikeDatabase(strike_database_);
  local_card_migration_strike_database.AddStrikes(
      local_card_migration_strike_database.GetMaxStrikesLimit());

  EXPECT_EQ(local_card_migration_strike_database.GetStrikes(),
            local_card_migration_strike_database.GetMaxStrikesLimit());

  base::HistogramTester histogram_tester;
  UseLocalCardWithOtherLocalCardsOnFile();

  ExpectUniqueLocalCardMigrationDecision(
      histogram_tester, AutofillMetrics::LocalCardMigrationDecisionMetric::
                            NOT_OFFERED_REACHED_MAX_STRIKE_COUNT);
}

// Use one local card with invalid local card so migration was not offered.
// Verify the migration decision metric is logged as not offered single local
// card.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationDecisionMetric_NotOfferedSingleLocalCard) {
  base::HistogramTester histogram_tester;
  UseLocalCardWithInvalidLocalCardsOnFile();

  ExpectUniqueLocalCardMigrationDecision(
      histogram_tester, AutofillMetrics::LocalCardMigrationDecisionMetric::
                            NOT_OFFERED_SINGLE_LOCAL_CARD);
}

// Use one server card with invalid local card so migration was not offered.
// Verify the migration decision metric is logged as no migratable cards.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationDecisionMetric_NoMigratableCards) {
  base::HistogramTester histogram_tester;
  UseServerCardWithInvalidLocalCardsOnFile();

  ExpectUniqueLocalCardMigrationDecision(
      histogram_tester, AutofillMetrics::LocalCardMigrationDecisionMetric::
                            NOT_OFFERED_NO_MIGRATABLE_CARDS);
}

// All migration requirements are met but GetUploadDetails rpc fails. Verify the
// migration decision metric is logged as get upload details failed.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationDecisionMetric_GetUploadDetailsFails) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  local_card_migration_manager_->SetAppLocaleForTesting("pt-BR");

  base::HistogramTester histogram_tester;
  UseLocalCardWithOtherLocalCardsOnFile();

  ExpectUniqueLocalCardMigrationDecision(
      histogram_tester, AutofillMetrics::LocalCardMigrationDecisionMetric::
                            NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
}

// Use one unsupported local card with more supported local cards will not
// show intermediate prompt. Verify the migration decision metric is logged as
// use unsupported local card.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationDecisionMetric_UseUnsupportedLocalCard) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a local credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  AddLocalCreditCard(personal_data_, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "1", "guid1");
  // Add another local credit card.
  AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid2");

  base::HistogramTester histogram_tester;
  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Set up the supported card bin ranges so that the used local card is not
  // supported but the one left is supported.
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(300, 305), std::make_pair(555, 555)};
  payments_client_->SetSupportedBINRanges(supported_card_bin_ranges);

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);

  ExpectUniqueLocalCardMigrationDecision(
      histogram_tester, AutofillMetrics::LocalCardMigrationDecisionMetric::
                            NOT_OFFERED_USE_UNSUPPORTED_LOCAL_CARD);
}

// Use one supported server card with more unsupported local cards will not show
// intermediate prompt. Verify the migration decision metric is logged as no
// supported cards.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationDecisionMetric_NoSupportedCardsForSupportedServerCard) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a masked server credit card whose |TypeAndLastFourDigits| matches what
  // we will enter below.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Flo Master", "1111", "11",
                          test::NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_.AddServerCreditCard(credit_card);
  // Add one valid local credit card, so it will trigger migration
  AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid1");

  // Set up the supported card bin ranges so that the used server card is
  // supported while the one left is unsupported.
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(300, 305), std::make_pair(411, 411)};
  payments_client_->SetSupportedBINRanges(supported_card_bin_ranges);

  base::HistogramTester histogram_tester;
  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);

  ExpectUniqueLocalCardMigrationDecision(
      histogram_tester, AutofillMetrics::LocalCardMigrationDecisionMetric::
                            NOT_OFFERED_NO_SUPPORTED_CARDS);
}

// Use one unsupported server card with more unsupported local cards will not
// show intermediate prompt. Verify the migration decision metric is logged as
// no supported cards.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationDecisionMetric_NoSupportedCardsForUnsupportedServerCard) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Add a masked server credit card whose |TypeAndLastFourDigits| matches what
  // we will enter below.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Flo Master", "1111", "11",
                          test::NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_.AddServerCreditCard(credit_card);
  // Add one valid local credit card, so it will trigger migration
  AddLocalCreditCard(personal_data_, "Flo Master", "5555555555554444", "11",
                     test::NextYear().c_str(), "1", "guid1");

  // Set up the supported card bin ranges so that the used server card and local
  // cards are all unsupported.
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(300, 305), std::make_pair(400, 400)};
  payments_client_->SetSupportedBINRanges(supported_card_bin_ranges);

  base::HistogramTester histogram_tester;
  // Set up our credit card form data.
  FormData credit_card_form;
  test::CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  EditCreditCardFrom(credit_card_form, "Flo Master", "4111111111111111", "11",
                     test::NextYear().c_str(), "123");
  FormSubmitted(credit_card_form);

  ExpectUniqueLocalCardMigrationDecision(
      histogram_tester, AutofillMetrics::LocalCardMigrationDecisionMetric::
                            NOT_OFFERED_NO_SUPPORTED_CARDS);
}

// All migration requirements are met and migration was offered. Verify the
// migration decision metric is logged as migration offered.
TEST_F(LocalCardMigrationManagerTest,
       LogMigrationDecisionMetric_MigrationOffered) {
  base::HistogramTester histogram_tester;
  UseLocalCardWithOtherLocalCardsOnFile();

  ExpectUniqueLocalCardMigrationDecision(
      histogram_tester,
      AutofillMetrics::LocalCardMigrationDecisionMetric::OFFERED);
}

// Tests that if payment client returns an invalid legal message migration
// should not be offered.
TEST_F(LocalCardMigrationManagerTest,
       InvalidLegalMessageInOnDidGetUploadDetails) {
  payments_client_->SetUseInvalidLegalMessageInGetUploadDetails(true);

  base::HistogramTester histogram_tester;
  UseLocalCardWithOtherLocalCardsOnFile();

  // Verify that the correct histogram entries were logged.
  ExpectUniqueLocalCardMigrationDecision(
      histogram_tester, AutofillMetrics::LocalCardMigrationDecisionMetric::
                            NOT_OFFERED_INVALID_LEGAL_MESSAGE);
}

}  // namespace autofill
