// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_sync_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model_impl/in_memory_metadata_change_list.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::NotNull;
using testing::Return;
using testing::UnorderedElementsAre;

constexpr char kSignonRealm1[] = "abc";
constexpr char kSignonRealm2[] = "def";
constexpr char kSignonRealm3[] = "xyz";

// |*arg| must be of type EntityData.
MATCHER_P(EntityDataHasSignonRealm, expected_signon_realm, "") {
  return arg->specifics.password()
             .client_only_encrypted_data()
             .signon_realm() == expected_signon_realm;
}

// |*arg| must be of type PasswordForm.
MATCHER_P(FormHasSignonRealm, expected_signon_realm, "") {
  return arg.signon_realm == expected_signon_realm;
}

// |*arg| must be of type PasswordStoreChange.
MATCHER_P(ChangeHasPrimaryKey, expected_primary_key, "") {
  return arg.primary_key() == expected_primary_key;
}

// |*arg| must be of type SyncMetadataStoreChangeList.
MATCHER_P(IsSyncMetadataStoreChangeListWithStore, expected_metadata_store, "") {
  return static_cast<const syncer::SyncMetadataStoreChangeList*>(arg)
             ->GetMetadataStoreForTesting() == expected_metadata_store;
}

sync_pb::PasswordSpecifics CreateSpecifics(const std::string& origin,
                                           const std::string& username_element,
                                           const std::string& username_value,
                                           const std::string& password_element,
                                           const std::string& signon_realm) {
  sync_pb::EntitySpecifics password_specifics;
  sync_pb::PasswordSpecificsData* password_data =
      password_specifics.mutable_password()
          ->mutable_client_only_encrypted_data();
  password_data->set_origin(origin);
  password_data->set_username_element(username_element);
  password_data->set_username_value(username_value);
  password_data->set_password_element(password_element);
  password_data->set_signon_realm(signon_realm);
  return password_specifics.password();
}

sync_pb::PasswordSpecifics CreateSpecificsWithSignonRealm(
    const std::string& signon_realm) {
  return CreateSpecifics("http://www.origin.com", "username_element",
                         "username_value", "password_element", signon_realm);
}

autofill::PasswordForm MakePasswordForm(const std::string& signon_realm) {
  autofill::PasswordForm form;
  form.origin = GURL("http://www.origin.com");
  form.username_element = base::UTF8ToUTF16("username_element");
  form.username_value = base::UTF8ToUTF16("username_value");
  form.password_element = base::UTF8ToUTF16("password_element");
  form.signon_realm = signon_realm;
  return form;
}

// A mini database class the supports Add/Update/Remove functionality. It also
// supports an auto increment primary key that starts from 1. It will be used to
// empower the MockPasswordStoreSync be forwarding all database calls to an
// instance of this class.
class FakeDatabase {
 public:
  FakeDatabase() = default;
  ~FakeDatabase() = default;

  FormRetrievalResult ReadAllLogins(PrimaryKeyToFormMap* map) {
    map->clear();
    for (const auto& pair : data_) {
      map->emplace(pair.first,
                   std::make_unique<autofill::PasswordForm>(*pair.second));
    }
    return FormRetrievalResult::kSuccess;
  }

  PasswordStoreChangeList AddLogin(const autofill::PasswordForm& form,
                                   AddLoginError* error) {
    if (error) {
      *error = error_;
    }
    if (error_ == AddLoginError::kNone) {
      data_[primary_key_] = std::make_unique<autofill::PasswordForm>(form);
      return {
          PasswordStoreChange(PasswordStoreChange::ADD, form, primary_key_++)};
    }
    return PasswordStoreChangeList();
  }

  PasswordStoreChangeList AddLoginForPrimaryKey(
      int primary_key,
      const autofill::PasswordForm& form) {
    DCHECK_EQ(0U, data_.count(primary_key));
    data_[primary_key] = std::make_unique<autofill::PasswordForm>(form);
    return {PasswordStoreChange(PasswordStoreChange::ADD, form, primary_key)};
  }

