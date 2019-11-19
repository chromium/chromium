// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_syncable_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_change_processor_wrapper_for_test.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::SyncChange;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using testing::AnyNumber;
using testing::DoAll;
using testing::ElementsAre;
using testing::IgnoreResult;
using testing::Invoke;
using testing::IsEmpty;
using testing::Matches;
using testing::Return;
using testing::SetArgPointee;
using testing::UnorderedElementsAre;
using testing::_;

namespace password_manager {

// Defined in the implementation file corresponding to this test.
syncer::SyncData SyncDataFromPassword(const autofill::PasswordForm& password);
autofill::PasswordForm PasswordFromSpecifics(
    const sync_pb::PasswordSpecificsData& password);
std::string MakePasswordSyncTag(const sync_pb::PasswordSpecificsData& password);
std::string MakePasswordSyncTag(const autofill::PasswordForm& password);

namespace {

// PasswordForm values for tests.
constexpr autofill::PasswordForm::Type kArbitraryType =
    autofill::PasswordForm::Type::kGenerated;
constexpr char kIconUrl[] = "https://fb.com/Icon";
constexpr char kDisplayName[] = "Agent Smith";
constexpr char kFederationUrl[] = "https://fb.com/";
constexpr char kPassword[] = "abcdef";
constexpr char kSignonRealm[] = "abc";
constexpr char kSignonRealm2[] = "def";
constexpr char kSignonRealm3[] = "xyz";
constexpr int kTimesUsed = 5;
constexpr char kUsername[] = "godzilla";

typedef std::vector<SyncChange> SyncChangeList;

const sync_pb::PasswordSpecificsData& GetPasswordSpecifics(
    const syncer::SyncData& sync_data) {
  return sync_data.GetSpecifics().password().client_only_encrypted_data();
}

MATCHER(HasDateSynced, "") {
  return !arg.date_synced.is_null() && !arg.date_synced.is_max();
}

MATCHER_P(PasswordIs, form, "") {
  sync_pb::PasswordSpecificsData actual_password =
      GetPasswordSpecifics(SyncDataFromPassword(arg));
  sync_pb::PasswordSpecificsData expected_password =
      GetPasswordSpecifics(SyncDataFromPassword(form));
  if (expected_password.scheme() == actual_password.scheme() &&
      expected_password.signon_realm() == actual_password.signon_realm() &&
      expected_password.origin() == actual_password.origin() &&
      expected_password.action() == actual_password.action() &&
      expected_password.username_element() ==
          actual_password.username_element() &&
      expected_password.password_element() ==
          actual_password.password_element() &&
      expected_password.username_value() == actual_password.username_value() &&
      expected_password.password_value() == actual_password.password_value() &&
      expected_password.preferred() == actual_password.preferred() &&
      expected_password.date_last_used() == actual_password.date_last_used() &&
      expected_password.date_created() == actual_password.date_created() &&
      expected_password.blacklisted() == actual_password.blacklisted() &&
      expected_password.type() == actual_password.type() &&
      expected_password.times_used() == actual_password.times_used() &&
      expected_password.display_name() == actual_password.display_name() &&
      expected_password.avatar_url() == actual_password.avatar_url() &&
      expected_password.federation_url() == actual_password.federation_url())
    return true;

  *result_listener << "Password protobuf does not match; expected:\n"
                   << form << '\n'
                   << "actual:" << '\n'
                   << arg;
  return false;
}

MATCHER_P2(SyncChangeIs, change_type, password, "") {
  const SyncData& data = arg.sync_data();
  autofill::PasswordForm form =
      PasswordFromSpecifics(GetPasswordSpecifics(data));
  return (arg.change_type() == change_type &&
          syncer::SyncDataLocal(data).GetTag() ==
              MakePasswordSyncTag(password) &&
          (change_type == SyncChange::ACTION_DELETE ||
           Matches(PasswordIs(password))(form)));
}

// The argument is std::vector<autofill::PasswordForm*>*. The caller is
// responsible for the lifetime of all the password forms.
ACTION_P(AppendForm, form) {
  arg0->push_back(std::make_unique<autofill::PasswordForm>(form));
  return true;
}

// Creates a sync data consisting of password specifics. The sign on realm is
// set to |signon_realm|.
SyncData CreateSyncData(const std::string& signon_realm) {
  sync_pb::EntitySpecifics password_data;
  sync_pb::PasswordSpecificsData* password_specifics =
      password_data.mutable_password()->mutable_client_only_encrypted_data();
  password_specifics->set_signon_realm(signon_realm);
  password_specifics->set_type(
      static_cast<int>(autofill::PasswordForm::Type::kGenerated));
  password_specifics->set_times_used(3);
  password_specifics->set_display_name("Mr. X");
  password_specifics->set_avatar_url("https://accounts.google.com/Icon");
  password_specifics->set_federation_url("https://google.com");
  password_specifics->set_username_value("kingkong");
  password_specifics->set_password_value("sicrit");

  std::string tag = MakePasswordSyncTag(*password_specifics);
  return syncer::SyncData::CreateLocalData(tag, tag, password_data);
}

SyncChange CreateSyncChange(const autofill::PasswordForm& password,
                            SyncChange::SyncChangeType type) {
  SyncData data = SyncDataFromPassword(password);
  return SyncChange(FROM_HERE, type, data);
}

// Mock implementation of SyncChangeProcessor.
class MockSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  MockSyncChangeProcessor() {}

