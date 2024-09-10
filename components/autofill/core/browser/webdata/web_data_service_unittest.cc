// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using base::Bucket;
using base::BucketsAre;
using testing::DoDefault;
using testing::ElementsAre;
using testing::Pointee;
using testing::UnorderedElementsAre;

template <class T>
class AutofillWebDataServiceWaiter : public WebDataServiceConsumer {
 public:
  AutofillWebDataServiceWaiter() = default;

  AutofillWebDataServiceWaiter(const AutofillWebDataServiceWaiter&) = delete;
  AutofillWebDataServiceWaiter& operator=(const AutofillWebDataServiceWaiter&) =
      delete;

  T& result() {
    if (!run_loop_.AnyQuitCalled()) {
      // Wait for the result.
      run_loop_.Run();
    }
    return result_;
  }

 private:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) final {
    result_ = std::move(static_cast<WDResult<T>*>(result.get())->GetValue());
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  T result_;
};

constexpr base::TimeDelta kWebDataServiceTimeout = base::Seconds(8);

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class MockAutofillWebDataServiceObserver
    : public AutofillWebDataServiceObserverOnDBSequence {
 public:
  MOCK_METHOD(void,
              AutocompleteEntriesChanged,
              (const AutocompleteChangeList& changes),
              (override));
  MOCK_METHOD(void,
              AutofillProfileChanged,
              (const AutofillProfileChange& change),
              (override));
};

class WebDataServiceTest : public testing::Test {
 public:
  WebDataServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)) {}

 protected:
  void SetUp() override {
    wdbs_ = base::MakeRefCounted<WebDatabaseService>(
        base::FilePath(WebDatabase::kInMemoryPath),
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
    wdbs_->AddTable(std::make_unique<AddressAutofillTable>());
    wdbs_->AddTable(std::make_unique<AutocompleteTable>());
    wdbs_->AddTable(std::make_unique<PaymentsAutofillTable>());
    wdbs_->LoadDatabase(os_crypt_.get());

    wds_ = base::MakeRefCounted<AutofillWebDataService>(
        wdbs_, base::SequencedTaskRunner::GetCurrentDefault());
    wds_->Init(base::NullCallback());
  }

  void TearDown() override {
    wds_->ShutdownOnUISequence();
    wdbs_->ShutdownDatabase();
    WaitForEmptyDBSequence();
  }

  void WaitForEmptyDBSequence() {
    base::RunLoop run_loop;
    wdbs_->GetDbSequence()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                             run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  base::FilePath profile_dir_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  scoped_refptr<WebDatabaseService> wdbs_;
  scoped_refptr<AutofillWebDataService> wds_;
};

class WebDataServiceAutofillTest : public WebDataServiceTest {
 public:
  WebDataServiceAutofillTest()
      : done_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED) {}

 protected:
  void SetUp() override {
    WebDataServiceTest::SetUp();

    void (AutofillWebDataService::*add_observer_func)(
        AutofillWebDataServiceObserverOnDBSequence*) =
        &AutofillWebDataService::AddObserver;
    wds_->GetDBTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(add_observer_func, wds_, &observer_));
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

  void TearDown() override {
    void (AutofillWebDataService::*remove_observer_func)(
        AutofillWebDataServiceObserverOnDBSequence*) =
        &AutofillWebDataService::RemoveObserver;
    wds_->GetDBTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(remove_observer_func, wds_, &observer_));

    WebDataServiceTest::TearDown();
  }

  void AppendFormField(const std::u16string& name,
                       const std::u16string& value,
                       std::vector<FormFieldData>& form_fields) {
    FormFieldData field;
    field.set_name(name);
    field.set_value(value);
    form_fields.push_back(field);
  }

  testing::NiceMock<MockAutofillWebDataServiceObserver> observer_;
  base::WaitableEvent done_event_;
};

TEST_F(WebDataServiceAutofillTest, FormFillAdd) {
  // This will verify that the correct notification is triggered,
  // passing the correct list of autocomplete keys in the details.
  EXPECT_CALL(observer_,
              AutocompleteEntriesChanged(ElementsAre(
                  AutocompleteChange(AutocompleteChange::ADD,
                                     AutocompleteKey("name1", "value1")),
                  AutocompleteChange(AutocompleteChange::ADD,
                                     AutocompleteKey("name2", "value2")))))
      .WillOnce(SignalEvent(&done_event_));

  std::vector<FormFieldData> form_fields;
  AppendFormField(u"name1", u"value1", form_fields);
  AppendFormField(u"name2", u"value2", form_fields);
  wds_->AddFormFields(form_fields);
  done_event_.TimedWait(kWebDataServiceTimeout);

  AutofillWebDataServiceWaiter<std::vector<AutocompleteEntry>> consumer;
  wds_->GetFormValuesForElementName(u"name1", std::u16string(),
                                    /*limit=*/10, &consumer);
  EXPECT_THAT(consumer.result(), UnorderedElementsAre(testing::Property(
                                     &AutocompleteEntry::key,
                                     AutocompleteKey("name1", "value1"))));
}

