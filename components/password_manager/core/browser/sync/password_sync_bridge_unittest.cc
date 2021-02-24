// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_sync_bridge.h"

#include <memory>
#include <random>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/test/model/mock_model_type_change_processor.h"
#include "components/sync/test/model/test_matchers.h"
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
using testing::UnorderedElementsAreArray;

constexpr char kSignonRealm1[] = "abc";
constexpr char kSignonRealm2[] = "def";
constexpr char kSignonRealm3[] = "xyz";

// |*arg| must be of type EntityData.
MATCHER_P(EntityDataHasSignonRealm, expected_signon_realm, "") {
  return arg->specifics.password()
             .client_only_encrypted_data()
             .signon_realm() == expected_signon_realm;
}

bool SpecificsHasExpectedInsecureTypes(
    const sync_pb::PasswordSpecificsData::PasswordIssues& specifics,
    const std::vector<InsecureType>& expected_types) {
  return base::ranges::all_of(expected_types, [&specifics](auto type) {
    switch (type) {
      case InsecureType::kLeaked:
        return specifics.has_leaked_password_issue();
      case InsecureType::kPhished:
        return specifics.has_phished_password_issue();
      case InsecureType::kWeak:
        return specifics.has_weak_password_issue();
      case InsecureType::kReused:
        return specifics.has_reused_password_issue();
    }
  });
}

MATCHER_P(EntityDataHasSecurityIssueTypes, expected_issue_types, "") {
  const auto& password_issues_data =
      arg->specifics.password().client_only_encrypted_data().password_issues();
  return SpecificsHasExpectedInsecureTypes(password_issues_data,
                                           expected_issue_types);
}

// |*arg| must be of type PasswordForm.
MATCHER_P(FormHasSignonRealm, expected_signon_realm, "") {
  return arg.signon_realm == expected_signon_realm;
}

// |*arg| must be of type PasswordStoreChange.
MATCHER_P(ChangeHasPrimaryKey, expected_primary_key, "") {
  return arg.primary_key().value() == expected_primary_key;
}

// |*arg| must be of type SyncMetadataStoreChangeList.
MATCHER_P(IsSyncMetadataStoreChangeListWithStore, expected_metadata_store, "") {
  return static_cast<const syncer::SyncMetadataStoreChangeList*>(arg)
             ->GetMetadataStoreForTesting() == expected_metadata_store;
}

sync_pb::PasswordSpecificsData_PasswordIssues CreateSpecificsIssues(
    const std::vector<InsecureType>& issue_types) {
  sync_pb::PasswordSpecificsData_PasswordIssues remote_issues;
  for (auto type : issue_types) {
    sync_pb::PasswordSpecificsData_PasswordIssues_PasswordIssue remote_issue;
    remote_issue.set_date_first_detection_microseconds(
        base::Time().ToDeltaSinceWindowsEpoch().InMicroseconds());
    remote_issue.set_is_muted(false);
    switch (type) {
      case InsecureType::kLeaked:
        *remote_issues.mutable_leaked_password_issue() = remote_issue;
        break;
      case InsecureType::kPhished:
        *remote_issues.mutable_phished_password_issue() = remote_issue;
        break;
      case InsecureType::kWeak:
        *remote_issues.mutable_weak_password_issue() = remote_issue;
        break;
      case InsecureType::kReused:
        *remote_issues.mutable_reused_password_issue() = remote_issue;
        break;
    }
  }
  return remote_issues;
}

sync_pb::PasswordSpecifics CreateSpecifics(
    const std::string& origin,
    const std::string& username_element,
    const std::string& username_value,
    const std::string& password_element,
    const std::string& signon_realm,
    const std::vector<InsecureType>& issue_types) {
  sync_pb::EntitySpecifics password_specifics;
  sync_pb::PasswordSpecificsData* password_data =
      password_specifics.mutable_password()
          ->mutable_client_only_encrypted_data();
  password_data->set_origin(origin);
  password_data->set_username_element(username_element);
  password_data->set_username_value(username_value);
  password_data->set_password_element(password_element);
  password_data->set_signon_realm(signon_realm);
  if (!issue_types.empty())
    *password_data->mutable_password_issues() =
        CreateSpecificsIssues(issue_types);
  return password_specifics.password();
}

sync_pb::PasswordSpecifics CreateSpecificsWithSignonRealm(
    const std::string& signon_realm) {
  return CreateSpecifics("http://www.origin.com/", "username_element",
                         "username_value", "password_element", signon_realm,
                         {});
}

sync_pb::PasswordSpecifics CreateSpecificsWithSignonRealmAndIssues(
    const std::string& signon_realm,
    const std::vector<InsecureType>& issue_types) {
  return CreateSpecifics("http://www.origin.com/", "username_element",
                         "username_value", "password_element", signon_realm,
                         issue_types);
}

PasswordForm MakePasswordForm(const std::string& signon_realm) {
  PasswordForm form;
  form.url = GURL("http://www.origin.com");
  form.username_element = base::UTF8ToUTF16("username_element");
  form.username_value = base::UTF8ToUTF16("username_value");
  form.password_element = base::UTF8ToUTF16("password_element");
  form.signon_realm = signon_realm;
  return form;
}

PasswordForm MakeBlocklistedForm(const std::string& signon_realm) {
  PasswordForm form;
  form.url = GURL("http://www.origin.com");
  form.signon_realm = signon_realm;
  form.blocked_by_user = true;
  return form;
}