  MOCK_METHOD2(ProcessSyncChanges,
               SyncError(const base::Location&, const SyncChangeList& list));
  SyncDataList GetAllSyncData(syncer::ModelType type) const override {
    NOTREACHED();
    return SyncDataList();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSyncChangeProcessor);
};

// Convenience wrapper around a PasswordSyncableService and PasswordStore
// pair.
class PasswordSyncableServiceWrapper {
 public:
  PasswordSyncableServiceWrapper() {
    password_store_ = new testing::StrictMock<MockPasswordStore>;
    password_store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr);
    service_.reset(
        new PasswordSyncableService(password_store_->GetSyncInterface()));

    ON_CALL(*password_store_, AddLoginImpl(HasDateSynced(), _))
        .WillByDefault([](const autofill::PasswordForm& form,
                          password_manager::AddLoginError* error) {
          if (error) {
            *error = AddLoginError::kNone;
          }
          return PasswordStoreChangeList();
        });
    ON_CALL(*password_store_, RemoveLoginImpl(_))
        .WillByDefault(Return(PasswordStoreChangeList()));
    ON_CALL(*password_store_, UpdateLoginImpl(HasDateSynced(), _))
        .WillByDefault([](const autofill::PasswordForm& form,
                          password_manager::UpdateLoginError* error) {
          if (error) {
            *error = UpdateLoginError::kNone;
          }
          return PasswordStoreChangeList();
        });
    EXPECT_CALL(*password_store(), NotifyLoginsChanged(_)).Times(AnyNumber());
  }

  ~PasswordSyncableServiceWrapper() { password_store_->ShutdownOnUIThread(); }

  MockPasswordStore* password_store() { return password_store_.get(); }

  PasswordSyncableService* service() { return service_.get(); }

 private:
  scoped_refptr<MockPasswordStore> password_store_;
  std::unique_ptr<PasswordSyncableService> service_;

  DISALLOW_COPY_AND_ASSIGN(PasswordSyncableServiceWrapper);
};

class PasswordSyncableServiceTest : public testing::Test {
 public:
  PasswordSyncableServiceTest()
      : processor_(new testing::StrictMock<MockSyncChangeProcessor>) {
    ON_CALL(*processor_, ProcessSyncChanges(_, _))
        .WillByDefault(Return(SyncError()));
  }
  MockPasswordStore* password_store() { return wrapper_.password_store(); }
  PasswordSyncableService* service() { return wrapper_.service(); }

  MOCK_METHOD1(StartSyncFlare, void(syncer::ModelType));

 protected:
  std::unique_ptr<MockSyncChangeProcessor> processor_;

 private:
  // Used by the password store.
  base::test::TaskEnvironment task_environment_;
  PasswordSyncableServiceWrapper wrapper_;
};

// Both sync and password db have data that are not present in the other.
TEST_F(PasswordSyncableServiceTest, AdditionsInBoth) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.password_value = base::ASCIIToUTF16(kPassword);

  SyncDataList list;
  list.push_back(CreateSyncData(kSignonRealm2));
  autofill::PasswordForm new_from_sync =
      PasswordFromSpecifics(GetPasswordSpecifics(list.back()));

  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));
  EXPECT_CALL(*password_store(), AddLoginImpl(PasswordIs(new_from_sync), _));
  EXPECT_CALL(*processor_,
              ProcessSyncChanges(
                  _, ElementsAre(SyncChangeIs(SyncChange::ACTION_ADD, form))));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, list, std::move(processor_),
      std::unique_ptr<syncer::SyncErrorFactory>());
}

