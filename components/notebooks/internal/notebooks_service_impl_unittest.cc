// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_service_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/notebooks/public/notebook.h"
#include "components/notebooks/public/notebook_id.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notebooks {

namespace {

const char kTestUuid1[] = "00000000-0000-0000-0000-000000000001";
const char kTestUuid2[] = "00000000-0000-0000-0000-000000000002";

sync_pb::NotebookSpecifics CreateTestNotebookSpecifics(
    const std::string& uuid) {
  sync_pb::NotebookSpecifics specifics;
  specifics.set_uuid(uuid);
  specifics.set_creation_time_windows_epoch_micros(
      base::Time::FromSecondsSinceUnixEpoch(1000)
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  specifics.set_update_time_windows_epoch_micros(
      base::Time::FromSecondsSinceUnixEpoch(1000)
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  specifics.mutable_notebook();
  specifics.set_schema_version(1);
  return specifics;
}

syncer::EntityData CreateTestEntityData(const std::string& uuid) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_notebook() = CreateTestNotebookSpecifics(uuid);
  entity_data.name = uuid;
  return entity_data;
}

}  // namespace

class NotebooksServiceImplTest : public testing::Test {
 public:
  NotebooksServiceImplTest() = default;
  ~NotebooksServiceImplTest() override = default;

  void SetUp() override {
    ON_CALL(mock_processor_, OnModelStarting)
        .WillByDefault(testing::SaveArg<0>(&bridge_));
    base::RunLoop run_loop;
    EXPECT_CALL(mock_processor_, ModelReadyToSync).WillOnce([&]() {
      run_loop.Quit();
    });
    service_ = std::make_unique<NotebooksServiceImpl>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest());
    run_loop.Run();
  }

  NotebooksServiceImpl* service() { return service_.get(); }
  syncer::DataTypeSyncBridge* bridge() { return bridge_; }

  void InjectRemoteAdd(const std::string& uuid) {
    CHECK(bridge_);
    syncer::EntityChangeList add_changes;
    add_changes.push_back(
        syncer::EntityChange::CreateAdd(uuid, CreateTestEntityData(uuid)));
    bridge_->ApplyIncrementalSyncChanges(bridge_->CreateMetadataChangeList(),
                                         std::move(add_changes));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  syncer::DataTypeSyncBridge* bridge_ = nullptr;
  std::unique_ptr<NotebooksServiceImpl> service_;
};

TEST_F(NotebooksServiceImplTest, IsEmptyForTestingReturnsFalse) {
  EXPECT_FALSE(service()->IsEmptyForTesting());
}

TEST_F(NotebooksServiceImplTest, GetNotebookReturnsNulloptForUnknownId) {
  EXPECT_EQ(service()->GetNotebook(NotebookId(base::Uuid::GenerateRandomV4())),
            std::nullopt);
}

TEST_F(NotebooksServiceImplTest, GetNotebookReturnsSyncedNotebook) {
  InjectRemoteAdd(kTestUuid1);

  const NotebookId id(base::Uuid::ParseCaseInsensitive(kTestUuid1));
  const std::optional<Notebook> notebook = service()->GetNotebook(id);
  ASSERT_TRUE(notebook.has_value());
  EXPECT_EQ(*notebook, Notebook(id, base::Time::FromSecondsSinceUnixEpoch(1000),
                                base::Time::FromSecondsSinceUnixEpoch(1000)));
}

TEST_F(NotebooksServiceImplTest, GetAllNotebooksReturnsEmptyInitially) {
  EXPECT_TRUE(service()->GetAllNotebooks().empty());
}

TEST_F(NotebooksServiceImplTest, GetAllNotebooksReturnsSyncedNotebooks) {
  InjectRemoteAdd(kTestUuid1);
  InjectRemoteAdd(kTestUuid2);

  const NotebookId id1(base::Uuid::ParseCaseInsensitive(kTestUuid1));
  const NotebookId id2(base::Uuid::ParseCaseInsensitive(kTestUuid2));
  const Notebook expected1(id1, base::Time::FromSecondsSinceUnixEpoch(1000),
                           base::Time::FromSecondsSinceUnixEpoch(1000));
  const Notebook expected2(id2, base::Time::FromSecondsSinceUnixEpoch(1000),
                           base::Time::FromSecondsSinceUnixEpoch(1000));
  EXPECT_THAT(service()->GetAllNotebooks(),
              testing::UnorderedElementsAre(expected1, expected2));
}

}  // namespace notebooks