std::vector<InsecureCredential> MakeInsecureCredentials(
    const PasswordForm& form,
    const std::vector<InsecureType>& types) {
  std::vector<InsecureCredential> issues;

  for (auto type : types) {
    issues.emplace_back(InsecureCredential(form.signon_realm,
                                           form.username_value, base::Time(),
                                           type, IsMuted(false)));
  }
  return issues;
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
      map->emplace(pair.first, std::make_unique<PasswordForm>(*pair.second));
    }
    return FormRetrievalResult::kSuccess;
  }

  std::vector<InsecureCredential> ReadSecurityIssues(
      FormPrimaryKey parent_key) {
    return security_issues_[parent_key];
  }

  PasswordStoreChangeList AddLogin(const PasswordForm& form,
                                   AddLoginError* error) {
    if (error) {
      *error = error_;
    }
    if (error_ == AddLoginError::kNone) {
      data_[FormPrimaryKey(primary_key_)] =
          std::make_unique<PasswordForm>(form);
      return {PasswordStoreChange(PasswordStoreChange::ADD, form,
                                  FormPrimaryKey(primary_key_++))};
    }
    return PasswordStoreChangeList();
  }

  bool AddInsecureCredentials(base::span<const InsecureCredential> issues) {
    if (issues.empty())
      return true;
    FormPrimaryKey primary_key = GetPrimaryKey(issues[0]);
    DCHECK_NE(primary_key.value(), -1);
    DCHECK(!base::Contains(security_issues_, primary_key));
    for (const auto& issue : issues)
      security_issues_[primary_key].push_back(issue);
    return true;
  }

  PasswordStoreChangeList AddLoginForPrimaryKey(int primary_key,
                                                const PasswordForm& form) {
    FormPrimaryKey form_primary_key(primary_key);
    DCHECK_EQ(0U, data_.count(form_primary_key));
    data_[form_primary_key] = std::make_unique<PasswordForm>(form);
    return {
        PasswordStoreChange(PasswordStoreChange::ADD, form, form_primary_key)};
  }

  PasswordStoreChangeList UpdateLogin(const PasswordForm& form,
                                      UpdateLoginError* error) {
    if (error) {
      *error = UpdateLoginError::kNone;
    }
    FormPrimaryKey key = GetPrimaryKey(form);
    DCHECK_NE(-1, key.value());
    data_[key] = std::make_unique<PasswordForm>(form);
    return {PasswordStoreChange(PasswordStoreChange::UPDATE, form, key)};
  }

  bool UpdateInsecureCredentialsSync(
      const PasswordForm& form,
      base::span<const InsecureCredential> credentials) {
    FormPrimaryKey key = GetPrimaryKey(form);
    if (key.value() == -1)
      return false;
    security_issues_[key].assign(credentials.begin(), credentials.end());
    return true;
  }

  PasswordStoreChangeList RemoveLogin(FormPrimaryKey key) {
    DCHECK_NE(0U, data_.count(key));
    PasswordForm form = *data_[key];
    data_.erase(key);
    return {PasswordStoreChange(PasswordStoreChange::REMOVE, form, key)};
  }

  void SetAddLoginError(AddLoginError error) { error_ = error; }

 private:
  FormPrimaryKey GetPrimaryKey(const PasswordForm& form) const {
    for (const auto& pair : data_) {
      if (ArePasswordFormUniqueKeysEqual(*pair.second, form)) {
        return pair.first;
      }
    }
    return FormPrimaryKey(-1);
  }

  FormPrimaryKey GetPrimaryKey(const InsecureCredential& issue) const {
    for (const auto& pair : data_) {
      if (pair.second->username_value == issue.username &&
          pair.second->signon_realm == issue.signon_realm) {
        return pair.first;
      }
    }
    return FormPrimaryKey(-1);
  }

  int primary_key_ = 1;
  PrimaryKeyToFormMap data_;
  std::map<FormPrimaryKey, std::vector<InsecureCredential>> security_issues_;
  AddLoginError error_ = AddLoginError::kNone;

  DISALLOW_COPY_AND_ASSIGN(FakeDatabase);
};

class MockSyncMetadataStore : public PasswordStoreSync::MetadataStore {
 public:
  MOCK_METHOD(std::unique_ptr<syncer::MetadataBatch>,
              GetAllSyncMetadata,
              (),
              (override));
  MOCK_METHOD(void, DeleteAllSyncMetadata, (), (override));
  MOCK_METHOD(bool,
              UpdateSyncMetadata,
              (syncer::ModelType,
               const std::string&,
               const sync_pb::EntityMetadata&),
              (override));
  MOCK_METHOD(bool,
              ClearSyncMetadata,
              (syncer::ModelType, const std::string&),
              (override));
  MOCK_METHOD(bool,
              UpdateModelTypeState,
              (syncer::ModelType, const sync_pb::ModelTypeState&),
              (override));
  MOCK_METHOD(bool, ClearModelTypeState, (syncer::ModelType), (override));
  MOCK_METHOD(void,
              SetDeletionsHaveSyncedCallback,
              (base::RepeatingCallback<void(bool)>),
              (override));
  MOCK_METHOD(bool, HasUnsyncedDeletions, (), (override));
};

