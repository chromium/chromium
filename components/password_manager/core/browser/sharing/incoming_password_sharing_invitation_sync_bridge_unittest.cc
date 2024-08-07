// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sharing/password_receiver_service.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::HasInitialSyncDone;
using syncer::IsEmptyMetadataBatch;
using syncer::MetadataBatchContains;
using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::IsEmpty;
using testing::Return;
using testing::SaveArg;

namespace password_manager {
namespace {

constexpr char kPasswordValue[] = "password";
constexpr char kSignonRealm[] = "signon_realm";
constexpr char kOrigin[] = "http://abc.com/";
constexpr char kUsernameElement[] = "username_element";
constexpr char kUsernameValue[] = "username";
constexpr char kPasswordElement[] = "password_element";
constexpr char kPasswordDisplayName[] = "password_display_name";
constexpr char kPasswordAvatarUrl[] = "http://avatar.url/";
constexpr char kSenderEmail[] = "sender@gmail.com";
constexpr char kSenderDisplayName[] = "sender_display_name";
constexpr char kSenderProfileImageUrl[] = "http://www.sender/profile_iamge";

class MockPasswordReceiverService : public PasswordReceiverService {
 public:
  MOCK_METHOD(void,
              ProcessIncomingSharingInvitation,
              (sync_pb::IncomingPasswordSharingInvitationSpecifics));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              ());
  MOCK_METHOD(void, OnSyncServiceInitialized, (syncer::SyncService*));
};

sync_pb::IncomingPasswordSharingInvitationSpecifics MakeSpecifics() {
  sync_pb::IncomingPasswordSharingInvitationSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  specifics.mutable_sender_info()->mutable_user_display_info()->set_email(
      kSenderEmail);
  specifics.mutable_sender_info()
      ->mutable_user_display_info()
      ->set_display_name(kSenderDisplayName);
  specifics.mutable_sender_info()
      ->mutable_user_display_info()
      ->set_profile_image_url(kSenderProfileImageUrl);

  sync_pb::PasswordSharingInvitationData::PasswordGroupData*
      mutable_password_group_data =
          specifics.mutable_client_only_unencrypted_data()
              ->mutable_password_group_data();
  mutable_password_group_data->set_password_value(kPasswordValue);
  mutable_password_group_data->set_username_value(kUsernameValue);
  sync_pb::PasswordSharingInvitationData::PasswordGroupElementData*
      group_element_data = mutable_password_group_data->add_element_data();
  group_element_data->set_scheme(
      static_cast<int>(password_manager::PasswordForm::Scheme::kHtml));
  group_element_data->set_signon_realm(kSignonRealm);
  group_element_data->set_origin(kOrigin);
  group_element_data->set_username_element(kUsernameElement);
  group_element_data->set_password_element(kPasswordElement);
  group_element_data->set_display_name(kPasswordDisplayName);
  group_element_data->set_avatar_url(kPasswordAvatarUrl);

  return specifics;
}

syncer::EntityData EntityDataFromSpecifics(
    const sync_pb::IncomingPasswordSharingInvitationSpecifics& specifics) {
  syncer::EntityData entity_data;
  entity_data.specifics.mutable_incoming_password_sharing_invitation()
      ->CopyFrom(specifics);
  entity_data.name = "test";
  return entity_data;
}

syncer::EntityData MakeEntityData() {
  return EntityDataFromSpecifics(MakeSpecifics());
}

std::unique_ptr<syncer::EntityChange> EntityChangeFromSpecifics(
    const sync_pb::IncomingPasswordSharingInvitationSpecifics& specifics) {
  return syncer::EntityChange::CreateAdd(specifics.guid(),
                                         EntityDataFromSpecifics(specifics));
}

class IncomingPasswordSharingInvitationSyncBridgeTest : public testing::Test {
 public:
  IncomingPasswordSharingInvitationSyncBridgeTest()
      : sync_metadata_store_(
            syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    ON_CALL(*mock_processor(), ModelReadyToSync)
        .WillByDefault(
            Invoke(this, &IncomingPasswordSharingInvitationSyncBridgeTest::
                             OnModelReadyToSync));

    // Do not create bridge and wait for ready to sync here to be able to set
    // additional expectations on |mock_processor_|.
  }

  // Creates a bridge and waits for its initialization, i.e. loading from the
  // disk.
  void CreateBridgeAndWaitForReadyToSync() {
    DCHECK(model_ready_to_sync_callback_.is_null());
    base::RunLoop run_loop;
    model_ready_to_sync_callback_ = run_loop.QuitClosure();

    bridge_ = std::make_unique<IncomingPasswordSharingInvitationSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
            sync_metadata_store_.get()));
    bridge_->SetPasswordReceiverService(&mock_password_receiver_service_);

