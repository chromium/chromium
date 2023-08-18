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
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::Bucket;
using base::BucketsAre;
using base::Time;
using base::WaitableEvent;
using testing::_;
using testing::DoDefault;
using testing::ElementsAre;
using testing::ElementsAreArray;

namespace {

template <class T>
class AutofillWebDataServiceWaiter : public WebDataServiceConsumer {
 public:
  AutofillWebDataServiceWaiter() = default;

  AutofillWebDataServiceWaiter(const AutofillWebDataServiceWaiter&) = delete;
  AutofillWebDataServiceWaiter& operator=(const AutofillWebDataServiceWaiter&) =
      delete;

  WebDataServiceBase::Handle WaitForHandle() {
    run_loop_.Run();
    return handle_;
  }
  T& result() { return result_; }

 private:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) final {
    handle_ = handle;
    result_ = std::move(static_cast<WDResult<T>*>(result.get())->GetValue());
    run_loop_.Quit();
  }
  WebDataServiceBase::Handle handle_ = 0;
  base::RunLoop run_loop_;
  T result_;
};

const int kWebDataServiceTimeoutSeconds = 8;

}  // namespace

namespace autofill {

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class MockAutofillWebDataServiceObserver
    : public AutofillWebDataServiceObserverOnDBSequence {
 public:
  MOCK_METHOD(void,
              AutofillEntriesChanged,
              (const AutofillChangeList& changes),
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
        db_task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {}

 protected:
  void SetUp() override {
    base::FilePath path(WebDatabase::kInMemoryPath);
    // OSCrypt is used for encryption of credit card data in this test.
    OSCryptMocker::SetUp();

    wdbs_ = new WebDatabaseService(
        path, base::SequencedTaskRunner::GetCurrentDefault(), db_task_runner_);
    wdbs_->AddTable(std::make_unique<AutofillTable>());
    wdbs_->LoadDatabase();

    wds_ = new AutofillWebDataService(
        wdbs_, base::SequencedTaskRunner::GetCurrentDefault(), db_task_runner_);
    wds_->Init(base::NullCallback());
  }

  void TearDown() override {
    wds_->ShutdownOnUISequence();
    wdbs_->ShutdownDatabase();
    base::RunLoop run_loop;
    // Drain the db_task_runner.
    db_task_runner_->PostTaskAndReply(FROM_HERE, base::BindOnce([]() {}),
                                      run_loop.QuitClosure());
    run_loop.Run();
    OSCryptMocker::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  base::FilePath profile_dir_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  scoped_refptr<WebDatabaseService> wdbs_;
  scoped_refptr<AutofillWebDataService> wds_;
};

class WebDataServiceAutofillTest : public WebDataServiceTest {
 public:
  WebDataServiceAutofillTest()
      : unique_id1_(1),
        unique_id2_(2),
        test_timeout_(base::Seconds(kWebDataServiceTimeoutSeconds)),
        done_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED) {}

 protected:
  void SetUp() override {
    WebDataServiceTest::SetUp();
    name1_ = u"name1";
    name2_ = u"name2";
    value1_ = u"value1";
    value2_ = u"value2";

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
                       std::vector<FormFieldData>* form_fields) {
    FormFieldData field;
    field.name = name;
    field.value = value;
    form_fields->push_back(field);
  }

  std::u16string name1_;
  std::u16string name2_;
  std::u16string value1_;
  std::u16string value2_;
  int unique_id1_, unique_id2_;
  const base::TimeDelta test_timeout_;
  testing::NiceMock<MockAutofillWebDataServiceObserver> observer_;
  WaitableEvent done_event_;
};

TEST_F(WebDataServiceAutofillTest, FormFillAdd) {
  const AutofillChange expected_changes[] = {
      AutofillChange(AutofillChange::ADD, AutofillKey(name1_, value1_)),
      AutofillChange(AutofillChange::ADD, AutofillKey(name2_, value2_))};

  // This will verify that the correct notification is triggered,
  // passing the correct list of autofill keys in the details.
  EXPECT_CALL(observer_,
              AutofillEntriesChanged(ElementsAreArray(expected_changes)))
      .WillOnce(SignalEvent(&done_event_));

  std::vector<FormFieldData> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  AppendFormField(name2_, value2_, &form_fields);
  wds_->AddFormFields(form_fields);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);

  AutofillWebDataServiceWaiter<std::vector<AutofillEntry>> consumer;
  WebDataServiceBase::Handle handle;
  static const int limit = 10;
  handle = wds_->GetFormValuesForElementName(name1_, std::u16string(), limit,
                                             &consumer);
  EXPECT_EQ(handle, consumer.WaitForHandle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(value1_, consumer.result()[0].key().value());
}

TEST_F(WebDataServiceAutofillTest, FormFillRemoveOne) {
  // First add some values to autofill.
  EXPECT_CALL(observer_, AutofillEntriesChanged(_))
      .WillOnce(SignalEvent(&done_event_));
  std::vector<FormFieldData> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  wds_->AddFormFields(form_fields);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);

  // This will verify that the correct notification is triggered,
  // passing the correct list of autofill keys in the details.
  const AutofillChange expected_changes[] = {
      AutofillChange(AutofillChange::REMOVE, AutofillKey(name1_, value1_))};
  EXPECT_CALL(observer_,
              AutofillEntriesChanged(ElementsAreArray(expected_changes)))
      .WillOnce(SignalEvent(&done_event_));
  wds_->RemoveFormValueForElementName(name1_, value1_);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, FormFillRemoveMany) {
  base::TimeDelta one_day(base::Days(1));
  Time t = AutofillClock::Now();

  EXPECT_CALL(observer_, AutofillEntriesChanged(_))
      .WillOnce(SignalEvent(&done_event_));

  std::vector<FormFieldData> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  AppendFormField(name2_, value2_, &form_fields);
  wds_->AddFormFields(form_fields);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);

