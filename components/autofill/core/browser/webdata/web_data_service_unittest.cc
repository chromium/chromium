// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
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
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::Time;
using base::TimeDelta;
using base::WaitableEvent;
using testing::_;
using testing::DoDefault;
using testing::ElementsAre;
using testing::ElementsAreArray;

namespace {

template <class T>
class AutofillWebDataServiceConsumer : public WebDataServiceConsumer {
 public:
  AutofillWebDataServiceConsumer() : handle_(0) {}
  virtual ~AutofillWebDataServiceConsumer() {}

  virtual void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) {
    handle_ = handle;
    result_ = std::move(static_cast<WDResult<T>*>(result.get())->GetValue());
  }

  WebDataServiceBase::Handle handle() { return handle_; }
  T& result() { return result_; }

 private:
  WebDataServiceBase::Handle handle_;
  T result_;
  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataServiceConsumer);
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
  MOCK_METHOD1(AutofillEntriesChanged, void(const AutofillChangeList& changes));
  MOCK_METHOD1(AutofillProfileChanged,
               void(const AutofillProfileChange& change));
};

class WebDataServiceTest : public testing::Test {
 public:
  WebDataServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

 protected:
  void SetUp() override {
    base::FilePath path(WebDatabase::kInMemoryPath);
    // OSCrypt is used for encryption of credit card data in this test.
    OSCryptMocker::SetUp();

    // TODO(pkasting): http://crbug.com/740773 This should likely be sequenced,
    // not single-threaded; it's also possible the various uses of this below
    // should each use their own sequences instead of sharing this one.
    auto db_task_runner = base::CreateSingleThreadTaskRunner(
        {base::ThreadPool(), base::MayBlock()});
    wdbs_ = new WebDatabaseService(path, base::ThreadTaskRunnerHandle::Get(),
                                   db_task_runner);
    wdbs_->AddTable(std::make_unique<AutofillTable>());
    wdbs_->LoadDatabase();

    wds_ = new AutofillWebDataService(
        wdbs_, base::ThreadTaskRunnerHandle::Get(), db_task_runner,
        WebDataServiceBase::ProfileErrorCallback());
    wds_->Init();
  }

  void TearDown() override {
    wds_->ShutdownOnUISequence();
    wdbs_->ShutdownDatabase();
    wds_ = nullptr;
    wdbs_ = nullptr;
    task_environment_.RunUntilIdle();
    OSCryptMocker::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  base::FilePath profile_dir_;
  scoped_refptr<AutofillWebDataService> wds_;
  scoped_refptr<WebDatabaseService> wdbs_;
};

class WebDataServiceAutofillTest : public WebDataServiceTest {
 public:
  WebDataServiceAutofillTest()
      : unique_id1_(1),
        unique_id2_(2),
        test_timeout_(TimeDelta::FromSeconds(kWebDataServiceTimeoutSeconds)),
        done_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED) {}

 protected:
  void SetUp() override {
    WebDataServiceTest::SetUp();
    name1_ = ASCIIToUTF16("name1");
    name2_ = ASCIIToUTF16("name2");
    value1_ = ASCIIToUTF16("value1");
    value2_ = ASCIIToUTF16("value2");

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

  void AppendFormField(const base::string16& name,
                       const base::string16& value,
                       std::vector<FormFieldData>* form_fields) {
    FormFieldData field;
    field.name = name;
    field.value = value;
    form_fields->push_back(field);
  }

  base::string16 name1_;
  base::string16 name2_;
  base::string16 value1_;
  base::string16 value2_;
  int unique_id1_, unique_id2_;
  const TimeDelta test_timeout_;
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

  AutofillWebDataServiceConsumer<std::vector<AutofillEntry>> consumer;
  WebDataServiceBase::Handle handle;
  static const int limit = 10;
  handle = wds_->GetFormValuesForElementName(name1_, base::string16(), limit,
                                             &consumer);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle, consumer.handle());
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
  TimeDelta one_day(TimeDelta::FromDays(1));
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
                                              profile.guid(), &profile);
  EXPECT_CALL(observer_, AutofillProfileChanged(expected_change))
      .WillOnce(SignalEvent(&done_event_));

  wds_->AddAutofillProfile(profile);
  done_event_.TimedWait(test_timeout_);

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<AutofillProfile>>>
      consumer;
  WebDataServiceBase::Handle handle = wds_->GetAutofillProfiles(&consumer);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle, consumer.handle());
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
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<AutofillProfile>>>
      consumer;
  WebDataServiceBase::Handle handle = wds_->GetAutofillProfiles(&consumer);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(profile, *consumer.result()[0]);

  // Check that GUID-based notification was sent.
  const AutofillProfileChange expected_change(AutofillProfileChange::REMOVE,
                                              profile.guid(), &profile);
  EXPECT_CALL(observer_, AutofillProfileChanged(expected_change))
      .WillOnce(SignalEvent(&done_event_));

  // Remove the profile.
  wds_->RemoveAutofillProfile(profile.guid());
  done_event_.TimedWait(test_timeout_);

  // Check that it was removed.
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<AutofillProfile>>>
      consumer2;
  WebDataServiceBase::Handle handle2 = wds_->GetAutofillProfiles(&consumer2);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle2, consumer2.handle());
  ASSERT_EQ(0U, consumer2.result().size());
}

