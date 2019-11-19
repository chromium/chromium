// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/generic_change_processor.h"

#include <stddef.h>

#include <utility>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_manager.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/model/data_type_error_handler_mock.h"
#include "components/sync/model/fake_syncable_service.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/syncable/read_node.h"
#include "components/sync/syncable/read_transaction.h"
#include "components/sync/syncable/test_user_share.h"
#include "components/sync/syncable/user_share.h"
#include "components/sync/syncable/write_node.h"
#include "components/sync/syncable/write_transaction.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class SyncGenericChangeProcessorTest : public testing::Test {
 public:
  // Most test cases will use this type.  For those that need a
  // GenericChangeProcessor for a different type, use |InitializeForType|.
  static const ModelType kType = PREFERENCES;

  SyncGenericChangeProcessorTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        syncable_service_ptr_factory_(&fake_syncable_service_) {}

  void SetUp() override {
    // Use kType by default, but allow test cases to re-initialize with whatever
    // type they choose.  Therefore, it's important that all type dependent
    // initialization occurs in InitializeForType.
    InitializeForType(kType);
  }

  void TearDown() override {
    if (test_user_share_) {
      test_user_share_->TearDown();
    }
  }

  // Initialize GenericChangeProcessor and related classes for testing with
  // model type |type|.
  void InitializeForType(ModelType type) {
    TearDown();
    test_user_share_ = std::make_unique<TestUserShare>();
    test_user_share_->SetUp();
    sync_merge_result_ = std::make_unique<SyncMergeResult>(type);
    merge_result_ptr_factory_ =
        std::make_unique<base::WeakPtrFactory<SyncMergeResult>>(
            sync_merge_result_.get());

    ModelTypeSet types = ProtocolTypes();
    for (ModelType type : types) {
      TestUserShare::CreateRoot(type, test_user_share_->user_share());
    }
    test_user_share_->encryption_handler()->Init();
    ConstructGenericChangeProcessor(type);
  }

  void ConstructGenericChangeProcessor(ModelType type) {
    change_processor_ = std::make_unique<GenericChangeProcessor>(
        type, std::make_unique<DataTypeErrorHandlerMock>(),
        syncable_service_ptr_factory_.GetWeakPtr(),
        merge_result_ptr_factory_->GetWeakPtr(),
        test_user_share_->user_share());
  }

  void BuildChildNodes(ModelType type, int n) {
    WriteTransaction trans(FROM_HERE, user_share());
    for (int i = 0; i < n; ++i) {
      WriteNode node(&trans);
      node.InitUniqueByCreation(type, base::StringPrintf("node%05d", i));
    }
  }

  GenericChangeProcessor* change_processor() { return change_processor_.get(); }

  UserShare* user_share() { return test_user_share_->user_share(); }

  void RunLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<SyncMergeResult> sync_merge_result_;
  std::unique_ptr<base::WeakPtrFactory<SyncMergeResult>>
      merge_result_ptr_factory_;

  FakeSyncableService fake_syncable_service_;
  base::WeakPtrFactory<FakeSyncableService> syncable_service_ptr_factory_;

  std::unique_ptr<TestUserShare> test_user_share_;
  testing::NiceMock<SyncApiComponentFactoryMock> sync_factory_;

  std::unique_ptr<GenericChangeProcessor> change_processor_;
};

// Similar to above, but focused on the method that implements sync/api
// interfaces and is hence exposed to datatypes directly.
TEST_F(SyncGenericChangeProcessorTest, StressGetAllSyncData) {
  const int kNumChildNodes = 1000;
  const int kRepeatCount = 1;

  ASSERT_NO_FATAL_FAILURE(BuildChildNodes(kType, kNumChildNodes));

  for (int i = 0; i < kRepeatCount; ++i) {
    SyncDataList sync_data = change_processor()->GetAllSyncData(kType);

    // Start with a simple test.  We can add more in-depth testing later.
    EXPECT_EQ(static_cast<size_t>(kNumChildNodes), sync_data.size());
  }
}

TEST_F(SyncGenericChangeProcessorTest, SetGetPasswords) {
  InitializeForType(PASSWORDS);
  const int kNumPasswords = 10;
  sync_pb::PasswordSpecificsData password_data;
  password_data.set_username_value("user");

  sync_pb::EntitySpecifics password_holder;

  SyncChangeList change_list;
  for (int i = 0; i < kNumPasswords; ++i) {
    password_data.set_password_value(base::StringPrintf("password%i", i));
    password_holder.mutable_password()
        ->mutable_client_only_encrypted_data()
        ->CopyFrom(password_data);
    change_list.push_back(
        SyncChange(FROM_HERE, SyncChange::ACTION_ADD,
                   SyncData::CreateLocalData(base::StringPrintf("tag%i", i),
                                             base::StringPrintf("title%i", i),
                                             password_holder)));
  }

  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, change_list).IsSet());

  SyncDataList password_list(change_processor()->GetAllSyncData(PASSWORDS));

  ASSERT_EQ(password_list.size(), change_list.size());
  for (int i = 0; i < kNumPasswords; ++i) {
    // Verify the password is returned properly.
    ASSERT_TRUE(password_list[i].GetSpecifics().has_password());
    ASSERT_TRUE(password_list[i]
                    .GetSpecifics()
                    .password()
                    .has_client_only_encrypted_data());
    ASSERT_FALSE(password_list[i].GetSpecifics().password().has_encrypted());
    const sync_pb::PasswordSpecificsData& sync_password =
        password_list[i].GetSpecifics().password().client_only_encrypted_data();
    const sync_pb::PasswordSpecificsData& change_password =
        change_list[i]
            .sync_data()
            .GetSpecifics()
            .password()
            .client_only_encrypted_data();
    ASSERT_EQ(sync_password.password_value(), change_password.password_value());
    ASSERT_EQ(sync_password.username_value(), change_password.username_value());

    // Verify the raw sync data was stored securely.
    ReadTransaction read_transaction(FROM_HERE, user_share());
    ReadNode node(&read_transaction);
    ASSERT_EQ(
        node.InitByClientTagLookup(PASSWORDS, base::StringPrintf("tag%i", i)),
        BaseNode::INIT_OK);
    ASSERT_EQ(node.GetTitle(), "encrypted");
    const sync_pb::EntitySpecifics& raw_specifics = node.GetEntitySpecifics();
    ASSERT_TRUE(raw_specifics.has_password());
    ASSERT_TRUE(raw_specifics.password().has_encrypted());
    ASSERT_FALSE(raw_specifics.password().has_client_only_encrypted_data());
  }
}

