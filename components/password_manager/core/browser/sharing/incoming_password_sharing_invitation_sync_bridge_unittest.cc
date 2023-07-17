// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::Return;

namespace password_manager {
namespace {

syncer::EntityData MakeEntityData() {
  syncer::EntityData entity_data;
  entity_data.specifics.mutable_incoming_password_sharing_invitation()
      ->set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  entity_data.name = "test";
  return entity_data;
}

class IncomingPasswordSharingInvitationSyncBridgeTest : public testing::Test {
 public:
  IncomingPasswordSharingInvitationSyncBridgeTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        bridge_(mock_processor_.CreateForwardingProcessor(),
                syncer::ModelTypeStoreTestUtil::MoveStoreToFactory(
                    std::move(store_))) {}

  IncomingPasswordSharingInvitationSyncBridge* bridge() { return &bridge_; }
  testing::NiceMock<syncer::MockModelTypeChangeProcessor>* mock_processor() {
    return &mock_processor_;
  }

 private:
  // In memory model type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  IncomingPasswordSharingInvitationSyncBridge bridge_;
};

TEST_F(IncomingPasswordSharingInvitationSyncBridgeTest, ShouldReturnClientTag) {
  EXPECT_TRUE(bridge()->SupportsGetClientTag());
  EXPECT_FALSE(bridge()->GetClientTag(MakeEntityData()).empty());
}

TEST_F(IncomingPasswordSharingInvitationSyncBridgeTest,
       ShouldCallModelReadyToSync) {
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_processor(), ModelReadyToSync)
      .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();
}

}  // namespace
}  // namespace password_manager