class MockPasswordStoreSync : public PasswordStoreSync {
 public:
  MOCK_METHOD(FormRetrievalResult,
              ReadAllLogins,
              (PrimaryKeyToFormMap*),
              (override));
  MOCK_METHOD(std::vector<InsecureCredential>,
              ReadSecurityIssues,
              (FormPrimaryKey),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              RemoveLoginByPrimaryKeySync,
              (FormPrimaryKey),
              (override));
  MOCK_METHOD(DatabaseCleanupResult, DeleteUndecryptableLogins, (), (override));
  MOCK_METHOD(PasswordStoreChangeList,
              AddLoginSync,
              (const PasswordForm&, AddLoginError*),
              (override));
  MOCK_METHOD(bool,
              AddInsecureCredentialsSync,
              (base::span<const InsecureCredential>),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              UpdateLoginSync,
              (const PasswordForm&, UpdateLoginError*),
              (override));
  MOCK_METHOD(bool,
              UpdateInsecureCredentialsSync,
              (const PasswordForm&, base::span<const InsecureCredential>),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              RemoveLoginSync,
              (const PasswordForm&),
              (override));
  MOCK_METHOD(void,
              NotifyLoginsChanged,
              (const PasswordStoreChangeList&),
              (override));
  MOCK_METHOD(void, NotifyInsecureCredentialsChanged, (), (override));
  MOCK_METHOD(void, NotifyDeletionsHaveSynced, (bool), (override));
  MOCK_METHOD(void,
              NotifyUnsyncedCredentialsWillBeDeleted,
              (std::vector<PasswordForm>),
              (override));
  MOCK_METHOD(bool, BeginTransaction, (), (override));
  MOCK_METHOD(bool, CommitTransaction, (), (override));
  MOCK_METHOD(void, RollbackTransaction, (), (override));
  MOCK_METHOD(PasswordStoreSync::MetadataStore*,
              GetMetadataStore,
              (),
              (override));
  MOCK_METHOD(bool, IsAccountStore, (), (const override));
  MOCK_METHOD(bool, DeleteAndRecreateDatabaseFile, (), (override));
};

}  // namespace

class PasswordSyncBridgeTest : public testing::Test,
                               public ::testing::WithParamInterface<bool> {
 public:
  PasswordSyncBridgeTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          password_manager::features::kSyncingCompromisedCredentials);
    } else {
      feature_list_.InitAndDisableFeature(
          password_manager::features::kSyncingCompromisedCredentials);
    }

    ON_CALL(mock_password_store_sync_, GetMetadataStore())
        .WillByDefault(testing::Return(&mock_sync_metadata_store_sync_));
    ON_CALL(mock_password_store_sync_, ReadAllLogins)
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::ReadAllLogins));
    ON_CALL(mock_password_store_sync_, ReadSecurityIssues)
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::ReadSecurityIssues));
    ON_CALL(mock_password_store_sync_, AddLoginSync)
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::AddLogin));
    ON_CALL(mock_password_store_sync_, AddInsecureCredentialsSync)
        .WillByDefault(
            Invoke(&fake_db_, &FakeDatabase::AddInsecureCredentials));
    ON_CALL(mock_password_store_sync_, UpdateLoginSync)
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::UpdateLogin));
    ON_CALL(mock_password_store_sync_, UpdateInsecureCredentialsSync)
        .WillByDefault(
            Invoke(&fake_db_, &FakeDatabase::UpdateInsecureCredentialsSync));
    ON_CALL(mock_password_store_sync_, RemoveLoginByPrimaryKeySync)
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::RemoveLogin));

    bridge_ = std::make_unique<PasswordSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &mock_password_store_sync_,
        sync_enabled_or_disabled_cb_.Get());

    // It's the responsibility of the PasswordStoreSync to inform the bridge
    // about changes in the password store. The bridge notifies the
    // PasswordStoreSync about the new changes even if they are initiated by the
    // bridge itself.
    ON_CALL(mock_password_store_sync_, NotifyLoginsChanged)
        .WillByDefault(
            Invoke(bridge(), &PasswordSyncBridge::ActOnPasswordStoreChanges));

    ON_CALL(mock_sync_metadata_store_sync_, GetAllSyncMetadata())
        .WillByDefault(
            []() { return std::make_unique<syncer::MetadataBatch>(); });
    ON_CALL(mock_sync_metadata_store_sync_, UpdateSyncMetadata)
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_sync_metadata_store_sync_, ClearSyncMetadata)
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_sync_metadata_store_sync_, UpdateModelTypeState)
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_sync_metadata_store_sync_, ClearModelTypeState)
        .WillByDefault(testing::Return(true));
  }

  // Creates an EntityData around a copy of the given specifics.
  syncer::EntityData SpecificsToEntity(
      const sync_pb::PasswordSpecifics& specifics) {
    syncer::EntityData data;
    *data.specifics.mutable_password() = specifics;
    data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::PASSWORDS, bridge()->GetClientTag(data));
    return data;
  }

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
  base::test::ScopedFeatureList feature_list_;
  FakeDatabase fake_db_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  testing::NiceMock<MockSyncMetadataStore> mock_sync_metadata_store_sync_;
  testing::NiceMock<MockPasswordStoreSync> mock_password_store_sync_;
  testing::NiceMock<base::MockRepeatingClosure> sync_enabled_or_disabled_cb_;
  std::unique_ptr<PasswordSyncBridge> bridge_;
};

TEST_P(PasswordSyncBridgeTest, ShouldComputeClientTagHash) {
  syncer::EntityData data;
  *data.specifics.mutable_password() = CreateSpecifics(
      "http://www.origin.com", "username_element", "username_value",
      "password_element", "signon_realm", /*issue_types=*/{});

  EXPECT_THAT(
      bridge()->GetClientTag(data),
      Eq("http%3A//www.origin.com/"
         "|username_element|username_value|password_element|signon_realm"));
}

TEST_P(PasswordSyncBridgeTest, ShouldForwardLocalChangesToTheProcessor) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));

  PasswordStoreChangeList changes;
  changes.push_back(PasswordStoreChange(PasswordStoreChange::ADD,
                                        MakePasswordForm(kSignonRealm1),
                                        FormPrimaryKey(1)));
  changes.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE,
                                        MakePasswordForm(kSignonRealm2),
                                        FormPrimaryKey(2)));
  changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE,
                                        MakePasswordForm(kSignonRealm3),
                                        FormPrimaryKey(3)));
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