  PasswordStoreChangeList UpdateLogin(const autofill::PasswordForm& form,
                                      UpdateLoginError* error) {
    if (error) {
      *error = UpdateLoginError::kNone;
    }
    int key = GetPrimaryKey(form);
    DCHECK_NE(-1, key);
    data_[key] = std::make_unique<autofill::PasswordForm>(form);
    return {PasswordStoreChange(PasswordStoreChange::UPDATE, form, key)};
  }

  PasswordStoreChangeList RemoveLogin(int key) {
    DCHECK_NE(0U, data_.count(key));
    autofill::PasswordForm form = *data_[key];
    data_.erase(key);
    return {PasswordStoreChange(PasswordStoreChange::REMOVE, form, key)};
  }

  void SetAddLoginError(AddLoginError error) { error_ = error; }

 private:
  int GetPrimaryKey(const autofill::PasswordForm& form) const {
    for (const auto& pair : data_) {
      if (ArePasswordFormUniqueKeysEqual(*pair.second, form)) {
        return pair.first;
      }
    }
    return -1;
  }

  int primary_key_ = 1;
  PrimaryKeyToFormMap data_;
  AddLoginError error_ = AddLoginError::kNone;

  DISALLOW_COPY_AND_ASSIGN(FakeDatabase);
};

class MockSyncMetadataStore : public PasswordStoreSync::MetadataStore {
 public:
  MockSyncMetadataStore() = default;
  ~MockSyncMetadataStore() = default;

  MOCK_METHOD0(GetAllSyncMetadata, std::unique_ptr<syncer::MetadataBatch>());
  MOCK_METHOD0(DeleteAllSyncMetadata, void());
  MOCK_METHOD3(UpdateSyncMetadata,
               bool(syncer::ModelType,
                    const std::string&,
                    const sync_pb::EntityMetadata&));
  MOCK_METHOD2(ClearSyncMetadata, bool(syncer::ModelType, const std::string&));
  MOCK_METHOD2(UpdateModelTypeState,
               bool(syncer::ModelType, const sync_pb::ModelTypeState&));
  MOCK_METHOD1(ClearModelTypeState, bool(syncer::ModelType));
};

class MockPasswordStoreSync : public PasswordStoreSync {
 public:
  MockPasswordStoreSync() = default;
  ~MockPasswordStoreSync() = default;

  MOCK_METHOD1(FillAutofillableLogins,
               bool(std::vector<std::unique_ptr<autofill::PasswordForm>>*));
  MOCK_METHOD1(FillBlacklistLogins,
               bool(std::vector<std::unique_ptr<autofill::PasswordForm>>*));
  MOCK_METHOD1(ReadAllLogins, FormRetrievalResult(PrimaryKeyToFormMap*));
  MOCK_METHOD1(RemoveLoginByPrimaryKeySync, PasswordStoreChangeList(int));
  MOCK_METHOD0(DeleteUndecryptableLogins, DatabaseCleanupResult());
  MOCK_METHOD2(AddLoginSync,
               PasswordStoreChangeList(const autofill::PasswordForm&,
                                       AddLoginError*));
  MOCK_METHOD2(UpdateLoginSync,
               PasswordStoreChangeList(const autofill::PasswordForm&,
                                       UpdateLoginError*));
  MOCK_METHOD1(RemoveLoginSync,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD1(NotifyLoginsChanged, void(const PasswordStoreChangeList&));
  MOCK_METHOD0(BeginTransaction, bool());
  MOCK_METHOD0(CommitTransaction, bool());
  MOCK_METHOD0(RollbackTransaction, void());
  MOCK_METHOD0(GetMetadataStore, PasswordStoreSync::MetadataStore*());
  MOCK_CONST_METHOD0(IsAccountStore, bool());
  MOCK_METHOD0(DeleteAndRecreateDatabaseFile, bool());
};

}  // namespace

