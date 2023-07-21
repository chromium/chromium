// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_sync_bridge.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::DataBatch;
using syncer::EntityData;
using testing::IsEmpty;
using testing::Not;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;

namespace password_manager {
namespace {

EntityData MakeEntityData() {
  EntityData entity_data;
  entity_data.name = "test";
  entity_data.specifics.mutable_outgoing_password_sharing_invitation()
      ->set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  return entity_data;
}

class OutgoingPasswordSharingInvitationSyncBridgeTest : public testing::Test {
 public:
  // Do not create bridge here to be able to set additional expectations on
  // |mock_processor_|.
  OutgoingPasswordSharingInvitationSyncBridgeTest() {
    ON_CALL(*mock_processor(), IsTrackingMetadata).WillByDefault(Return(true));
  }

  void CreateBridge() {
    bridge_ = std::make_unique<OutgoingPasswordSharingInvitationSyncBridge>(
        mock_processor_.CreateForwardingProcessor());
  }

  std::unique_ptr<EntityData> GetDataFromBridge(
      const std::string& storage_key) {
    base::RunLoop loop;
    std::unique_ptr<DataBatch> batch;
    bridge_->GetData(
        {storage_key},
        base::BindOnce(
            [](base::RunLoop* loop, std::unique_ptr<DataBatch>* out_batch,
               std::unique_ptr<DataBatch> result_data) {
              *out_batch = std::move(result_data);
              loop->Quit();
            },
            &loop, &batch));
    loop.Run();

    if (!batch || !batch->HasNext()) {
      return nullptr;
    }

    auto [key, data] = batch->Next();
    EXPECT_EQ(key, storage_key);
    EXPECT_FALSE(batch->HasNext());

    return std::move(data);
  }

  testing::NiceMock<syncer::MockModelTypeChangeProcessor>* mock_processor() {
    return &mock_processor_;
  }
  OutgoingPasswordSharingInvitationSyncBridge* bridge() {
    return bridge_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<OutgoingPasswordSharingInvitationSyncBridge> bridge_;
};

TEST_F(OutgoingPasswordSharingInvitationSyncBridgeTest, ShouldReturnClientTag) {
  CreateBridge();
  EXPECT_TRUE(bridge()->SupportsGetClientTag());
  EXPECT_FALSE(bridge()->GetClientTag(MakeEntityData()).empty());
}

TEST_F(OutgoingPasswordSharingInvitationSyncBridgeTest,
       ShouldSendEntityForCommit) {
  EXPECT_CALL(*mock_processor(), Put);
  CreateBridge();

  bridge()->SendPassword(CredentialUIEntry(), /*recipient=*/{"user_id"});
}

TEST_F(OutgoingPasswordSharingInvitationSyncBridgeTest,
       ShouldReturnInFlightDataForRetry) {
  std::string storage_key;
  EXPECT_CALL(*mock_processor(), Put).WillOnce(SaveArg<0>(&storage_key));
  CreateBridge();
  bridge()->SendPassword(CredentialUIEntry(), /*recipient=*/{"user_id"});

  ASSERT_THAT(storage_key, Not(IsEmpty()));
  std::unique_ptr<EntityData> get_data_result = GetDataFromBridge(storage_key);
  ASSERT_THAT(get_data_result, NotNull());
  EXPECT_TRUE(
      get_data_result->specifics.has_outgoing_password_sharing_invitation());

  const sync_pb::OutgoingPasswordSharingInvitationSpecifics&
      invitation_specifics =
          get_data_result->specifics.outgoing_password_sharing_invitation();
  EXPECT_EQ(invitation_specifics.guid(), storage_key);
  EXPECT_EQ(invitation_specifics.recipient_user_id(), "user_id");
  // TODO(crbug.com/1445868): check for the other fields once populated.
}

}  // namespace
}  // namespace password_manager