TEST_P(PasswordSyncBridgeTest,
       ShouldNotForwardLocalChangesToTheProcessorIfSyncDisabled) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(false));

  PasswordStoreChangeList changes;
  changes.push_back(PasswordStoreChange(PasswordStoreChange::ADD,
                                        MakePasswordForm(kSignonRealm1),
                                        FormPrimaryKey(1)));
  changes.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE,
                                        MakePasswordForm(kSignonRealm2),
                                        FormPrimaryKey(2)));
  changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE,
                                        MakePasswordForm(kSignonRealm3),
                                        FormPrimaryKey(3)));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), Delete).Times(0);

  bridge()->ActOnPasswordStoreChanges(changes);
}

TEST_P(PasswordSyncBridgeTest, ShouldApplyEmptySyncChangesWithoutError) {
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), syncer::EntityChangeList());
  EXPECT_FALSE(error);
}

TEST_P(PasswordSyncBridgeTest, ShouldApplyMetadataWithEmptySyncChanges) {
  const std::string kStorageKey = "1";
  const std::string kServerId = "TestServerId";
  sync_pb::EntityMetadata metadata;
  metadata.set_server_id(kServerId);
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  metadata_change_list->UpdateMetadata(kStorageKey, metadata);

  EXPECT_CALL(*mock_password_store_sync(), NotifyLoginsChanged).Times(0);
  EXPECT_CALL(*mock_password_store_sync(), NotifyInsecureCredentialsChanged)
      .Times(0);

  EXPECT_CALL(*mock_sync_metadata_store_sync(),
              UpdateSyncMetadata(syncer::PASSWORDS, kStorageKey, _));

  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      std::move(metadata_change_list), syncer::EntityChangeList());
  EXPECT_FALSE(error);
}

TEST_P(PasswordSyncBridgeTest, ShouldApplyRemoteCreation) {
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
  EXPECT_CALL(*mock_password_store_sync(), NotifyInsecureCredentialsChanged)
      .Times(0);

  // Processor shouldn't be notified about remote changes.
  EXPECT_CALL(mock_processor(), Put).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_P(PasswordSyncBridgeTest,
       ShouldIgnoreAndUntrackRemoteCreationWithInvalidData) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  fake_db()->SetAddLoginError(AddLoginError::kConstraintViolation);

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealm(kSignonRealm1);

  EXPECT_CALL(mock_processor(),
              UntrackEntityForClientTagHash(
                  SpecificsToEntity(specifics).client_tag_hash));

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_P(PasswordSyncBridgeTest, ShouldApplyRemoteUpdate) {
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
  EXPECT_CALL(*mock_password_store_sync(), NotifyInsecureCredentialsChanged)
      .Times(0);

  // Processor shouldn't be notified about remote changes.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), UpdateStorageKey).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateUpdate(
      kStorageKey, SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_P(PasswordSyncBridgeTest, ShouldApplyRemoteDeletion) {
  const int kPrimaryKey = 1000;
  const std::string kStorageKey = "1000";
  // Add the form to the DB.
  fake_db()->AddLoginForPrimaryKey(kPrimaryKey,
                                   MakePasswordForm(kSignonRealm1));

  testing::InSequence in_sequence;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              RemoveLoginByPrimaryKeySync(FormPrimaryKey(kPrimaryKey)));
  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              NotifyLoginsChanged(
                  UnorderedElementsAre(ChangeHasPrimaryKey(kPrimaryKey))));
  EXPECT_CALL(*mock_password_store_sync(), NotifyInsecureCredentialsChanged)
      .Times(0);

  // Processor shouldn't be notified about remote changes.
  EXPECT_CALL(mock_processor(), Delete).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateDelete(kStorageKey));
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_P(PasswordSyncBridgeTest, ShouldGetDataForStorageKey) {
  const int kPrimaryKey1 = 1000;
  const int kPrimaryKey2 = 1001;
  const std::string kPrimaryKeyStr1 = "1000";
  const std::string kPrimaryKeyStr2 = "1001";
  PasswordForm form1 = MakePasswordForm(kSignonRealm1);
  PasswordForm form2 = MakePasswordForm(kSignonRealm2);

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

TEST_P(PasswordSyncBridgeTest, ShouldNotGetDataForNonExistingStorageKey) {
  const std::string kPrimaryKeyStr = "1";

  base::Optional<sync_pb::PasswordSpecifics> optional_specifics =
      GetDataFromBridge(/*storage_key=*/kPrimaryKeyStr);
  EXPECT_FALSE(optional_specifics.has_value());
}

TEST_P(PasswordSyncBridgeTest, ShouldMergeSyncRemoteAndLocalPasswords) {
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
  PasswordForm form1 = MakePasswordForm(kSignonRealm1);
  PasswordForm form2 = MakePasswordForm(kSignonRealm2);
  PasswordForm form3 = MakePasswordForm(kSignonRealm3);
  sync_pb::PasswordSpecifics specifics1 =
      CreateSpecificsWithSignonRealm(kSignonRealm1);
  sync_pb::PasswordSpecifics specifics2 =
      CreateSpecificsWithSignonRealm(kSignonRealm2);
  sync_pb::PasswordSpecifics specifics3 =
      CreateSpecificsWithSignonRealm(kSignonRealm3);

  base::Time now = base::Time::Now();
  base::Time yesterday = now - base::TimeDelta::FromDays(1);

  form2.date_created = yesterday;
  specifics2.mutable_client_only_encrypted_data()->set_date_created(
      now.ToDeltaSinceWindowsEpoch().InMicroseconds());

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

TEST_P(PasswordSyncBridgeTest,
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
  PasswordForm form1 = MakePasswordForm(kSignonRealm1);
  form1.date_created = now;
  sync_pb::PasswordSpecifics specifics1 =
      CreateSpecificsWithSignonRealm(kSignonRealm1);
  specifics1.mutable_client_only_encrypted_data()->set_date_created(
      yesterday.ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Remote form 2 is more recent than the local.
  PasswordForm form2 = MakePasswordForm(kSignonRealm2);
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

// This tests that if reading sync metadata from the store fails,
// metadata should be deleted and Sync starts without error.
TEST_P(
    PasswordSyncBridgeTest,
    ShouldMergeSyncRemoteAndLocalPasswordsWithoutErrorWhenMetadataReadFails) {
  // Simulate a failed GetAllSyncMetadata() by returning a nullptr.
  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault(testing::ReturnNull());

  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata());
  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/syncer::HasNotInitialSyncDone(),
                                    /*entities=*/testing::SizeIs(0))));
  auto bridge = std::make_unique<PasswordSyncBridge>(
      mock_processor().CreateForwardingProcessor(), mock_password_store_sync(),
      base::DoNothing());
}