class PasswordSyncBridgeTest : public testing::Test {
 public:
  PasswordSyncBridgeTest() {
    ON_CALL(mock_password_store_sync_, GetMetadataStore())
        .WillByDefault(testing::Return(&mock_sync_metadata_store_sync_));
    ON_CALL(mock_password_store_sync_, ReadAllLogins(_))
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::ReadAllLogins));
    ON_CALL(mock_password_store_sync_, AddLoginSync(_, _))
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::AddLogin));
    ON_CALL(mock_password_store_sync_, UpdateLoginSync(_, _))
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::UpdateLogin));
    ON_CALL(mock_password_store_sync_, RemoveLoginByPrimaryKeySync(_))
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::RemoveLogin));

    bridge_ = std::make_unique<PasswordSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &mock_password_store_sync_,
        sync_enabled_or_disabled_cb_.Get());

    // It's the responsibility of the PasswordStoreSync to inform the bridge
    // about changes in the password store. The bridge notifies the
    // PasswordStoreSync about the new changes even if they are initiated by the
    // bridge itself.
    ON_CALL(mock_password_store_sync_, NotifyLoginsChanged(_))
        .WillByDefault(
            Invoke(bridge(), &PasswordSyncBridge::ActOnPasswordStoreChanges));

    ON_CALL(mock_sync_metadata_store_sync_, GetAllSyncMetadata())
        .WillByDefault(
            []() { return std::make_unique<syncer::MetadataBatch>(); });
    ON_CALL(mock_sync_metadata_store_sync_, UpdateSyncMetadata(_, _, _))
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_sync_metadata_store_sync_, ClearSyncMetadata(_, _))
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_sync_metadata_store_sync_, UpdateModelTypeState(_, _))
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_sync_metadata_store_sync_, ClearModelTypeState(_))
        .WillByDefault(testing::Return(true));
  }

  // Creates an EntityData around a copy of the given specifics.
  std::unique_ptr<syncer::EntityData> SpecificsToEntity(
      const sync_pb::PasswordSpecifics& specifics) {
    auto data = std::make_unique<syncer::EntityData>();
    *data->specifics.mutable_password() = specifics;
    data->client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::PASSWORDS, bridge()->GetClientTag(*data));
    return data;
  }

  ~PasswordSyncBridgeTest() override {}

  base::Optional<sync_pb::PasswordSpecifics> GetDataFromBridge(
      const std::string& storage_key) {
    std::unique_ptr<syncer::DataBatch> batch;
    bridge_->GetData({storage_key},
                     base::BindLambdaForTesting(
                         [&](std::unique_ptr<syncer::DataBatch> in_batch) {
                           batch = std::move(in_batch);
                         }));
    EXPECT_THAT(batch, NotNull());
    if (!batch || !batch->HasNext()) {
      return base::nullopt;
    }
    const syncer::KeyAndData& data_pair = batch->Next();
    EXPECT_THAT(data_pair.first, Eq(storage_key));
    EXPECT_FALSE(batch->HasNext());
    return data_pair.second->specifics.password();
  }

  FakeDatabase* fake_db() { return &fake_db_; }

  PasswordSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  MockSyncMetadataStore* mock_sync_metadata_store_sync() {
    return &mock_sync_metadata_store_sync_;
  }

  MockPasswordStoreSync* mock_password_store_sync() {
    return &mock_password_store_sync_;
  }

  base::MockRepeatingClosure* mock_sync_enabled_or_disabled_cb() {
    return &sync_enabled_or_disabled_cb_;
  }

 private:
  FakeDatabase fake_db_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  testing::NiceMock<MockSyncMetadataStore> mock_sync_metadata_store_sync_;
  testing::NiceMock<MockPasswordStoreSync> mock_password_store_sync_;
  testing::NiceMock<base::MockRepeatingClosure> sync_enabled_or_disabled_cb_;
  std::unique_ptr<PasswordSyncBridge> bridge_;
};

TEST_F(PasswordSyncBridgeTest, ShouldComputeClientTagHash) {
  syncer::EntityData data;
  *data.specifics.mutable_password() =
      CreateSpecifics("http://www.origin.com", "username_element",
                      "username_value", "password_element", "signon_realm");

  EXPECT_THAT(
      bridge()->GetClientTag(data),
      Eq("http%3A//www.origin.com/"
         "|username_element|username_value|password_element|signon_realm"));
}