TEST_F(WebDataServiceAutofillTest, FormFillRemoveOne) {
  // First add some values to autocomplete.
  EXPECT_CALL(observer_, AutocompleteEntriesChanged)
      .WillOnce(SignalEvent(&done_event_));
  std::vector<FormFieldData> form_fields;
  AppendFormField(u"name1", u"value1", form_fields);
  wds_->AddFormFields(form_fields);
  done_event_.TimedWait(kWebDataServiceTimeout);

  // This will verify that the correct notification is triggered,
  // passing the correct list of autocomplete keys in the details.
  EXPECT_CALL(
      observer_,
      AutocompleteEntriesChanged(ElementsAre(AutocompleteChange(
          AutocompleteChange::REMOVE, AutocompleteKey("name1", "value1")))))
      .WillOnce(SignalEvent(&done_event_));
  wds_->RemoveFormValueForElementName(u"name1", u"value1");
  done_event_.TimedWait(kWebDataServiceTimeout);
}

TEST_F(WebDataServiceAutofillTest, FormFillRemoveMany) {
  EXPECT_CALL(observer_, AutocompleteEntriesChanged)
      .WillOnce(SignalEvent(&done_event_));

  std::vector<FormFieldData> form_fields;
  AppendFormField(u"name1", u"value1", form_fields);
  AppendFormField(u"name2", u"value2", form_fields);
  wds_->AddFormFields(form_fields);
  done_event_.TimedWait(kWebDataServiceTimeout);

  // This will verify that the correct notification is triggered,
  // passing the correct list of autocomplete keys in the details.
  EXPECT_CALL(observer_,
              AutocompleteEntriesChanged(ElementsAre(
                  AutocompleteChange(AutocompleteChange::REMOVE,
                                     AutocompleteKey("name1", "value1")),
                  AutocompleteChange(AutocompleteChange::REMOVE,
                                     AutocompleteKey("name2", "value2")))))
      .WillOnce(SignalEvent(&done_event_));
  wds_->RemoveFormElementsAddedBetween(AutofillClock::Now(),
                                       AutofillClock::Now() + base::Days(1));
  done_event_.TimedWait(kWebDataServiceTimeout);
}

TEST_F(WebDataServiceAutofillTest, ProfileAdd) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);

  // Check that GUID-based notification was sent.
  EXPECT_CALL(observer_,
              AutofillProfileChanged(AutofillProfileChange(
                  AutofillProfileChange::ADD, profile.guid(), profile)))
      .WillOnce(SignalEvent(&done_event_));

  wds_->AddAutofillProfile(profile);
  done_event_.TimedWait(kWebDataServiceTimeout);

  // Check that it was added.
  AutofillWebDataServiceWaiter<std::vector<AutofillProfile>> consumer;
  wds_->GetAutofillProfiles(&consumer);
  EXPECT_THAT(consumer.result(), UnorderedElementsAre(profile));
}

TEST_F(WebDataServiceAutofillTest, ProfileRemove) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);

  // Add a profile.
  EXPECT_CALL(observer_, AutofillProfileChanged)
      .WillOnce(SignalEvent(&done_event_));
  wds_->AddAutofillProfile(profile);
  done_event_.TimedWait(kWebDataServiceTimeout);

  // Check that it was added.
  AutofillWebDataServiceWaiter<std::vector<AutofillProfile>> consumer;
  wds_->GetAutofillProfiles(&consumer);
  EXPECT_THAT(consumer.result(), UnorderedElementsAre(profile));

  // Check that GUID-based notification was sent.
  EXPECT_CALL(observer_,
              AutofillProfileChanged(AutofillProfileChange(
                  AutofillProfileChange::REMOVE, profile.guid(), profile)))
      .WillOnce(SignalEvent(&done_event_));

  // Remove the profile.
  wds_->RemoveAutofillProfile(profile.guid());
  done_event_.TimedWait(kWebDataServiceTimeout);

  // Check that it was removed.
  AutofillWebDataServiceWaiter<std::vector<AutofillProfile>> consumer2;
  wds_->GetAutofillProfiles(&consumer2);
  ASSERT_TRUE(consumer2.result().empty());
}