// Sync has data that is not present in the password db.
TEST_F(PasswordSyncableServiceTest, AdditionOnlyInSync) {
  SyncDataList list;
  list.push_back(CreateSyncData(kSignonRealm));
  autofill::PasswordForm new_from_sync =
      PasswordFromSpecifics(GetPasswordSpecifics(list.back()));

  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));
  EXPECT_CALL(*password_store(), AddLoginImpl(PasswordIs(new_from_sync), _));
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, IsEmpty()));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, list, std::move(processor_),
      std::unique_ptr<syncer::SyncErrorFactory>());
}

// Passwords db has data that is not present in sync.
TEST_F(PasswordSyncableServiceTest, AdditionOnlyInPasswordStore) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  form.times_used = kTimesUsed;
  form.type = kArbitraryType;
  form.display_name = base::ASCIIToUTF16(kDisplayName);
  form.icon_url = GURL(kIconUrl);
  form.federation_origin = url::Origin::Create(GURL(kFederationUrl));
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.password_value = base::ASCIIToUTF16(kPassword);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));

  EXPECT_CALL(*processor_,
              ProcessSyncChanges(
                  _, ElementsAre(SyncChangeIs(SyncChange::ACTION_ADD, form))));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, SyncDataList(), std::move(processor_),
      std::unique_ptr<syncer::SyncErrorFactory>());
}

// Both passwords db and sync contain the same data.
TEST_F(PasswordSyncableServiceTest, BothInSync) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  form.times_used = kTimesUsed;
  form.type = kArbitraryType;
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.password_value = base::ASCIIToUTF16(kPassword);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));

  EXPECT_CALL(*processor_, ProcessSyncChanges(_, IsEmpty()));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, SyncDataList(1, SyncDataFromPassword(form)),
      std::move(processor_), std::unique_ptr<syncer::SyncErrorFactory>());
}

// Both passwords db and sync have the same data but they need to be merged
// as some fields of the data differ.
TEST_F(PasswordSyncableServiceTest, Merge) {
  autofill::PasswordForm form1;
  form1.signon_realm = kSignonRealm;
  form1.action = GURL("http://pie.com");
  form1.date_created = base::Time::Now();
  form1.preferred = true;
  form1.date_last_used = form1.date_created;
  form1.username_value = base::ASCIIToUTF16(kUsername);
  form1.password_value = base::ASCIIToUTF16(kPassword);

  autofill::PasswordForm form2(form1);
  form2.preferred = false;
  form2.date_created = form1.date_created + base::TimeDelta::FromDays(1);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form1));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));
  EXPECT_CALL(*password_store(), UpdateLoginImpl(PasswordIs(form2), _));
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, IsEmpty()));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, SyncDataList(1, SyncDataFromPassword(form2)),
      std::move(processor_), std::unique_ptr<syncer::SyncErrorFactory>());
}

// Initiate sync due to local DB changes.
TEST_F(PasswordSyncableServiceTest, PasswordStoreChanges) {
  // Save the reference to the processor because |processor_| is NULL after
  // MergeDataAndStartSyncing().
  MockSyncChangeProcessor& weak_processor = *processor_;
  EXPECT_CALL(weak_processor, ProcessSyncChanges(_, IsEmpty()));
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));
  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, SyncDataList(), std::move(processor_),
      std::unique_ptr<syncer::SyncErrorFactory>());

  autofill::PasswordForm form1;
  form1.signon_realm = kSignonRealm;
  autofill::PasswordForm form2;
  form2.signon_realm = kSignonRealm2;
  autofill::PasswordForm form3;
  form3.signon_realm = kSignonRealm3;

  SyncChangeList sync_list;
  sync_list.push_back(CreateSyncChange(form1, SyncChange::ACTION_ADD));
  sync_list.push_back(CreateSyncChange(form2, SyncChange::ACTION_UPDATE));
  sync_list.push_back(CreateSyncChange(form3, SyncChange::ACTION_DELETE));

  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form1));
  list.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form2));
  list.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form3));
  EXPECT_CALL(
      weak_processor,
      ProcessSyncChanges(
          _, ElementsAre(SyncChangeIs(SyncChange::ACTION_ADD, form1),
                         SyncChangeIs(SyncChange::ACTION_UPDATE, form2),
                         SyncChangeIs(SyncChange::ACTION_DELETE, form3))));
  service()->ActOnPasswordStoreChanges(list);
}