TEST_F(PasswordSyncBridgeTest, ShouldForwardLocalChangesToTheProcessor) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));

  PasswordStoreChangeList changes;
  changes.push_back(PasswordStoreChange(
      PasswordStoreChange::ADD, MakePasswordForm(kSignonRealm1), /*id=*/1));
  changes.push_back(PasswordStoreChange(
      PasswordStoreChange::UPDATE, MakePasswordForm(kSignonRealm2), /*id=*/2));
  changes.push_back(PasswordStoreChange(
      PasswordStoreChange::REMOVE, MakePasswordForm(kSignonRealm3), /*id=*/3));
  PasswordStoreSync::MetadataStore* store =
      mock_password_store_sync()->GetMetadataStore();
  EXPECT_CALL(mock_processor(),
              Put("1", EntityDataHasSignonRealm(kSignonRealm1),
                  IsSyncMetadataStoreChangeListWithStore(store)));
  EXPECT_CALL(mock_processor(),
              Put("2", EntityDataHasSignonRealm(kSignonRealm2),
                  IsSyncMetadataStoreChangeListWithStore(store)));
  EXPECT_CALL(mock_processor(),
              Delete("3", IsSyncMetadataStoreChangeListWithStore(store)));

  bridge()->ActOnPasswordStoreChanges(changes);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldNotForwardLocalChangesToTheProcessorIfSyncDisabled) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(false));

  PasswordStoreChangeList changes;
  changes.push_back(PasswordStoreChange(
      PasswordStoreChange::ADD, MakePasswordForm(kSignonRealm1), /*id=*/1));
  changes.push_back(PasswordStoreChange(
      PasswordStoreChange::UPDATE, MakePasswordForm(kSignonRealm2), /*id=*/2));
  changes.push_back(PasswordStoreChange(
      PasswordStoreChange::REMOVE, MakePasswordForm(kSignonRealm3), /*id=*/3));

  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);

  bridge()->ActOnPasswordStoreChanges(changes);
}