TEST_F(WebDataServiceAutofillTest, ProfileUpdate) {
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile1.SetRawInfo(NAME_FIRST, u"Abe");
  profile1.FinalizeAfterImport();

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile2.SetRawInfo(NAME_FIRST, u"Alice");
  profile2.FinalizeAfterImport();

  EXPECT_CALL(observer_, AutofillProfileChanged)
      .WillOnce(DoDefault())
      .WillOnce(SignalEvent(&done_event_));

  wds_->AddAutofillProfile(profile1);
  wds_->AddAutofillProfile(profile2);
  done_event_.TimedWait(kWebDataServiceTimeout);

  // Check that they were added.
  AutofillWebDataServiceWaiter<std::vector<AutofillProfile>> consumer;
  wds_->GetAutofillProfiles(&consumer);
  EXPECT_THAT(consumer.result(), UnorderedElementsAre(profile1, profile2));

  AutofillProfile profile2_changed(profile2);
  profile2_changed.SetRawInfo(NAME_FIRST, u"Bill");
  EXPECT_CALL(observer_, AutofillProfileChanged(AutofillProfileChange(
                             AutofillProfileChange::UPDATE, profile2.guid(),
                             profile2_changed)))
      .WillOnce(SignalEvent(&done_event_));

  // Update the profile.
  wds_->UpdateAutofillProfile(profile2_changed);
  done_event_.TimedWait(kWebDataServiceTimeout);

  // Check that the updates were made.
  AutofillWebDataServiceWaiter<std::vector<AutofillProfile>> consumer2;
  wds_->GetAutofillProfiles(&consumer2);
  EXPECT_THAT(consumer2.result(),
              UnorderedElementsAre(profile1, profile2_changed));
}

TEST_F(WebDataServiceAutofillTest, CreditAdd) {
  CreditCard card;
  wds_->AddCreditCard(card);

  // Check that it was added.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<CreditCard>>>
      consumer;
  wds_->GetCreditCards(&consumer);
  EXPECT_THAT(consumer.result(), UnorderedElementsAre(Pointee(card)));
}

TEST_F(WebDataServiceAutofillTest, CreditCardRemove) {
  CreditCard credit_card;

  // Add a credit card.
  wds_->AddCreditCard(credit_card);

  // Check that it was added.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<CreditCard>>>
      consumer;
  wds_->GetCreditCards(&consumer);
  EXPECT_THAT(consumer.result(), UnorderedElementsAre(Pointee(credit_card)));

  // Remove the credit card.
  wds_->RemoveCreditCard(credit_card.guid());

  // Check that it was removed.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<CreditCard>>>
      consumer2;
  wds_->GetCreditCards(&consumer2);
  ASSERT_TRUE(consumer2.result().empty());
}

TEST_F(WebDataServiceAutofillTest, CreditUpdate) {
  CreditCard card1("E4D2662E-5E16-44F3-AF5A-5A77FAE4A6F3", std::string());
  card1.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Abe");
  CreditCard card2("B9C52112-BD5F-4080-84E1-C651D2CB90E2", std::string());
  card2.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Alice");

  wds_->AddCreditCard(card1);
  wds_->AddCreditCard(card2);

  // Check that they got added.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<CreditCard>>>
      consumer;
  wds_->GetCreditCards(&consumer);
  EXPECT_THAT(consumer.result(),
              UnorderedElementsAre(Pointee(card1), Pointee(card2)));

  CreditCard card2_changed(card2);
  card2_changed.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Bill");

  wds_->UpdateCreditCard(card2_changed);

  // Check that the updates were made.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<CreditCard>>>
      consumer2;
  wds_->GetCreditCards(&consumer2);
  EXPECT_THAT(consumer2.result(),
              UnorderedElementsAre(Pointee(card1), Pointee(card2_changed)));
}

// Verify that WebDatabase.AutofillWebDataBackendImpl.OperationSuccess records
// success and failures in the methods of AutofillWebDataBackendImpl.
TEST_F(WebDataServiceAutofillTest, SuccessReporting) {
  // Values are taken from enum Result in autofill_webdata_backend_impl.cc.
  constexpr int kAddCreditCard_Success = 70;
  constexpr int kRemoveCreditCard_ReadFailure = 91;

  // Verify that success is reported correctly.
  {
    base::HistogramTester histogram_tester;
    wds_->AddCreditCard(CreditCard());
    WaitForEmptyDBSequence();
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "WebDatabase.AutofillWebDataBackendImpl.OperationResult"),
                BucketsAre(Bucket(kAddCreditCard_Success, 1)));
  }

  // Verify that failure is reported correctly.
  {
    base::HistogramTester histogram_tester;
    // Asynchronously delete a non-existing card which should trigger a failure
    // report.
    std::string non_existing_guid =
        base::Uuid::GenerateRandomV4().AsLowercaseString();
    wds_->RemoveCreditCard(non_existing_guid);
    WaitForEmptyDBSequence();
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "WebDatabase.AutofillWebDataBackendImpl.OperationResult"),
                BucketsAre(Bucket(kRemoveCreditCard_ReadFailure, 1)));
  }
}

}  // namespace

}  // namespace autofill