// This tests that if reading logins from the store fails,
// ShouldMergeSync() would return an error without crashing.
TEST_P(PasswordSyncBridgeTest,
       ShouldMergeSyncRemoteAndLocalPasswordsWithErrorWhenStoreReadFails) {
  // Simulate a failed ReadAllLogins() by returning a kDbError.
  ON_CALL(*mock_password_store_sync(), ReadAllLogins)
      .WillByDefault(testing::Return(FormRetrievalResult::kDbError));
  base::Optional<syncer::ModelError> error =
      bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(), {});
  EXPECT_TRUE(error);
}

// This tests that if adding logins to the store fails,
// ShouldMergeSync() would return an error without crashing.
TEST_P(PasswordSyncBridgeTest,
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
TEST_P(
    PasswordSyncBridgeTest,
    ShouldMergeSyncRemoteAndLocalPasswordsWithErrorWhenStoreUpdateModelTypeStateFails) {
  // Simulate failure in UpdateModelTypeState();
  ON_CALL(*mock_sync_metadata_store_sync(), UpdateModelTypeState)
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

TEST_P(PasswordSyncBridgeTest,
       ShouldMergeAndIgnoreAndUntrackRemotePasswordWithInvalidData) {
  fake_db()->SetAddLoginError(AddLoginError::kConstraintViolation);

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealm(kSignonRealm1);

  EXPECT_CALL(mock_processor(),
              UntrackEntityForClientTagHash(
                  SpecificsToEntity(specifics).client_tag_hash));

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->MergeSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_P(PasswordSyncBridgeTest,
       ShouldGetAllDataForDebuggingWithRedactedPassword) {
  const int kPrimaryKey1 = 1000;
  const int kPrimaryKey2 = 1001;
  PasswordForm form1 = MakePasswordForm(kSignonRealm1);
  PasswordForm form2 = MakePasswordForm(kSignonRealm2);

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
    EXPECT_EQ("<redacted>", data_pair.second->specifics.password()
                                .client_only_encrypted_data()
                                .password_value());
  }
}

TEST_P(PasswordSyncBridgeTest,
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

#if defined(OS_MAC)
// Tests that in case ReadAllLogins() during initial merge returns encryption
// service failure, the bridge would try to do a DB clean up.
TEST_P(PasswordSyncBridgeTest, ShouldDeleteUndecryptableLoginsDuringMerge) {
  ON_CALL(*mock_password_store_sync(), DeleteUndecryptableLogins())
      .WillByDefault(Return(DatabaseCleanupResult::kSuccess));

  // We should try to read first, and simulate an encryption failure. Then,
  // cleanup the database and try to read again which should be successful now.
  EXPECT_CALL(*mock_password_store_sync(), ReadAllLogins)
      .WillOnce(Return(FormRetrievalResult::kEncrytionServiceFailure))
      .WillOnce(Return(FormRetrievalResult::kSuccess));
  EXPECT_CALL(*mock_password_store_sync(), DeleteUndecryptableLogins());

  base::Optional<syncer::ModelError> error =
      bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(), {});
  EXPECT_FALSE(error);
}
#endif

TEST_P(PasswordSyncBridgeTest,
       ShouldDeleteSyncMetadataWhenApplyStopSyncChanges) {
  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata());
  bridge()->ApplyStopSyncChanges(bridge()->CreateMetadataChangeList());
}

TEST_P(PasswordSyncBridgeTest, ShouldNotifyOnSyncEnable) {
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

TEST_P(PasswordSyncBridgeTest, ShouldNotNotifyOnSyncChange) {
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

TEST_P(PasswordSyncBridgeTest, ShouldNotifyOnSyncDisableIfAccountStore) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(true));

  // The account password store gets cleared when sync is disabled, so this
  // should trigger the callback.
  EXPECT_CALL(*mock_sync_enabled_or_disabled_cb(), Run());

  bridge()->ApplyStopSyncChanges(bridge()->CreateMetadataChangeList());
}

TEST_P(PasswordSyncBridgeTest, ShouldNotNotifyOnSyncDisableIfProfileStore) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(false));

  // The profile password store does *not* get cleared when sync is disabled, so
  // this should *not* trigger the callback.
  EXPECT_CALL(*mock_sync_enabled_or_disabled_cb(), Run()).Times(0);

  bridge()->ApplyStopSyncChanges(bridge()->CreateMetadataChangeList());
}