TEST_F(PasswordSyncBridgeTest, ShouldApplyEmptySyncChangesWithoutError) {
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), syncer::EntityChangeList());
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldApplyMetadataWithEmptySyncChanges) {
  const std::string kStorageKey = "1";
  const std::string kServerId = "TestServerId";
  sync_pb::EntityMetadata metadata;
  metadata.set_server_id(kServerId);
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  metadata_change_list->UpdateMetadata(kStorageKey, metadata);

  EXPECT_CALL(*mock_password_store_sync(), NotifyLoginsChanged(_)).Times(0);

  EXPECT_CALL(*mock_sync_metadata_store_sync(),
              UpdateSyncMetadata(syncer::PASSWORDS, kStorageKey, _));

  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      std::move(metadata_change_list), syncer::EntityChangeList());
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldApplyRemoteCreation) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  // Since this remote creation is the first entry in the FakeDatabase, it will
  // be assigned a primary key 1.
  const std::string kStorageKey = "1";

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealm(kSignonRealm1);

  testing::InSequence in_sequence;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              AddLoginSync(FormHasSignonRealm(kSignonRealm1), _));
  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, kStorageKey, _));
  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction());
  EXPECT_CALL(
      *mock_password_store_sync(),
      NotifyLoginsChanged(UnorderedElementsAre(ChangeHasPrimaryKey(1))));

  // Processor shouldn't be notified about remote changes.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldIgnoreAndUntrackRemoteCreationWithInvalidData) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  fake_db()->SetAddLoginError(AddLoginError::kConstraintViolation);

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealm(kSignonRealm1);

  EXPECT_CALL(mock_processor(),
              UntrackEntityForClientTagHash(
                  SpecificsToEntity(specifics)->client_tag_hash));

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldApplyRemoteUpdate) {
  const int kPrimaryKey = 1000;
  const std::string kStorageKey = "1000";
  // Add the form to the DB.
  fake_db()->AddLoginForPrimaryKey(kPrimaryKey,
                                   MakePasswordForm(kSignonRealm1));

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealm(kSignonRealm1);

  testing::InSequence in_sequence;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              UpdateLoginSync(FormHasSignonRealm(kSignonRealm1), _));
  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              NotifyLoginsChanged(
                  UnorderedElementsAre(ChangeHasPrimaryKey(kPrimaryKey))));

  // Processor shouldn't be notified about remote changes.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, _, _)).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateUpdate(
      kStorageKey, SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldApplyRemoteDeletion) {
  const int kPrimaryKey = 1000;
  const std::string kStorageKey = "1000";
  // Add the form to the DB.
  fake_db()->AddLoginForPrimaryKey(kPrimaryKey,
                                   MakePasswordForm(kSignonRealm1));

  testing::InSequence in_sequence;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              RemoveLoginByPrimaryKeySync(kPrimaryKey));
  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              NotifyLoginsChanged(
                  UnorderedElementsAre(ChangeHasPrimaryKey(kPrimaryKey))));

  // Processor shouldn't be notified about remote changes.
  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateDelete(kStorageKey));
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldGetDataForStorageKey) {
  const int kPrimaryKey1 = 1000;
  const int kPrimaryKey2 = 1001;
  const std::string kPrimaryKeyStr1 = "1000";
  const std::string kPrimaryKeyStr2 = "1001";
  autofill::PasswordForm form1 = MakePasswordForm(kSignonRealm1);
  autofill::PasswordForm form2 = MakePasswordForm(kSignonRealm2);

  fake_db()->AddLoginForPrimaryKey(kPrimaryKey1, form1);
  fake_db()->AddLoginForPrimaryKey(kPrimaryKey2, form2);

  base::Optional<sync_pb::PasswordSpecifics> optional_specifics =
      GetDataFromBridge(/*storage_key=*/kPrimaryKeyStr1);
  ASSERT_TRUE(optional_specifics.has_value());
  EXPECT_EQ(
      kSignonRealm1,
      optional_specifics.value().client_only_encrypted_data().signon_realm());

  optional_specifics = GetDataFromBridge(/*storage_key=*/kPrimaryKeyStr2);
  ASSERT_TRUE(optional_specifics.has_value());
  EXPECT_EQ(kSignonRealm2,
            optional_specifics->client_only_encrypted_data().signon_realm());
}

TEST_F(PasswordSyncBridgeTest, ShouldNotGetDataForNonExistingStorageKey) {
  const std::string kPrimaryKeyStr = "1";

  base::Optional<sync_pb::PasswordSpecifics> optional_specifics =
      GetDataFromBridge(/*storage_key=*/kPrimaryKeyStr);
  EXPECT_FALSE(optional_specifics.has_value());
}

