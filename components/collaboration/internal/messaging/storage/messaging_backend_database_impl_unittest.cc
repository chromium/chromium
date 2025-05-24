// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/storage/messaging_backend_database_impl.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/collaboration/internal/messaging/storage/collaboration_message_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace collaboration::messaging {
namespace {

using testing::Eq;
using testing::UnorderedElementsAre;

class MessagingBackendDatabaseImplTest : public testing::Test {
 public:
  MessagingBackendDatabaseImplTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    InitDatabaseAndWaitForLoading();
  }

  ~MessagingBackendDatabaseImplTest() override = default;

  void TearDown() override {
    // This is needed to ensure that `temp_dir_` outlives any write tasks on DB
    // sequence.
    base::RunLoop run_loop;
    database_->SetShutdownCallbackForTesting(run_loop.QuitClosure());
    database_ = nullptr;
    run_loop.Run();
  }

  void MimicRestart() {
    all_entries_from_db_.clear();
    base::RunLoop run_loop;
    database_->SetShutdownCallbackForTesting(run_loop.QuitClosure());

    database_ = nullptr;
    run_loop.Run();

    InitDatabaseAndWaitForLoading();
  }

 protected:
  collaboration_pb::Message CreateMessage(
      collaboration_pb::EventType event_type,
      const std::string& collaboration_id = "TEST_COLLAB_ID") {
    collaboration_pb::Message message;
    message.set_event_type(event_type);

    message.set_uuid(base::Uuid::GenerateRandomV4().AsLowercaseString());
    message.set_collaboration_id(collaboration_id);
    message.set_dirty(static_cast<int>(DirtyType::kAll));
    message.set_event_timestamp(base::Time::Now().ToTimeT());

    MessageCategory category = GetMessageCategory(message);
    if (category == MessageCategory::kTab) {
      message.mutable_tab_data()->set_sync_tab_id(
          base::Uuid::GenerateRandomV4().AsLowercaseString());
      message.mutable_tab_data()->set_sync_tab_group_id(
          base::Uuid::GenerateRandomV4().AsLowercaseString());
    } else if (category == MessageCategory::kTabGroup) {
      message.mutable_tab_group_data()->set_sync_tab_group_id(
          base::Uuid::GenerateRandomV4().AsLowercaseString());
    } else if (category == MessageCategory::kCollaboration) {
      *message.mutable_collaboration_data() =
          collaboration_pb::CollaborationData();
    }
    return message;
  }

  std::map<std::string, collaboration_pb::Message> GetAllDataFromDB() {
    return all_entries_from_db_;
  }

  std::unique_ptr<MessagingBackendDatabaseImpl> database_;

 private:
  void InitDatabaseAndWaitForLoading() {
    base::RunLoop run_loop;
    database_ =
        std::make_unique<MessagingBackendDatabaseImpl>(temp_dir_.GetPath());
    database_->Initialize(
        base::BindOnce(&MessagingBackendDatabaseImplTest::OnDBLoaded,
                       weak_ptr_factory_.GetWeakPtr(), &run_loop));
    run_loop.Run();
  }

  void OnDBLoaded(
      base::RunLoop* run_loop,
      bool success,
      const std::map<std::string, collaboration_pb::Message>& data) {
    EXPECT_EQ(true, success);
    LOG(ERROR) << data.size();
    all_entries_from_db_ = data;
    run_loop->Quit();
  }

  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  std::map<std::string, collaboration_pb::Message> all_entries_from_db_;
  base::WeakPtrFactory<MessagingBackendDatabaseImplTest> weak_ptr_factory_{
      this};
};

TEST_F(MessagingBackendDatabaseImplTest, AddMessages) {
  // Create 2 message;
  auto message1 = CreateMessage(collaboration_pb::TAB_ADDED);
  database_->Update(message1);
  auto message2 = CreateMessage(collaboration_pb::TAB_UPDATED);
  database_->Update(message2);
  MimicRestart();

  // Make sure 2 messages are saved after restart.
  std::optional<collaboration_pb::Message> db_message1 =
      database_->GetMessageForTesting(message1.uuid());
  ASSERT_TRUE(db_message1.has_value());
  ASSERT_EQ(message1.uuid(), db_message1->uuid());
  std::optional<collaboration_pb::Message> db_message2 =
      database_->GetMessageForTesting(message2.uuid());
  ASSERT_TRUE(db_message2.has_value());
  ASSERT_EQ(message2.uuid(), db_message2->uuid());
}

TEST_F(MessagingBackendDatabaseImplTest, UpdateMessage) {
  // Create a message.
  auto message = CreateMessage(collaboration_pb::TAB_UPDATED);
  database_->Update(message);
  MimicRestart();

  // Update the message.
  message.set_dirty(static_cast<int>(DirtyType::kNone));
  database_->Update(message);
  MimicRestart();

  // Make sure the massage is updated after restart.
  std::optional<collaboration_pb::Message> db_message =
      database_->GetMessageForTesting(message.uuid());
  ASSERT_TRUE(db_message.has_value());
  ASSERT_EQ(message.uuid(), db_message->uuid());
  ASSERT_EQ(static_cast<int>(DirtyType::kNone), db_message->dirty());
}

TEST_F(MessagingBackendDatabaseImplTest, DeleteMessage) {
  // Update a message.
  auto message = CreateMessage(collaboration_pb::TAB_ADDED);
  database_->Update(message);
  MimicRestart();

  // Make sure the message is saved after restart.
  std::optional<collaboration_pb::Message> db_message =
      database_->GetMessageForTesting(message.uuid());
  ASSERT_TRUE(db_message.has_value());
  // Delete message.
  database_->Delete({message.uuid()});

  // Make sure the message is deleted after restart.
  MimicRestart();
  ASSERT_FALSE(database_->GetMessageForTesting(message.uuid()).has_value());
}

TEST_F(MessagingBackendDatabaseImplTest, DeleteAllMessages) {
  // Update a message.
  auto message = CreateMessage(collaboration_pb::TAB_ADDED);
  database_->Update(message);
  MimicRestart();

  // Make sure the message is saved after restart.
  std::optional<collaboration_pb::Message> db_message =
      database_->GetMessageForTesting(message.uuid());
  ASSERT_TRUE(db_message.has_value());
  // Delete message.
  database_->DeleteAllData();

  // Make sure the message is deleted after restart.
  MimicRestart();
  ASSERT_FALSE(database_->GetMessageForTesting(message.uuid()).has_value());
  ASSERT_TRUE(GetAllDataFromDB().empty());
}

}  // namespace
}  // namespace collaboration::messaging