TEST_P(PasswordSyncBridgeTest, ShouldNotifyUnsyncedCredentialsIfAccountStore) {
  base::HistogramTester histogram_tester;
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(true));

  const std::string kPrimaryKeyUnsyncedCredentialStr = "1000";
  const std::string kPrimaryKeySyncedCredentialStr = "1001";
  const std::string kPrimaryKeyUnsyncedDeletionStr = "1002";
  const std::string kPrimaryKeyUnsyncedBlocklistStr = "1003";
  ON_CALL(mock_processor(), IsEntityUnsynced(kPrimaryKeyUnsyncedCredentialStr))
      .WillByDefault(Return(true));
  ON_CALL(mock_processor(), IsEntityUnsynced(kPrimaryKeySyncedCredentialStr))
      .WillByDefault(Return(false));
  ON_CALL(mock_processor(), IsEntityUnsynced(kPrimaryKeyUnsyncedDeletionStr))
      .WillByDefault(Return(true));
  ON_CALL(mock_processor(), IsEntityUnsynced(kPrimaryKeyUnsyncedBlocklistStr))
      .WillByDefault(Return(true));

  sync_pb::EntityMetadata is_deletion_metadata;
  is_deletion_metadata.set_is_deleted(true);
  sync_pb::EntityMetadata is_not_deletion_metadata;
  is_not_deletion_metadata.set_is_deleted(false);
  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        auto batch = std::make_unique<syncer::MetadataBatch>();
        batch->AddMetadata(kPrimaryKeyUnsyncedCredentialStr,
                           std::make_unique<sync_pb::EntityMetadata>(
                               is_not_deletion_metadata));
        batch->AddMetadata(kPrimaryKeySyncedCredentialStr,
                           std::make_unique<sync_pb::EntityMetadata>(
                               is_not_deletion_metadata));
        batch->AddMetadata(
            kPrimaryKeyUnsyncedDeletionStr,
            std::make_unique<sync_pb::EntityMetadata>(is_deletion_metadata));
        batch->AddMetadata(kPrimaryKeyUnsyncedBlocklistStr,
                           std::make_unique<sync_pb::EntityMetadata>(
                               is_not_deletion_metadata));
        return batch;
      });

  // No form is added to the database for the unsynced deletion primary key,
  // because the deletion is supposed to have already removed such form.
  const int kPrimaryKeyUnsyncedCredential = 1000;
  const int kPrimaryKeySyncedCredential = 1001;
  const int kPrimaryKeyUnsyncedBlocklist = 1003;
  PasswordForm unsynced_credential = MakePasswordForm(kSignonRealm1);
  PasswordForm synced_credential = MakePasswordForm(kSignonRealm2);
  PasswordForm unsynced_blocklist = MakeBlocklistedForm(kSignonRealm3);
  fake_db()->AddLoginForPrimaryKey(kPrimaryKeyUnsyncedCredential,
                                   unsynced_credential);
  fake_db()->AddLoginForPrimaryKey(kPrimaryKeySyncedCredential,
                                   synced_credential);
  fake_db()->AddLoginForPrimaryKey(kPrimaryKeyUnsyncedBlocklist,
                                   unsynced_blocklist);

  // The notification should only contain new credentials that are unsynced,
  // ignoring both synced ones, deletion entries and blocklists.
  EXPECT_CALL(*mock_password_store_sync(),
              NotifyUnsyncedCredentialsWillBeDeleted(
                  UnorderedElementsAre(unsynced_credential)));

  // The content of the metadata change list does not matter in this case.
  bridge()->ApplyStopSyncChanges(bridge()->CreateMetadataChangeList());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStorage.UnsyncedPasswordsFoundDuringSignOut", 1,
      1);
}

TEST_P(PasswordSyncBridgeTest,
       ShouldNotNotifyUnsyncedCredentialsIfProfileStore) {
  base::HistogramTester histogram_tester;
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(false));

  const int kPrimaryKeyUnsyncedCredential = 1000;
  const std::string kPrimaryKeyUnsyncedCredentialStr = "1000";
  ON_CALL(mock_processor(), IsEntityUnsynced(kPrimaryKeyUnsyncedCredentialStr))
      .WillByDefault(Return(true));

  sync_pb::EntityMetadata is_not_deletion_metadata;
  is_not_deletion_metadata.set_is_deleted(false);
  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        auto batch = std::make_unique<syncer::MetadataBatch>();
        batch->AddMetadata(kPrimaryKeyUnsyncedCredentialStr,
                           std::make_unique<sync_pb::EntityMetadata>(
                               is_not_deletion_metadata));
        return batch;
      });

  PasswordForm unsynced_deletion = MakePasswordForm(kSignonRealm3);
  fake_db()->AddLoginForPrimaryKey(kPrimaryKeyUnsyncedCredential,
                                   MakePasswordForm(kSignonRealm1));

  EXPECT_CALL(*mock_password_store_sync(),
              NotifyUnsyncedCredentialsWillBeDeleted)
      .Times(0);

  // The content of the metadata change list does not matter in this case.
  bridge()->ApplyStopSyncChanges(bridge()->CreateMetadataChangeList());

  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountStorage.UnsyncedPasswordsFoundDuringSignOut", 0);
}

TEST_P(PasswordSyncBridgeTest, ShouldReportDownloadedPasswordsIfAccountStore) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(true));
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"",
      SpecificsToEntity(CreateSpecificsWithSignonRealm(kSignonRealm1))));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"",
      SpecificsToEntity(CreateSpecificsWithSignonRealm(kSignonRealm2))));

  sync_pb::PasswordSpecifics blocklisted_specifics;
  sync_pb::PasswordSpecificsData* password_data =
      blocklisted_specifics.mutable_client_only_encrypted_data();
  password_data->set_origin("http://www.origin.com");
  password_data->set_signon_realm(kSignonRealm3);
  password_data->set_blacklisted(true);

  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(blocklisted_specifics)));

  base::HistogramTester histogram_tester;
  base::Optional<syncer::ModelError> error = bridge()->MergeSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStoreCredentialsAfterOptIn", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStoreBlocklistedEntriesAfterOptIn", 1, 1);
}