// Process all types of changes from sync.
TEST_F(PasswordSyncableServiceTest, ProcessSyncChanges) {
  autofill::PasswordForm updated_form;
  updated_form.signon_realm = kSignonRealm;
  updated_form.action = GURL("http://foo.com");
  updated_form.date_created = base::Time::Now();
  updated_form.username_value = base::ASCIIToUTF16(kUsername);
  updated_form.password_value = base::ASCIIToUTF16(kPassword);
  autofill::PasswordForm deleted_form;
  deleted_form.signon_realm = kSignonRealm2;
  deleted_form.action = GURL("http://bar.com");
  deleted_form.blacklisted_by_user = true;

  SyncData add_data = CreateSyncData(kSignonRealm3);
  autofill::PasswordForm new_from_sync =
      PasswordFromSpecifics(GetPasswordSpecifics(add_data));

  SyncChangeList list;
  list.push_back(
      SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD, add_data));
  list.push_back(
      CreateSyncChange(updated_form, syncer::SyncChange::ACTION_UPDATE));
  list.push_back(
      CreateSyncChange(deleted_form, syncer::SyncChange::ACTION_DELETE));
  EXPECT_CALL(*password_store(), AddLoginImpl(PasswordIs(new_from_sync), _));
  EXPECT_CALL(*password_store(), UpdateLoginImpl(PasswordIs(updated_form), _));
  EXPECT_CALL(*password_store(), RemoveLoginImpl(PasswordIs(deleted_form)));
  service()->ProcessSyncChanges(FROM_HERE, list);
}

// Retrives sync data from the model.
TEST_F(PasswordSyncableServiceTest, GetAllSyncData) {
  autofill::PasswordForm form1;
  form1.signon_realm = kSignonRealm;
  form1.action = GURL("http://foo.com");
  form1.times_used = kTimesUsed;
  form1.type = kArbitraryType;
  form1.display_name = base::ASCIIToUTF16(kDisplayName);
  form1.icon_url = GURL(kIconUrl);
  form1.federation_origin = url::Origin::Create(GURL(kFederationUrl));
  form1.username_value = base::ASCIIToUTF16(kUsername);
  form1.password_value = base::ASCIIToUTF16(kPassword);
  autofill::PasswordForm form2;
  form2.signon_realm = kSignonRealm2;
  form2.action = GURL("http://bar.com");
  form2.blacklisted_by_user = true;
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form1));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_))
      .WillOnce(AppendForm(form2));

  SyncDataList actual_list = service()->GetAllSyncData(syncer::PASSWORDS);
  std::vector<autofill::PasswordForm> actual_form_list;
  for (auto it = actual_list.begin(); it != actual_list.end(); ++it) {
    actual_form_list.push_back(
        PasswordFromSpecifics(GetPasswordSpecifics(*it)));
  }
  EXPECT_THAT(actual_form_list,
              UnorderedElementsAre(PasswordIs(form1), PasswordIs(form2)));
}

// Creates 2 PasswordSyncableService instances, merges the content of the first
// one to the second one and back.
TEST_F(PasswordSyncableServiceTest, MergeDataAndPushBack) {
  autofill::PasswordForm form1;
  form1.signon_realm = kSignonRealm;
  form1.action = GURL("http://foo.com");
  form1.username_value = base::ASCIIToUTF16(kUsername);
  form1.password_value = base::ASCIIToUTF16(kPassword);

  PasswordSyncableServiceWrapper other_service_wrapper;
  autofill::PasswordForm form2;
  form2.signon_realm = kSignonRealm2;
  form2.action = GURL("http://bar.com");
  form2.username_value = base::ASCIIToUTF16(kUsername);
  form2.password_value = base::ASCIIToUTF16(kPassword);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form1));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));
  EXPECT_CALL(*other_service_wrapper.password_store(),
              FillAutofillableLogins(_))
      .WillOnce(AppendForm(form2));
  EXPECT_CALL(*other_service_wrapper.password_store(), FillBlacklistLogins(_))
      .WillOnce(Return(true));
  // This method reads all passwords from the database. Make sure that the
  // database is not read twice if there was no problem getting all the
  // passwords during the first read.
  EXPECT_CALL(*password_store(), DeleteUndecryptableLogins()).Times(0);

  EXPECT_CALL(*password_store(), AddLoginImpl(PasswordIs(form2), _));
  EXPECT_CALL(*other_service_wrapper.password_store(),
              AddLoginImpl(PasswordIs(form1), _));

  syncer::SyncDataList other_service_data =
      other_service_wrapper.service()->GetAllSyncData(syncer::PASSWORDS);
  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, other_service_data,
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          other_service_wrapper.service()),
      std::unique_ptr<syncer::SyncErrorFactory>());
}