    // Wait for ready to sync.
    run_loop.Run();
    DCHECK(model_ready_to_sync_callback_.is_null());
  }

  testing::NiceMock<MockPasswordReceiverService>*
  mock_password_receiver_service() {
    return &mock_password_receiver_service_;
  }
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>*
  mock_processor() {
    return &mock_processor_;
  }
  IncomingPasswordSharingInvitationSyncBridge* bridge() {
    return bridge_.get();
  }

 private:
  void OnModelReadyToSync() {
    if (model_ready_to_sync_callback_) {
      std::move(model_ready_to_sync_callback_).Run();
    }
  }

  // In memory data type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  testing::NiceMock<MockPasswordReceiverService>
      mock_password_receiver_service_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::DataTypeStore> sync_metadata_store_;
  std::unique_ptr<IncomingPasswordSharingInvitationSyncBridge> bridge_;

  // Called if present on ModelReadyToSync() call for |mock_processor_|.
  base::OnceClosure model_ready_to_sync_callback_;
};

TEST_F(IncomingPasswordSharingInvitationSyncBridgeTest, ShouldReturnClientTag) {
  CreateBridgeAndWaitForReadyToSync();
  EXPECT_TRUE(bridge()->SupportsGetClientTag());
  EXPECT_FALSE(bridge()->GetClientTag(MakeEntityData()).empty());
}

TEST_F(IncomingPasswordSharingInvitationSyncBridgeTest,
       ShouldProcessIncrementalIncomingInvitations) {
  sync_pb::IncomingPasswordSharingInvitationSpecifics received_invitation;
  EXPECT_CALL(*mock_password_receiver_service(),
              ProcessIncomingSharingInvitation)
      .WillOnce(SaveArg<0>(&received_invitation));

  CreateBridgeAndWaitForReadyToSync();
  std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();
  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(EntityChangeFromSpecifics(MakeSpecifics()));

  EXPECT_CALL(*mock_processor(),
              Delete(entity_changes.front()->storage_key(), _, _));
  bridge()->ApplyIncrementalSyncChanges(std::move(metadata_changes),
                                        std::move(entity_changes));

  const sync_pb::PasswordSharingInvitationData::PasswordGroupData&
      received_credentials = received_invitation.client_only_unencrypted_data()
                                 .password_group_data();
  EXPECT_EQ(received_credentials.password_value(), kPasswordValue);
  EXPECT_EQ(received_credentials.username_value(), kUsernameValue);
  EXPECT_EQ(static_cast<PasswordForm::Scheme>(
                received_credentials.element_data(0).scheme()),
            password_manager::PasswordForm::Scheme::kHtml);
  EXPECT_EQ(received_credentials.element_data(0).signon_realm(), kSignonRealm);
  EXPECT_EQ(received_credentials.element_data(0).origin(), kOrigin);
  EXPECT_EQ(received_credentials.element_data(0).username_element(),
            kUsernameElement);
  EXPECT_EQ(received_credentials.element_data(0).password_element(),
            kPasswordElement);
  EXPECT_EQ(received_credentials.element_data(0).display_name(),
            kPasswordDisplayName);
  EXPECT_EQ(received_credentials.element_data(0).avatar_url(),
            kPasswordAvatarUrl);

  EXPECT_EQ(received_invitation.sender_info().user_display_info().email(),
            kSenderEmail);
  EXPECT_EQ(
      received_invitation.sender_info().user_display_info().display_name(),
      kSenderDisplayName);
  EXPECT_EQ(
      received_invitation.sender_info().user_display_info().profile_image_url(),
      kSenderProfileImageUrl);
}

TEST_F(IncomingPasswordSharingInvitationSyncBridgeTest,
       ShouldProcessInvitationsDuringInitialMerge) {
  sync_pb::IncomingPasswordSharingInvitationSpecifics received_invitation;
  EXPECT_CALL(*mock_password_receiver_service(),
              ProcessIncomingSharingInvitation)
      .WillOnce(SaveArg<0>(&received_invitation));

  CreateBridgeAndWaitForReadyToSync();
  std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();
  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(EntityChangeFromSpecifics(MakeSpecifics()));

  EXPECT_CALL(*mock_processor(),
              Delete(entity_changes.front()->storage_key(), _, _));
  bridge()->MergeFullSyncData(std::move(metadata_changes),
                              std::move(entity_changes));

  // Check only password value for sanity, the other fields are covered by other
  // tests.
  EXPECT_EQ(received_invitation.client_only_unencrypted_data()
                .password_group_data()
                .password_value(),
            kPasswordValue);
}

TEST_F(IncomingPasswordSharingInvitationSyncBridgeTest,
       ShouldStoreAndLoadSyncMetadata) {
  // The first call is expected in the very beginning when there is no metadata
  // yet. Use InSequence to guarantee the sequence of the expected calls.
  InSequence sequence;
  EXPECT_CALL(*mock_processor(), ModelReadyToSync(IsEmptyMetadataBatch()));

  // The following call should happen on the second initialization, and it
  // should have non-empty metadata.
  EXPECT_CALL(*mock_processor(),
              ModelReadyToSync(MetadataBatchContains(HasInitialSyncDone(),
                                                     /*entities*/ IsEmpty())));

  CreateBridgeAndWaitForReadyToSync();

  // Simulate the initial sync merge.
  std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();
  sync_pb::DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState::INITIAL_SYNC_DONE);
  metadata_changes->UpdateDataTypeState(data_type_state);
  bridge()->MergeFullSyncData(std::move(metadata_changes),
                              syncer::EntityChangeList());

  // Simulate restarting the sync bridge.
  CreateBridgeAndWaitForReadyToSync();
}

}  // namespace
}  // namespace password_manager