TEST_P(PasswordSyncBridgeTest,
       ShouldAddRemoteInsecureCredentilasUponRemoteCreation) {
  if (!GetParam())
    return;
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  const std::vector<InsecureType> kIssuesTypes = {InsecureType::kLeaked,
                                                  InsecureType::kWeak};
  const std::vector<InsecureCredential> kExpectedIssues =
      MakeInsecureCredentials(MakePasswordForm(kSignonRealm1), kIssuesTypes);

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealmAndIssues(kSignonRealm1, kIssuesTypes);

  testing::InSequence in_sequence;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              AddLoginSync(FormHasSignonRealm(kSignonRealm1), _));

  EXPECT_CALL(
      *mock_password_store_sync(),
      AddInsecureCredentialsSync(UnorderedElementsAreArray(kExpectedIssues)));

  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction());

  EXPECT_CALL(*mock_password_store_sync(), NotifyInsecureCredentialsChanged);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_P(PasswordSyncBridgeTest,
       ShouldAddRemoteInsecureCredentilasDuringInitialMerge) {
  if (!GetParam())
    return;
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  const std::vector<InsecureType> kIssuesTypes = {InsecureType::kLeaked,
                                                  InsecureType::kReused};
  const std::vector<InsecureCredential> kIssues =
      MakeInsecureCredentials(MakePasswordForm(kSignonRealm1), kIssuesTypes);
  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealmAndIssues(kSignonRealm1, kIssuesTypes);

  // Form will be added to the password store sync. We use sequence since the
  // order is important. The form itself should be added before we add the
  // insecure credentials.

  testing::Sequence in_sequence;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction());

  EXPECT_CALL(*mock_password_store_sync(),
              AddLoginSync(FormHasSignonRealm(kSignonRealm1), _));

  EXPECT_CALL(*mock_password_store_sync(),
              AddInsecureCredentialsSync(UnorderedElementsAreArray(kIssues)));
  EXPECT_CALL(*mock_password_store_sync(), NotifyInsecureCredentialsChanged);

  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));

  base::Optional<syncer::ModelError> error = bridge()->MergeSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_EQ(error, base::nullopt);
}

TEST_P(PasswordSyncBridgeTest, ShouldPutSecurityIssuesOnLoginChange) {
  if (!GetParam())
    return;
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  // Since this remote creation is the first entry in the FakeDatabase, it will
  // be assigned a primary key 1.
  const int kPrimaryKey1 = 1;
  const std::string kPrimaryKeyStr1 = "1";
  const std::vector<InsecureType> kIssuesTypes = {InsecureType::kLeaked,
                                                  InsecureType::kReused};
  const PasswordForm kForm = MakePasswordForm(kSignonRealm1);

  fake_db()->AddLoginForPrimaryKey(kPrimaryKey1, kForm);
  fake_db()->AddInsecureCredentials(
      MakeInsecureCredentials(kForm, kIssuesTypes));

  PasswordStoreChangeList changes;
  changes.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, kForm,
                                        FormPrimaryKey(kPrimaryKey1)));
  EXPECT_CALL(
      mock_processor(),
      Put(kPrimaryKeyStr1, EntityDataHasSecurityIssueTypes(kIssuesTypes), _));

  bridge()->ActOnPasswordStoreChanges(changes);
}

TEST_P(PasswordSyncBridgeTest, ShouldAddLocalSecurityIssuesDuringInitialMerge) {
  if (!GetParam())
    return;
  const int kPrimaryKey1 = 1000;
  const std::string kPrimaryKeyStr1 = "1000";
  const std::vector<InsecureType> kIssuesTypes = {InsecureType::kLeaked,
                                                  InsecureType::kReused};
  const PasswordForm kForm = MakePasswordForm(kSignonRealm1);

  sync_pb::PasswordSpecifics specifics1 =
      CreateSpecificsWithSignonRealm(kSignonRealm1);

  fake_db()->AddLoginForPrimaryKey(kPrimaryKey1, kForm);
  fake_db()->AddInsecureCredentials(
      MakeInsecureCredentials(kForm, kIssuesTypes));

  EXPECT_CALL(
      mock_processor(),
      Put(kPrimaryKeyStr1, EntityDataHasSecurityIssueTypes(kIssuesTypes), _));

  base::Optional<syncer::ModelError> error =
      bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(), {});
  EXPECT_FALSE(error);
}

TEST_P(PasswordSyncBridgeTest, GetDataWithIssuesForStorageKey) {
  if (!GetParam())
    return;
  const int kPrimaryKey1 = 1000;
  const std::string kPrimaryKeyStr1 = "1000";
  const std::vector<InsecureType> kIssuesTypes = {InsecureType::kLeaked,
                                                  InsecureType::kReused};
  const PasswordForm kForm = MakePasswordForm(kSignonRealm1);

  fake_db()->AddLoginForPrimaryKey(kPrimaryKey1, kForm);
  fake_db()->AddInsecureCredentials(
      MakeInsecureCredentials(kForm, kIssuesTypes));

  base::Optional<sync_pb::PasswordSpecifics> optional_specifics =
      GetDataFromBridge(/*storage_key=*/kPrimaryKeyStr1);
  ASSERT_TRUE(optional_specifics.has_value());
  ASSERT_TRUE(SpecificsHasExpectedInsecureTypes(
      optional_specifics.value().client_only_encrypted_data().password_issues(),
      kIssuesTypes));
}