// Calls ActOnPasswordStoreChanges without SyncChangeProcessor. StartSyncFlare
// should be called.
TEST_F(PasswordSyncableServiceTest, StartSyncFlare) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.password_value = base::ASCIIToUTF16(kPassword);
  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));

  // No flare and no SyncChangeProcessor, the call shouldn't crash.
  service()->ActOnPasswordStoreChanges(list);

  // Set the flare. It should be called as there is no SyncChangeProcessor.
  service()->InjectStartSyncFlare(base::Bind(
      &PasswordSyncableServiceTest::StartSyncFlare, base::Unretained(this)));
  EXPECT_CALL(*this, StartSyncFlare(syncer::PASSWORDS));
  service()->ActOnPasswordStoreChanges(list);
}

// Start syncing with an error. Subsequent password store updates shouldn't be
// propagated to Sync.
TEST_F(PasswordSyncableServiceTest, FailedReadFromPasswordStore) {
  std::unique_ptr<syncer::SyncErrorFactoryMock> error_factory(
      new syncer::SyncErrorFactoryMock);
  syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                          "Failed to get passwords from store.",
                          syncer::PASSWORDS);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(Return(false));
  EXPECT_CALL(*password_store(), DeleteUndecryptableLogins())
      .WillOnce(Return(DatabaseCleanupResult::kDatabaseUnavailable));
  EXPECT_CALL(*error_factory, CreateAndUploadError(_, _))
      .WillOnce(Return(error));
  // ActOnPasswordStoreChanges() below shouldn't generate any changes for Sync.
  // |processor_| will be destroyed in MergeDataAndStartSyncing().
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, _)).Times(0);
  syncer::SyncMergeResult result = service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, syncer::SyncDataList(), std::move(processor_),
      std::move(error_factory));
  EXPECT_TRUE(result.error().IsSet());

  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  service()->ActOnPasswordStoreChanges(list);
}
class PasswordSyncableServiceTestWithoutDeleteCorruptedPasswords
    : public PasswordSyncableServiceTest {
 public:
  PasswordSyncableServiceTestWithoutDeleteCorruptedPasswords() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kDeleteCorruptedPasswords);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that passwords are recovered for Sync users using the older logic (i.e.
// recover passwords only for Sync users) when the feature for deleting
// corrupted passwords for all users is disabled.
TEST_F(PasswordSyncableServiceTestWithoutDeleteCorruptedPasswords,
       RecoverPasswordsOnlyForSyncUsers) {
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, IsEmpty()));
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .Times(2)
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*password_store(), DeleteUndecryptableLogins())
      .WillOnce(Return(DatabaseCleanupResult::kSuccess));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));

  syncer::SyncMergeResult result = service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, SyncDataList(), std::move(processor_), nullptr);
  EXPECT_FALSE(result.error().IsSet());
}

class PasswordSyncableServiceTestWithDeleteCorruptedPasswords
    : public PasswordSyncableServiceTest {
 public:
  PasswordSyncableServiceTestWithDeleteCorruptedPasswords() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDeleteCorruptedPasswords);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that passwords are not recovered when merging data if the feature for
// deleting passwords for all users is enabled.
TEST_F(PasswordSyncableServiceTestWithDeleteCorruptedPasswords,
       PasswordRecoveryForAllUsersEnabled) {
  auto error_factory = std::make_unique<syncer::SyncErrorFactoryMock>();
  syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                          "Failed to get passwords from store.",
                          syncer::PASSWORDS);
  EXPECT_CALL(*error_factory, CreateAndUploadError(_, _))
      .WillOnce(Return(error));

  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(Return(false));
  EXPECT_CALL(*password_store(), DeleteUndecryptableLogins()).Times(0);

  syncer::SyncMergeResult result = service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, SyncDataList(), std::move(processor_),
      std::move(error_factory));
  EXPECT_TRUE(result.error().IsSet());
}