TEST_F(SyncGenericChangeProcessorTest, UpdatePasswords) {
  InitializeForType(PASSWORDS);
  const int kNumPasswords = 10;
  sync_pb::PasswordSpecificsData password_data;
  password_data.set_username_value("user");

  sync_pb::EntitySpecifics password_holder;

  SyncChangeList change_list;
  SyncChangeList change_list2;
  for (int i = 0; i < kNumPasswords; ++i) {
    password_data.set_password_value(base::StringPrintf("password%i", i));
    password_holder.mutable_password()
        ->mutable_client_only_encrypted_data()
        ->CopyFrom(password_data);
    change_list.push_back(
        SyncChange(FROM_HERE, SyncChange::ACTION_ADD,
                   SyncData::CreateLocalData(base::StringPrintf("tag%i", i),
                                             base::StringPrintf("title%i", i),
                                             password_holder)));
    password_data.set_password_value(base::StringPrintf("password_m%i", i));
    password_holder.mutable_password()
        ->mutable_client_only_encrypted_data()
        ->CopyFrom(password_data);
    change_list2.push_back(
        SyncChange(FROM_HERE, SyncChange::ACTION_UPDATE,
                   SyncData::CreateLocalData(base::StringPrintf("tag%i", i),
                                             base::StringPrintf("title_m%i", i),
                                             password_holder)));
  }

  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, change_list).IsSet());
  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, change_list2).IsSet());

  SyncDataList password_list(change_processor()->GetAllSyncData(PASSWORDS));

  ASSERT_EQ(password_list.size(), change_list2.size());
  for (int i = 0; i < kNumPasswords; ++i) {
    // Verify the password is returned properly.
    ASSERT_TRUE(password_list[i].GetSpecifics().has_password());
    ASSERT_TRUE(password_list[i]
                    .GetSpecifics()
                    .password()
                    .has_client_only_encrypted_data());
    ASSERT_FALSE(password_list[i].GetSpecifics().password().has_encrypted());
    const sync_pb::PasswordSpecificsData& sync_password =
        password_list[i].GetSpecifics().password().client_only_encrypted_data();
    const sync_pb::PasswordSpecificsData& change_password =
        change_list2[i]
            .sync_data()
            .GetSpecifics()
            .password()
            .client_only_encrypted_data();
    ASSERT_EQ(sync_password.password_value(), change_password.password_value());
    ASSERT_EQ(sync_password.username_value(), change_password.username_value());

    // Verify the raw sync data was stored securely.
    ReadTransaction read_transaction(FROM_HERE, user_share());
    ReadNode node(&read_transaction);
    ASSERT_EQ(
        node.InitByClientTagLookup(PASSWORDS, base::StringPrintf("tag%i", i)),
        BaseNode::INIT_OK);
    ASSERT_EQ(node.GetTitle(), "encrypted");
    const sync_pb::EntitySpecifics& raw_specifics = node.GetEntitySpecifics();
    ASSERT_TRUE(raw_specifics.has_password());
    ASSERT_TRUE(raw_specifics.password().has_encrypted());
    ASSERT_FALSE(raw_specifics.password().has_client_only_encrypted_data());
  }
}

// Test that attempting to add an entry that already exists still works.
TEST_F(SyncGenericChangeProcessorTest, AddExistingEntry) {
  InitializeForType(SESSIONS);
  sync_pb::EntitySpecifics sessions_specifics;
  sessions_specifics.mutable_session()->set_session_tag("session tag");
  SyncChangeList changes;

  // First add it normally.
  changes.push_back(
      SyncChange(FROM_HERE, SyncChange::ACTION_ADD,
                 SyncData::CreateLocalData(base::StringPrintf("tag"),
                                           base::StringPrintf("title"),
                                           sessions_specifics)));
  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, changes).IsSet());

  // Now attempt to add it again, but with different specifics. Should not
  // result in an error and should still update the specifics.
  sessions_specifics.mutable_session()->set_session_tag("session tag 2");
  changes[0] = SyncChange(FROM_HERE, SyncChange::ACTION_ADD,
                          SyncData::CreateLocalData(base::StringPrintf("tag"),
                                                    base::StringPrintf("title"),
                                                    sessions_specifics));
  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, changes).IsSet());

  // Verify the data was updated properly.
  SyncDataList sync_data = change_processor()->GetAllSyncData(SESSIONS);
  ASSERT_EQ(sync_data.size(), 1U);
  ASSERT_EQ("session tag 2",
            sync_data[0].GetSpecifics().session().session_tag());
  EXPECT_FALSE(SyncDataRemote(sync_data[0]).GetClientTagHash().value().empty());
}

}  // namespace

}  // namespace syncer