TEST_F(PasswordSyncBridgeTest, ShouldMergeSyncRemoteAndLocalPasswords) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  // Setup the test to have Form 1 and Form 2 stored locally, and Form 2 and
  // Form 3 coming as remote changes. We will assign primary keys for Form 1 and
  // Form 2. Form 3 will arrive as remote creation, and FakeDatabase will assign
  // it primary key 1.
  const int kPrimaryKey1 = 1000;
  const int kPrimaryKey2 = 1001;
  const int kExpectedPrimaryKey3 = 1;
  const std::string kPrimaryKeyStr1 = "1000";
  const std::string kPrimaryKeyStr2 = "1001";
  const std::string kExpectedPrimaryKeyStr3 = "1";
  autofill::PasswordForm form1 = MakePasswordForm(kSignonRealm1);
  autofill::PasswordForm form2 = MakePasswordForm(kSignonRealm2);
  autofill::PasswordForm form3 = MakePasswordForm(kSignonRealm3);
  sync_pb::PasswordSpecifics specifics1 =
      CreateSpecificsWithSignonRealm(kSignonRealm1);
  sync_pb::PasswordSpecifics specifics2 =
      CreateSpecificsWithSignonRealm(kSignonRealm2);
  sync_pb::PasswordSpecifics specifics3 =
      CreateSpecificsWithSignonRealm(kSignonRealm3);

  fake_db()->AddLoginForPrimaryKey(kPrimaryKey1, form1);
  fake_db()->AddLoginForPrimaryKey(kPrimaryKey2, form2);

  // Form 1 will be added to the change processor. The local version of Form 2
  // isn't more recent than the remote version, therefore it  will be updated in
  // the password sync store using the remote version. Form 3 will be added to
  // the password store sync.

  // Interactions should happen in this order:
  //           +--> Put(1) ------------------------------------+
  //           |                                               |
  //           |--> UpdateStorageKey(2) -----------------------|
  // Begin() --|                                               |--> Commit()
  //           |--> UpdateLoginSync(3) ------------------------|
  //           |                                               |
  //           +--> AddLoginSync (4) ---> UpdateStorageKey(4)--+

  testing::Sequence s1, s2, s3, s4;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction())
      .InSequence(s1, s2, s3, s4);
  EXPECT_CALL(mock_processor(),
              Put(kPrimaryKeyStr1, EntityDataHasSignonRealm(kSignonRealm1), _))
      .InSequence(s1);

  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, kPrimaryKeyStr2, _))
      .InSequence(s2);

  EXPECT_CALL(*mock_password_store_sync(),
              UpdateLoginSync(FormHasSignonRealm(kSignonRealm2), _))
      .InSequence(s3);

  EXPECT_CALL(*mock_password_store_sync(),
              AddLoginSync(FormHasSignonRealm(kSignonRealm3), _))
      .InSequence(s4);
  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, kExpectedPrimaryKeyStr3, _))
      .InSequence(s4);

  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction())
      .InSequence(s1, s2, s3, s4);

  EXPECT_CALL(*mock_password_store_sync(),
              NotifyLoginsChanged(UnorderedElementsAre(
                  ChangeHasPrimaryKey(kPrimaryKey2),
                  ChangeHasPrimaryKey(kExpectedPrimaryKey3))))
      .InSequence(s1, s2, s3, s4);

  // Processor shouldn't be informed about Form 2 or Form 3.
  EXPECT_CALL(mock_processor(), Put(kPrimaryKeyStr2, _, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(kExpectedPrimaryKeyStr3, _, _)).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics2)));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics3)));

  base::Optional<syncer::ModelError> error = bridge()->MergeSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldMergeSyncRemoteAndLocalPasswordsChoosingTheMoreRecent) {
  // Setup the test to have Form 1 and Form 2 stored locally and remotely. Local
  // Form 1 is more recent than the remote one. Remote Form 2 is more recent
  // than the local one. We will assign primary keys for Form 1 and Form 2 in
  // the local DB.
  base::Time now = base::Time::Now();
  base::Time yesterday = now - base::TimeDelta::FromDays(1);
  const int kPrimaryKey1 = 1000;
  const int kPrimaryKey2 = 1001;
  const std::string kPrimaryKeyStr1 = "1000";
  const std::string kPrimaryKeyStr2 = "1001";

  // Local form 1 is more recent than the remote.
  autofill::PasswordForm form1 = MakePasswordForm(kSignonRealm1);
  form1.date_created = now;
  sync_pb::PasswordSpecifics specifics1 =
      CreateSpecificsWithSignonRealm(kSignonRealm1);
  specifics1.mutable_client_only_encrypted_data()->set_date_created(
      yesterday.ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Remote form 2 is more recent than the local.
  autofill::PasswordForm form2 = MakePasswordForm(kSignonRealm2);
  form2.date_created = yesterday;
  sync_pb::PasswordSpecifics specifics2 =
      CreateSpecificsWithSignonRealm(kSignonRealm2);
  specifics2.mutable_client_only_encrypted_data()->set_date_created(
      now.ToDeltaSinceWindowsEpoch().InMicroseconds());

  fake_db()->AddLoginForPrimaryKey(kPrimaryKey1, form1);
  fake_db()->AddLoginForPrimaryKey(kPrimaryKey2, form2);

  // The processor should be informed about the storage keys of both passwords.
  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, kPrimaryKeyStr1, _));
  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, kPrimaryKeyStr2, _));

  // Since local Form 1 is more recent, it will be put() in the processor.
  EXPECT_CALL(mock_processor(),
              Put(kPrimaryKeyStr1, EntityDataHasSignonRealm(kSignonRealm1), _));

  // Since the remote Form 2 is more recent, it will be updated in the password
  // store.
  EXPECT_CALL(*mock_password_store_sync(),
              UpdateLoginSync(FormHasSignonRealm(kSignonRealm2), _));
  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics1)));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics2)));
  base::Optional<syncer::ModelError> error = bridge()->MergeSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