TEST_P(PasswordSyncBridgeTest,
       ShouldUpdateInsecureCredentialsDuringRemoteUpdate) {
  if (!GetParam())
    return;
  const int kPrimaryKey = 1000;
  const std::string kStorageKey = "1000";
  // Add the form to the DB.
  fake_db()->AddLoginForPrimaryKey(kPrimaryKey,
                                   MakePasswordForm(kSignonRealm1));

  const std::vector<InsecureType> kIssuesTypes = {InsecureType::kLeaked,
                                                  InsecureType::kReused};
  const std::vector<InsecureCredential> kIssues =
      MakeInsecureCredentials(MakePasswordForm(kSignonRealm1), kIssuesTypes);

  EXPECT_CALL(
      *mock_password_store_sync(),
      UpdateInsecureCredentialsSync(FormHasSignonRealm(kSignonRealm1),
                                    UnorderedElementsAreArray(kIssues)));
  EXPECT_CALL(*mock_password_store_sync(), NotifyInsecureCredentialsChanged);

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealmAndIssues(kSignonRealm1, kIssuesTypes);
  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateUpdate(
      kStorageKey, SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_P(PasswordSyncBridgeTest,
       EqualInsecureCredentialsRequireNoUpdateDuringRemoteUpdate) {
  if (!GetParam())
    return;
  const int kPrimaryKey = 1000;
  // Add a password form with a corresponding list of insecure credentials of
  // types Leaked and Reused.
  const std::string kStorageKey = "1000";
  const PasswordForm kForm = MakePasswordForm(kSignonRealm1);
  std::vector<InsecureType> kIssuesTypes = {
      InsecureType::kLeaked, InsecureType::kReused, InsecureType::kWeak};
  const std::vector<InsecureCredential> kIssues =
      MakeInsecureCredentials(kForm, kIssuesTypes);

  fake_db()->AddLoginForPrimaryKey(kPrimaryKey, kForm);
  fake_db()->AddInsecureCredentials(kIssues);

  // Simulate a remote update to the password that contains the same set of
  // issues.
  std::shuffle(kIssuesTypes.begin(), kIssuesTypes.end(),
               std::default_random_engine{});
  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealmAndIssues(kSignonRealm1, kIssuesTypes);
  sync_pb::PasswordSpecificsData* password_data =
      specifics.mutable_client_only_encrypted_data();
  password_data->set_times_used(1);

  // Test that only UpdateLoginSync() is invoked,
  // UpdateInsecureCredentialsSync() isn't invoked because there are no
  // change in the insecure credentials information.
  EXPECT_CALL(*mock_password_store_sync(),
              UpdateLoginSync(FormHasSignonRealm(kSignonRealm1), _));
  EXPECT_CALL(*mock_password_store_sync(), UpdateInsecureCredentialsSync)
      .Times(0);
  EXPECT_CALL(*mock_password_store_sync(), NotifyInsecureCredentialsChanged)
      .Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateUpdate(
      kStorageKey, SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

INSTANTIATE_TEST_SUITE_P(SyncingInsecureCredentialsDisabledAndEnabled,
                         PasswordSyncBridgeTest,
                         ::testing::Bool());

TEST_P(PasswordSyncBridgeTest,
       EqualPasswordsDifferentInsecureCredentialsDuringMerge) {
  // This test is relevant only for syncing insecure credentials feature.
  if (!GetParam())
    return;
  const int kPrimaryKey = 1000;
  // Test that during merge when Passwords are equal but have different
  // insecure credentials, local data get updated.
  const std::string kStorageKey = "1000";
  const PasswordForm kForm = MakePasswordForm(kSignonRealm1);
  std::vector<InsecureType> kRemoteIssuesTypes = {InsecureType::kReused,
                                                  InsecureType::kWeak};
  const std::vector<InsecureCredential> kRemoteIssues =
      MakeInsecureCredentials(kForm, kRemoteIssuesTypes);
  const std::vector<InsecureCredential> kLocalIssues =
      MakeInsecureCredentials(kForm, {InsecureType::kLeaked});

  fake_db()->AddLoginForPrimaryKey(kPrimaryKey, kForm);
  fake_db()->AddInsecureCredentials(kLocalIssues);

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealmAndIssues(kSignonRealm1,
                                              kRemoteIssuesTypes);

  // Test that UpdateLoginSync and UpdateInsecureCredentialsSync() are
  // invoked with remote insecure credentials.
  EXPECT_CALL(*mock_password_store_sync(),
              UpdateLoginSync(FormHasSignonRealm(kSignonRealm1), _));
  EXPECT_CALL(
      *mock_password_store_sync(),
      UpdateInsecureCredentialsSync(FormHasSignonRealm(kSignonRealm1),
                                    UnorderedElementsAreArray(kRemoteIssues)));
  EXPECT_CALL(*mock_password_store_sync(), NotifyInsecureCredentialsChanged);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      kStorageKey, SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->MergeSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_P(PasswordSyncBridgeTest,
       EqualPasswordsAndEqualInsecureCredentialsDuringMerge) {
  if (!GetParam())
    return;
  // Test that during merge when Passwords and insecure credentials are equal
  // there are no updates.
  const int kPrimaryKey = 1000;
  const std::string kStorageKey = "1000";
  PasswordForm kForm = MakePasswordForm(kSignonRealm1);
  std::vector<InsecureType> kIssuesTypes = {InsecureType::kReused,
                                            InsecureType::kWeak};
  const std::vector<InsecureCredential> kIssues =
      MakeInsecureCredentials(kForm, kIssuesTypes);

  base::Time now = base::Time::Now();
  kForm.date_created = now;

  fake_db()->AddLoginForPrimaryKey(kPrimaryKey, kForm);
  fake_db()->AddInsecureCredentials(kIssues);

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealmAndIssues(kSignonRealm1, kIssuesTypes);
  specifics.mutable_client_only_encrypted_data()->set_date_created(
      now.ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Test that neither password store nor processor is invoked.
  EXPECT_CALL(*mock_password_store_sync(), UpdateLoginSync).Times(0);
  EXPECT_CALL(*mock_password_store_sync(), UpdateInsecureCredentialsSync)
      .Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      kStorageKey, SpecificsToEntity(specifics)));
  base::Optional<syncer::ModelError> error = bridge()->MergeSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

}  // namespace password_manager