// Database cleanup fails because encryption is unavailable.
TEST_F(PasswordSyncableServiceTestWithoutDeleteCorruptedPasswords,
       FailedDeleteUndecryptableLogins) {
  auto error_factory = std::make_unique<syncer::SyncErrorFactoryMock>();
  syncer::SyncError error(
      FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
      "Failed to get encryption key during database cleanup.",
      syncer::PASSWORDS);
  EXPECT_CALL(*error_factory, CreateAndUploadError(_, _))
      .WillOnce(Return(error));

  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(Return(false));
  EXPECT_CALL(*password_store(), DeleteUndecryptableLogins())
      .WillOnce(Return(DatabaseCleanupResult::kEncryptionUnavailable));

  syncer::SyncMergeResult result = service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, SyncDataList(), std::move(processor_),
      std::move(error_factory));
  EXPECT_TRUE(result.error().IsSet());
}

// Start syncing with an error in ProcessSyncChanges. Subsequent password store
// updates shouldn't be propagated to Sync.
TEST_F(PasswordSyncableServiceTest, FailedProcessSyncChanges) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.password_value = base::ASCIIToUTF16(kPassword);
  std::unique_ptr<syncer::SyncErrorFactoryMock> error_factory(
      new syncer::SyncErrorFactoryMock);
  syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                          "There is a problem", syncer::PASSWORDS);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));

  // ActOnPasswordStoreChanges() below shouldn't generate any changes for Sync.
  // |processor_| will be destroyed in MergeDataAndStartSyncing().
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, _))
      .Times(1)
      .WillOnce(Return(error));
  syncer::SyncMergeResult result = service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, syncer::SyncDataList(), std::move(processor_),
      std::move(error_factory));
  EXPECT_TRUE(result.error().IsSet());

  form.signon_realm = kSignonRealm2;
  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  service()->ActOnPasswordStoreChanges(list);
}

// Serialize and deserialize empty federation_origin and make sure it's an empty
// string.
TEST_F(PasswordSyncableServiceTest, SerializeEmptyFederation) {
  autofill::PasswordForm form;
  EXPECT_TRUE(form.federation_origin.opaque());
  syncer::SyncData data = SyncDataFromPassword(form);
  const sync_pb::PasswordSpecificsData& specifics = GetPasswordSpecifics(data);
  EXPECT_TRUE(specifics.has_federation_url());
  EXPECT_EQ(std::string(), specifics.federation_url());

  // Deserialize back.
  form = PasswordFromSpecifics(specifics);
  EXPECT_TRUE(form.federation_origin.opaque());

  // Make sure that the Origins uploaded incorrectly are still deserialized
  // correctly.
  // crbug.com/593380.
  sync_pb::PasswordSpecificsData specifics1;
  specifics1.set_federation_url("null");
  form = PasswordFromSpecifics(specifics1);
  EXPECT_TRUE(form.federation_origin.opaque());
}

// Serialize empty PasswordForm and make sure the Sync representation is
// matching the expectations
TEST_F(PasswordSyncableServiceTest, SerializeEmptyPasswordForm) {
  autofill::PasswordForm form;
  syncer::SyncData data = SyncDataFromPassword(form);
  const sync_pb::PasswordSpecificsData& specifics = GetPasswordSpecifics(data);
  EXPECT_TRUE(specifics.has_scheme());
  EXPECT_EQ(0, specifics.scheme());
  EXPECT_TRUE(specifics.has_signon_realm());
  EXPECT_EQ("", specifics.signon_realm());
  EXPECT_TRUE(specifics.has_origin());
  EXPECT_EQ("", specifics.origin());
  EXPECT_TRUE(specifics.has_action());
  EXPECT_EQ("", specifics.action());
  EXPECT_TRUE(specifics.has_username_element());
  EXPECT_EQ("", specifics.username_element());
  EXPECT_TRUE(specifics.has_username_value());
  EXPECT_EQ("", specifics.username_value());
  EXPECT_TRUE(specifics.has_password_element());
  EXPECT_EQ("", specifics.password_element());
  EXPECT_TRUE(specifics.has_password_value());
  EXPECT_EQ("", specifics.password_value());
  EXPECT_TRUE(specifics.has_preferred());
  EXPECT_FALSE(specifics.preferred());
  EXPECT_TRUE(specifics.has_date_last_used());
  EXPECT_EQ(0, specifics.date_last_used());
  EXPECT_TRUE(specifics.has_date_created());
  EXPECT_EQ(0, specifics.date_created());
  EXPECT_TRUE(specifics.has_blacklisted());
  EXPECT_FALSE(specifics.blacklisted());
  EXPECT_TRUE(specifics.has_type());
  EXPECT_EQ(0, specifics.type());
  EXPECT_TRUE(specifics.has_times_used());
  EXPECT_EQ(0, specifics.times_used());
  EXPECT_TRUE(specifics.has_display_name());
  EXPECT_EQ("", specifics.display_name());
  EXPECT_TRUE(specifics.has_avatar_url());
  EXPECT_EQ("", specifics.avatar_url());
  EXPECT_TRUE(specifics.has_federation_url());
  EXPECT_EQ("", specifics.federation_url());
}