  // This will verify that the correct notification is triggered,
  // passing the correct list of autofill keys in the details.
  const AutofillChange expected_changes[] = {
      AutofillChange(AutofillChange::REMOVE, AutofillKey(name1_, value1_)),
      AutofillChange(AutofillChange::REMOVE, AutofillKey(name2_, value2_))};
  EXPECT_CALL(observer_,
              AutofillEntriesChanged(ElementsAreArray(expected_changes)))
      .WillOnce(SignalEvent(&done_event_));
  wds_->RemoveFormElementsAddedBetween(t, t + one_day);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, ProfileAdd) {
  AutofillProfile profile;

  // Check that GUID-based notification was sent.
  const AutofillProfileChange expected_change(AutofillProfileChange::ADD,
                                              profile.guid(), profile);
  EXPECT_CALL(observer_, AutofillProfileChanged(expected_change))
      .WillOnce(SignalEvent(&done_event_));

  wds_->AddAutofillProfile(profile);
  done_event_.TimedWait(test_timeout_);

  // Check that it was added.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<AutofillProfile>>>
      consumer;
  WebDataServiceBase::Handle handle = wds_->GetAutofillProfiles(
      AutofillProfile::Source::kLocalOrSyncable, &consumer);
  EXPECT_EQ(handle, consumer.WaitForHandle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(profile, *consumer.result()[0]);
}

TEST_F(WebDataServiceAutofillTest, ProfileRemove) {
  AutofillProfile profile;

  // Add a profile.
  EXPECT_CALL(observer_, AutofillProfileChanged(_))
      .WillOnce(SignalEvent(&done_event_));
  wds_->AddAutofillProfile(profile);
  done_event_.TimedWait(test_timeout_);

  // Check that it was added.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<AutofillProfile>>>
      consumer;
  WebDataServiceBase::Handle handle = wds_->GetAutofillProfiles(
      AutofillProfile::Source::kLocalOrSyncable, &consumer);
  EXPECT_EQ(handle, consumer.WaitForHandle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(profile, *consumer.result()[0]);

  // Check that GUID-based notification was sent.
  const AutofillProfileChange expected_change(AutofillProfileChange::REMOVE,
                                              profile.guid(), profile);
  EXPECT_CALL(observer_, AutofillProfileChanged(expected_change))
      .WillOnce(SignalEvent(&done_event_));

  // Remove the profile.
  wds_->RemoveAutofillProfile(profile.guid(),
                              AutofillProfile::Source::kLocalOrSyncable);
  done_event_.TimedWait(test_timeout_);

  // Check that it was removed.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<AutofillProfile>>>
      consumer2;
  WebDataServiceBase::Handle handle2 = wds_->GetAutofillProfiles(
      AutofillProfile::Source::kLocalOrSyncable, &consumer2);
  EXPECT_EQ(handle2, consumer2.WaitForHandle());
  ASSERT_EQ(0U, consumer2.result().size());
}

TEST_F(WebDataServiceAutofillTest, ProfileUpdate) {
  // The GUIDs are alphabetical for easier testing.
  AutofillProfile profile1("6141084B-72D7-4B73-90CF-3D6AC154673B");
  profile1.SetRawInfo(NAME_FIRST, u"Abe");
  profile1.FinalizeAfterImport();

  AutofillProfile profile2("087151C8-6AB1-487C-9095-28E80BE5DA15");
  profile2.SetRawInfo(NAME_FIRST, u"Alice");
  profile2.FinalizeAfterImport();

  EXPECT_CALL(observer_, AutofillProfileChanged(_))
      .WillOnce(DoDefault())
      .WillOnce(SignalEvent(&done_event_));

  wds_->AddAutofillProfile(profile1);
  wds_->AddAutofillProfile(profile2);
  done_event_.TimedWait(test_timeout_);

  // Check that they were added.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<AutofillProfile>>>
      consumer;
  WebDataServiceBase::Handle handle = wds_->GetAutofillProfiles(
      AutofillProfile::Source::kLocalOrSyncable, &consumer);
  EXPECT_EQ(handle, consumer.WaitForHandle());
  ASSERT_EQ(2U, consumer.result().size());
  EXPECT_EQ(profile2, *consumer.result()[0]);
  EXPECT_EQ(profile1, *consumer.result()[1]);

  AutofillProfile profile2_changed(profile2);
  profile2_changed.SetRawInfo(NAME_FIRST, u"Bill");
  const AutofillProfileChange expected_change(
      AutofillProfileChange::UPDATE, profile2.guid(), profile2_changed);

  EXPECT_CALL(observer_, AutofillProfileChanged(expected_change))
      .WillOnce(SignalEvent(&done_event_));

  // Update the profile.
  wds_->UpdateAutofillProfile(profile2_changed);
  done_event_.TimedWait(test_timeout_);

  // Check that the updates were made.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<AutofillProfile>>>
      consumer2;
  WebDataServiceBase::Handle handle2 = wds_->GetAutofillProfiles(
      AutofillProfile::Source::kLocalOrSyncable, &consumer2);
  EXPECT_EQ(handle2, consumer2.WaitForHandle());
  ASSERT_EQ(2U, consumer2.result().size());
  EXPECT_EQ(profile2_changed, *consumer2.result()[0]);
  EXPECT_NE(profile2, *consumer2.result()[0]);
  EXPECT_EQ(profile1, *consumer2.result()[1]);
}

TEST_F(WebDataServiceAutofillTest, CreditAdd) {
  CreditCard card;
  wds_->AddCreditCard(card);

  // Check that it was added.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<CreditCard>>>
      consumer;
  WebDataServiceBase::Handle handle = wds_->GetCreditCards(&consumer);
  EXPECT_EQ(handle, consumer.WaitForHandle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(card, *consumer.result()[0]);
}

TEST_F(WebDataServiceAutofillTest, CreditCardRemove) {
  CreditCard credit_card;

  // Add a credit card.
  wds_->AddCreditCard(credit_card);

  // Check that it was added.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<CreditCard>>>
      consumer;
  WebDataServiceBase::Handle handle = wds_->GetCreditCards(&consumer);
  EXPECT_EQ(handle, consumer.WaitForHandle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(credit_card, *consumer.result()[0]);

  // Remove the credit card.
  wds_->RemoveCreditCard(credit_card.guid());

  // Check that it was removed.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<CreditCard>>>
      consumer2;
  WebDataServiceBase::Handle handle2 = wds_->GetCreditCards(&consumer2);
  EXPECT_EQ(handle2, consumer2.WaitForHandle());
  ASSERT_EQ(0U, consumer2.result().size());
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
  WebDataServiceBase::Handle handle = wds_->GetCreditCards(&consumer);
  EXPECT_EQ(handle, consumer.WaitForHandle());
  ASSERT_EQ(2U, consumer.result().size());
  EXPECT_EQ(card2, *consumer.result()[0]);
  EXPECT_EQ(card1, *consumer.result()[1]);

  CreditCard card2_changed(card2);
  card2_changed.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Bill");

  wds_->UpdateCreditCard(card2_changed);

  // Check that the updates were made.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<CreditCard>>>
      consumer2;
  WebDataServiceBase::Handle handle2 = wds_->GetCreditCards(&consumer2);
  EXPECT_EQ(handle2, consumer2.WaitForHandle());
  ASSERT_EQ(2U, consumer2.result().size());
  EXPECT_NE(card2, *consumer2.result()[0]);
  EXPECT_EQ(card2_changed, *consumer2.result()[0]);
  EXPECT_EQ(card1, *consumer2.result()[1]);
}

TEST_F(WebDataServiceAutofillTest, AutofillRemoveModifiedBetween) {
  // Add a profile.
  EXPECT_CALL(observer_, AutofillProfileChanged(_))
      .WillOnce(SignalEvent(&done_event_));
  AutofillProfile profile;
  wds_->AddAutofillProfile(profile);
  done_event_.TimedWait(test_timeout_);

  // Check that it was added.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<AutofillProfile>>>
      profile_consumer;
  WebDataServiceBase::Handle handle = wds_->GetAutofillProfiles(
      AutofillProfile::Source::kLocalOrSyncable, &profile_consumer);
  EXPECT_EQ(handle, profile_consumer.WaitForHandle());
  ASSERT_EQ(1U, profile_consumer.result().size());
  EXPECT_EQ(profile, *profile_consumer.result()[0]);

  // Add a credit card.
  CreditCard credit_card;
  wds_->AddCreditCard(credit_card);

  // Check that it was added.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<CreditCard>>>
      card_consumer;
  handle = wds_->GetCreditCards(&card_consumer);
  EXPECT_EQ(handle, card_consumer.WaitForHandle());
  ASSERT_EQ(1U, card_consumer.result().size());
  EXPECT_EQ(credit_card, *card_consumer.result()[0]);

  // Check that GUID-based notification was sent for the profile.
  const AutofillProfileChange expected_profile_change(
      AutofillProfileChange::REMOVE, profile.guid(), profile);
  EXPECT_CALL(observer_, AutofillProfileChanged(expected_profile_change))
      .WillOnce(SignalEvent(&done_event_));

  // Remove the profile using time range of "all time".
  wds_->RemoveAutofillDataModifiedBetween(Time(), Time());
  done_event_.TimedWait(test_timeout_);

  // Check that the profile was removed.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<AutofillProfile>>>
      profile_consumer2;
  WebDataServiceBase::Handle handle2 = wds_->GetAutofillProfiles(
      AutofillProfile::Source::kLocalOrSyncable, &profile_consumer2);
  EXPECT_EQ(handle2, profile_consumer2.WaitForHandle());
  ASSERT_EQ(0U, profile_consumer2.result().size());

  // Check that the credit card was removed.
  AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<CreditCard>>>
      card_consumer2;
  handle2 = wds_->GetCreditCards(&card_consumer2);
  EXPECT_EQ(handle2, card_consumer2.WaitForHandle());
  ASSERT_EQ(0U, card_consumer2.result().size());
}

// Verify that WebDatabase.AutofillWebDataBackendImpl.OperationSuccess records
// success and failures in the methods of AutofillWebDataBackendImpl.
TEST_F(WebDataServiceAutofillTest, SuccessReporting) {
  auto add_card_synchronously = [&](const CreditCard& card) {
    wds_->AddCreditCard(card);

    // Wait that card was added by enqueuing a lookup which is handled on the
    // same SequencedTaskRunner as the previous `AddCreditCard` operation and
    // waiting for the result.
    AutofillWebDataServiceWaiter<std::vector<std::unique_ptr<CreditCard>>>
        consumer;
    WebDataServiceBase::Handle handle = wds_->GetCreditCards(&consumer);
    EXPECT_EQ(handle, consumer.WaitForHandle());
  };

  // Values are taken from enum Result in autofill_webdata_backend_impl.cc.
  constexpr int kAddCreditCard_Success = 70;
  constexpr int kRemoveCreditCard_ReadFailure = 91;

  // Verify that success is reported correctly.
  {
    base::HistogramTester histogram_tester;
    add_card_synchronously(CreditCard());
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

    // Add a second card just to ensure that the delete operation has been fully
    // processed.
    add_card_synchronously(CreditCard());

    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "WebDatabase.AutofillWebDataBackendImpl.OperationResult"),
                BucketsAre(Bucket(kRemoveCreditCard_ReadFailure, 1),
                           Bucket(kAddCreditCard_Success, 1)));
  }
}

}  // namespace autofill