// This tests that if reading logins from the store fails,
// ShouldMergeSync() would return an error without crashing.
TEST_F(PasswordSyncBridgeTest,
       ShouldMergeSyncRemoteAndLocalPasswordsWithErrorWhenStoreReadFails) {
  // Simulate a failed ReadAllLogins() by returning a kDbError.
  ON_CALL(*mock_password_store_sync(), ReadAllLogins(_))
      .WillByDefault(testing::Return(FormRetrievalResult::kDbError));
  base::Optional<syncer::ModelError> error =
      bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(), {});
  EXPECT_TRUE(error);
}

// This tests that if adding logins to the store fails,
// ShouldMergeSync() would return an error without crashing.
TEST_F(PasswordSyncBridgeTest,
       ShouldMergeSyncRemoteAndLocalPasswordsWithErrorWhenStoreAddFails) {
  fake_db()->SetAddLoginError(AddLoginError::kDbError);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"",
      SpecificsToEntity(CreateSpecificsWithSignonRealm(kSignonRealm1))));

  EXPECT_CALL(*mock_password_store_sync(), RollbackTransaction());
  base::Optional<syncer::ModelError> error = bridge()->MergeSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_TRUE(error);
}

// This tests that if storing model type state fails,
// ShouldMergeSync() would return an error without crashing.
TEST_F(
    PasswordSyncBridgeTest,
    ShouldMergeSyncRemoteAndLocalPasswordsWithErrorWhenStoreUpdateModelTypeStateFails) {
  // Simulate failure in UpdateModelTypeState();
  ON_CALL(*mock_sync_metadata_store_sync(), UpdateModelTypeState(_, _))
      .WillByDefault(testing::Return(false));

  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_done(true);

  std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();
  metadata_changes->UpdateModelTypeState(model_type_state);

  EXPECT_CALL(*mock_password_store_sync(), RollbackTransaction());
  base::Optional<syncer::ModelError> error =
      bridge()->MergeSyncData(std::move(metadata_changes), {});
  EXPECT_TRUE(error);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldMergeAndIgnoreAndUntrackRemotePasswordWithInvalidData) {
  fake_db()->SetAddLoginError(AddLoginError::kConstraintViolation);

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealm(kSignonRealm1);

  EXPECT_CALL(mock_processor(),
              UntrackEntityForClientTagHash(
                  SpecificsToEntity(specifics)->client_tag_hash));

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->MergeSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldGetAllDataForDebuggingWithHiddenPassword) {
  const int kPrimaryKey1 = 1000;
  const int kPrimaryKey2 = 1001;
  autofill::PasswordForm form1 = MakePasswordForm(kSignonRealm1);
  autofill::PasswordForm form2 = MakePasswordForm(kSignonRealm2);

  fake_db()->AddLoginForPrimaryKey(kPrimaryKey1, form1);
  fake_db()->AddLoginForPrimaryKey(kPrimaryKey2, form2);

  std::unique_ptr<syncer::DataBatch> batch;

  bridge()->GetAllDataForDebugging(base::BindLambdaForTesting(
      [&](std::unique_ptr<syncer::DataBatch> in_batch) {
        batch = std::move(in_batch);
      }));

  ASSERT_THAT(batch, NotNull());
  EXPECT_TRUE(batch->HasNext());
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    EXPECT_EQ("hidden", data_pair.second->specifics.password()
                            .client_only_encrypted_data()
                            .password_value());
  }
}