// Serialize a PasswordForm with non-default member values and make sure the
// Sync representation is matching the expectations.
TEST_F(PasswordSyncableServiceTest, SerializeNonEmptyPasswordForm) {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::Scheme::kUsernameOnly;
  form.signon_realm = "http://google.com/";
  form.origin = GURL("https://google.com/origin");
  form.action = GURL("https://google.com/action");
  form.username_element = base::ASCIIToUTF16("username_element");
  form.username_value = base::ASCIIToUTF16("god@google.com");
  form.password_element = base::ASCIIToUTF16("password_element");
  form.password_value = base::ASCIIToUTF16("!@#$%^&*()");
  form.preferred = true;
  form.date_last_used = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(100));
  form.date_created = base::Time::FromInternalValue(100);
  form.blacklisted_by_user = true;
  form.type = autofill::PasswordForm::Type::kMaxValue;
  form.times_used = 11;
  form.display_name = base::ASCIIToUTF16("Great Peter");
  form.icon_url = GURL("https://google.com/icon");
  form.federation_origin = url::Origin::Create(GURL("https://google.com/"));

  syncer::SyncData data = SyncDataFromPassword(form);
  const sync_pb::PasswordSpecificsData& specifics = GetPasswordSpecifics(data);
  EXPECT_TRUE(specifics.has_scheme());
  EXPECT_EQ(static_cast<int>(autofill::PasswordForm::Scheme::kUsernameOnly),
            specifics.scheme());
  EXPECT_TRUE(specifics.has_signon_realm());
  EXPECT_EQ("http://google.com/", specifics.signon_realm());
  EXPECT_TRUE(specifics.has_origin());
  EXPECT_EQ("https://google.com/origin", specifics.origin());
  EXPECT_TRUE(specifics.has_action());
  EXPECT_EQ("https://google.com/action", specifics.action());
  EXPECT_TRUE(specifics.has_username_element());
  EXPECT_EQ("username_element", specifics.username_element());
  EXPECT_TRUE(specifics.has_username_value());
  EXPECT_EQ("god@google.com", specifics.username_value());
  EXPECT_TRUE(specifics.has_password_element());
  EXPECT_EQ("password_element", specifics.password_element());
  EXPECT_TRUE(specifics.has_password_value());
  EXPECT_EQ("!@#$%^&*()", specifics.password_value());
  EXPECT_TRUE(specifics.has_preferred());
  EXPECT_TRUE(specifics.preferred());
  EXPECT_TRUE(specifics.has_date_last_used());
  EXPECT_EQ(100, specifics.date_last_used());
  EXPECT_TRUE(specifics.has_date_created());
  EXPECT_EQ(100, specifics.date_created());
  EXPECT_TRUE(specifics.has_blacklisted());
  EXPECT_TRUE(specifics.blacklisted());
  EXPECT_TRUE(specifics.has_type());
  EXPECT_EQ(static_cast<int>(autofill::PasswordForm::Type::kMaxValue),
            specifics.type());
  EXPECT_TRUE(specifics.has_times_used());
  EXPECT_EQ(11, specifics.times_used());
  EXPECT_TRUE(specifics.has_display_name());
  EXPECT_EQ("Great Peter", specifics.display_name());
  EXPECT_TRUE(specifics.has_avatar_url());
  EXPECT_EQ("https://google.com/icon", specifics.avatar_url());
  EXPECT_TRUE(specifics.has_federation_url());
  EXPECT_EQ("https://google.com", specifics.federation_url());
}

}  // namespace

}  // namespace password_manager