TEST_F(WebDataServiceAutofillTest, ProfileUpdate) {
  // The GUIDs are alphabetical for easier testing.
  AutofillProfile profile1("6141084B-72D7-4B73-90CF-3D6AC154673B",
                           std::string());
  profile1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Abe"));
  AutofillProfile profile2("087151C8-6AB1-487C-9095-28E80BE5DA15",
                           std::string());
  profile2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Alice"));

  EXPECT_CALL(observer_, AutofillProfileChanged(_))
      .WillOnce(DoDefault())
      .WillOnce(SignalEvent(&done_event_));

  wds_->AddAutofillProfile(profile1);
  wds_->AddAutofillProfile(profile2);
  done_event_.TimedWait(test_timeout_);

  // Check that they were added.
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<AutofillProfile>>>
      consumer;
  WebDataServiceBase::Handle handle = wds_->GetAutofillProfiles(&consumer);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(2U, consumer.result().size());
  EXPECT_EQ(profile2, *consumer.result()[0]);
  EXPECT_EQ(profile1, *consumer.result()[1]);

  AutofillProfile profile2_changed(profile2);
  profile2_changed.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Bill"));
  const AutofillProfileChange expected_change(
      AutofillProfileChange::UPDATE, profile2.guid(), &profile2_changed);

  EXPECT_CALL(observer_, AutofillProfileChanged(expected_change))
      .WillOnce(SignalEvent(&done_event_));

  // Update the profile.
  wds_->UpdateAutofillProfile(profile2_changed);
  done_event_.TimedWait(test_timeout_);

  // Check that the updates were made.
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<AutofillProfile>>>
      consumer2;
  WebDataServiceBase::Handle handle2 = wds_->GetAutofillProfiles(&consumer2);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle2, consumer2.handle());
  ASSERT_EQ(2U, consumer2.result().size());
  EXPECT_EQ(profile2_changed, *consumer2.result()[0]);
  EXPECT_NE(profile2, *consumer2.result()[0]);
  EXPECT_EQ(profile1, *consumer2.result()[1]);
}

TEST_F(WebDataServiceAutofillTest, CreditAdd) {
  CreditCard card;
  wds_->AddCreditCard(card);

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<CreditCard>>>
      consumer;
  WebDataServiceBase::Handle handle = wds_->GetCreditCards(&consumer);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(card, *consumer.result()[0]);
}

TEST_F(WebDataServiceAutofillTest, CreditCardRemove) {
  CreditCard credit_card;

  // Add a credit card.
  wds_->AddCreditCard(credit_card);

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<CreditCard>>>
      consumer;
  WebDataServiceBase::Handle handle = wds_->GetCreditCards(&consumer);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(credit_card, *consumer.result()[0]);

  // Remove the credit card.
  wds_->RemoveCreditCard(credit_card.guid());

  // Check that it was removed.
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<CreditCard>>>
      consumer2;
  WebDataServiceBase::Handle handle2 = wds_->GetCreditCards(&consumer2);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle2, consumer2.handle());
  ASSERT_EQ(0U, consumer2.result().size());
}

TEST_F(WebDataServiceAutofillTest, CreditUpdate) {
  CreditCard card1("E4D2662E-5E16-44F3-AF5A-5A77FAE4A6F3", std::string());
  card1.SetRawInfo(CREDIT_CARD_NAME_FULL, ASCIIToUTF16("Abe"));
  CreditCard card2("B9C52112-BD5F-4080-84E1-C651D2CB90E2", std::string());
  card2.SetRawInfo(CREDIT_CARD_NAME_FULL, ASCIIToUTF16("Alice"));

  wds_->AddCreditCard(card1);
  wds_->AddCreditCard(card2);

  // Check that they got added.
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<CreditCard>>>
      consumer;
  WebDataServiceBase::Handle handle = wds_->GetCreditCards(&consumer);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(2U, consumer.result().size());
  EXPECT_EQ(card2, *consumer.result()[0]);
  EXPECT_EQ(card1, *consumer.result()[1]);

  CreditCard card2_changed(card2);
  card2_changed.SetRawInfo(CREDIT_CARD_NAME_FULL, ASCIIToUTF16("Bill"));

  wds_->UpdateCreditCard(card2_changed);

  // Check that the updates were made.
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<CreditCard>>>
      consumer2;
  WebDataServiceBase::Handle handle2 = wds_->GetCreditCards(&consumer2);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle2, consumer2.handle());
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
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<AutofillProfile>>>
      profile_consumer;
  WebDataServiceBase::Handle handle =
      wds_->GetAutofillProfiles(&profile_consumer);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle, profile_consumer.handle());
  ASSERT_EQ(1U, profile_consumer.result().size());
  EXPECT_EQ(profile, *profile_consumer.result()[0]);

  // Add a credit card.
  CreditCard credit_card;
  wds_->AddCreditCard(credit_card);

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<CreditCard>>>
      card_consumer;
  handle = wds_->GetCreditCards(&card_consumer);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle, card_consumer.handle());
  ASSERT_EQ(1U, card_consumer.result().size());
  EXPECT_EQ(credit_card, *card_consumer.result()[0]);

  // Check that GUID-based notification was sent for the profile.
  const AutofillProfileChange expected_profile_change(
      AutofillProfileChange::REMOVE, profile.guid(), &profile);
  EXPECT_CALL(observer_, AutofillProfileChanged(expected_profile_change))
      .WillOnce(SignalEvent(&done_event_));

  // Remove the profile using time range of "all time".
  wds_->RemoveAutofillDataModifiedBetween(Time(), Time());
  done_event_.TimedWait(test_timeout_);

  // Check that the profile was removed.
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<AutofillProfile>>>
      profile_consumer2;
  WebDataServiceBase::Handle handle2 =
      wds_->GetAutofillProfiles(&profile_consumer2);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle2, profile_consumer2.handle());
  ASSERT_EQ(0U, profile_consumer2.result().size());

  // Check that the credit card was removed.
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<CreditCard>>>
      card_consumer2;
  handle2 = wds_->GetCreditCards(&card_consumer2);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(handle2, card_consumer2.handle());
  ASSERT_EQ(0U, card_consumer2.result().size());
}

}  // namespace autofill