TEST_F(PasswordSyncBridgeTest,
       ShouldCallModelReadyUponConstructionWithMetadata) {
  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        sync_pb::ModelTypeState model_type_state;
        model_type_state.set_initial_sync_done(true);
        auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
        metadata_batch->SetModelTypeState(model_type_state);
        metadata_batch->AddMetadata(
            "storage_key", std::make_unique<sync_pb::EntityMetadata>());
        return metadata_batch;
      });

  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/syncer::HasInitialSyncDone(),
                                    /*entities=*/testing::SizeIs(1))));

  PasswordSyncBridge bridge(mock_processor().CreateForwardingProcessor(),
                            mock_password_store_sync(), base::DoNothing());
}

// Tests that in case ReadAllLogins() during initial merge returns encryption
// service failure, the bridge would try to do a DB clean up.
TEST_F(PasswordSyncBridgeTest, ShouldDeleteUndecryptableLoginsDuringMerge) {
  ON_CALL(*mock_password_store_sync(), DeleteUndecryptableLogins())
      .WillByDefault(Return(DatabaseCleanupResult::kSuccess));

  // We should try to read first, and simulate an encryption failure. Then,
  // cleanup the database and try to read again which should be successful now.
  EXPECT_CALL(*mock_password_store_sync(), ReadAllLogins(_))
      .WillOnce(Return(FormRetrievalResult::kEncrytionServiceFailure))
      .WillOnce(Return(FormRetrievalResult::kSuccess));
  EXPECT_CALL(*mock_password_store_sync(), DeleteUndecryptableLogins());

  base::Optional<syncer::ModelError> error =
      bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(), {});
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldDeleteSyncMetadataWhenApplyStopSyncChanges) {
  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata());
  bridge()->ApplyStopSyncChanges(bridge()->CreateMetadataChangeList());
}

TEST_F(PasswordSyncBridgeTest, ShouldNotifyOnSyncEnable) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(true));

  // New password data becoming available because sync was newly enabled should
  // trigger the callback.
  EXPECT_CALL(*mock_sync_enabled_or_disabled_cb(), Run());

  syncer::EntityChangeList initial_entity_data;
  initial_entity_data.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"",
      SpecificsToEntity(CreateSpecificsWithSignonRealm(kSignonRealm1))));

  base::Optional<syncer::ModelError> error = bridge()->MergeSyncData(
      bridge()->CreateMetadataChangeList(), std::move(initial_entity_data));
  ASSERT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldNotNotifyOnSyncChange) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(true));

  // New password data becoming available due to an incoming sync change should
  // *not* trigger the callback. This is mainly for performance reasons: In
  // practice, this callback will cause all PasswordFormManagers to re-query
  // from the password store, which can be expensive.
  EXPECT_CALL(*mock_sync_enabled_or_disabled_cb(), Run()).Times(0);

  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"",
      SpecificsToEntity(CreateSpecificsWithSignonRealm(kSignonRealm1))));

  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_changes));
  ASSERT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldNotifyOnSyncDisableIfAccountStore) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(true));

  // The account password store gets cleared when sync is disabled, so this
  // should trigger the callback.
  EXPECT_CALL(*mock_sync_enabled_or_disabled_cb(), Run());

  bridge()->ApplyStopSyncChanges(bridge()->CreateMetadataChangeList());
}

TEST_F(PasswordSyncBridgeTest, ShouldNotNotifyOnSyncDisableIfProfileStore) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(false));

  // The profile password store does *not* get cleared when sync is disabled, so
  // this should *not* trigger the callback.
  EXPECT_CALL(*mock_sync_enabled_or_disabled_cb(), Run()).Times(0);

  bridge()->ApplyStopSyncChanges(bridge()->CreateMetadataChangeList());
}

}  // namespace password_manager
