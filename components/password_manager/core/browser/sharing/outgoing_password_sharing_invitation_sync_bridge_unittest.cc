// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_sync_bridge.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::OutgoingPasswordSharingInvitationSpecifics;
using syncer::DataBatch;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using testing::DoAll;
using testing::IsEmpty;
using testing::IsNull;
using testing::Not;
using testing::NotNull;
using testing::Property;
using testing::Return;
using testing::SaveArg;
using testing::UnorderedElementsAre;

namespace password_manager {
namespace {

// Action SaveArgPointeeMove<k>(pointer) saves the value pointed to by the k-th
// (0-based) argument of the mock function by moving it to *pointer.
ACTION_TEMPLATE(SaveArgPointeeMove,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  *pointer = std::move(*testing::get<k>(args));
}

constexpr char kRecipientUserId[] = "recipient_user_id";
constexpr char kPasswordValue[] = "password";
constexpr char kSignonRealm[] = "http://abc.com/";
constexpr char kPslMatchSignonRealm[] = "http://n.abc.com/";
constexpr char kOrigin[] = "http://abc.com/";
constexpr char kUsernameElement[] = "username_element";
constexpr char kUsernameValue[] = "username";
constexpr char kPasswordElement[] = "password_element";
constexpr char kPasswordDisplayName[] = "password_display_name";
constexpr char kPasswordAvatarUrl[] = "http://avatar.url/";

PasswordForm MakePasswordForm() {
  PasswordForm password_form;
  password_form.password_value = base::UTF8ToUTF16(std::string(kPasswordValue));
  password_form.signon_realm = kSignonRealm;
  password_form.url = GURL(kOrigin);
  password_form.username_element =
      base::UTF8ToUTF16(std::string(kUsernameElement));
  password_form.username_value = base::UTF8ToUTF16(std::string(kUsernameValue));
  password_form.password_element =
      base::UTF8ToUTF16(std::string(kPasswordElement));
  password_form.display_name =
      base::UTF8ToUTF16(std::string(kPasswordDisplayName));
  password_form.icon_url = GURL(kPasswordAvatarUrl);
  return password_form;
}

OutgoingPasswordSharingInvitationSpecifics MakeSpecifics() {
  OutgoingPasswordSharingInvitationSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  specifics.set_recipient_user_id(kRecipientUserId);

  sync_pb::PasswordSharingInvitationData::PasswordGroupData*
      mutable_password_group_data =
          specifics.mutable_client_only_unencrypted_data()
              ->mutable_password_group_data();
  mutable_password_group_data->set_password_value(kPasswordValue);
  mutable_password_group_data->set_username_value(kUsernameValue);

  sync_pb::PasswordSharingInvitationData::PasswordGroupElementData*
      mutable_password_group_element_data =
          mutable_password_group_data->add_element_data();
  mutable_password_group_element_data->set_scheme(
      static_cast<int>(password_manager::PasswordForm::Scheme::kHtml));
  mutable_password_group_element_data->set_signon_realm(kSignonRealm);
  mutable_password_group_element_data->set_origin(kOrigin);
  mutable_password_group_element_data->set_username_element(kUsernameElement);
  mutable_password_group_element_data->set_password_element(kPasswordElement);
  mutable_password_group_element_data->set_display_name(kPasswordDisplayName);
  mutable_password_group_element_data->set_avatar_url(kPasswordAvatarUrl);

  return specifics;
}

EntityData EntityDataFromSpecifics(
    const OutgoingPasswordSharingInvitationSpecifics& specifics) {
  EntityData entity_data;
  entity_data.specifics.mutable_outgoing_password_sharing_invitation()
      ->CopyFrom(specifics);
  entity_data.name = "test";
  return entity_data;
}

EntityData MakeEntityData() {
  return EntityDataFromSpecifics(MakeSpecifics());
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
    std::unique_ptr<DataBatch> batch = bridge_->GetDataForCommit({storage_key});

    if (!batch || !batch->HasNext()) {
      return nullptr;
    }

    auto [key, data] = batch->Next();
    EXPECT_EQ(key, storage_key);
    EXPECT_FALSE(batch->HasNext());

    return std::move(data);
  }

  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>*
  mock_processor() {
    return &mock_processor_;
  }
  OutgoingPasswordSharingInvitationSyncBridge* bridge() {
    return bridge_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<OutgoingPasswordSharingInvitationSyncBridge> bridge_;
};

TEST_F(OutgoingPasswordSharingInvitationSyncBridgeTest, ShouldReturnClientTag) {
  CreateBridge();
  EXPECT_TRUE(bridge()->SupportsGetClientTag());
  EXPECT_FALSE(bridge()->GetClientTag(MakeEntityData()).empty());
}

TEST_F(OutgoingPasswordSharingInvitationSyncBridgeTest,
       ShouldSendEntityForCommit) {
  std::string storage_key;
  EntityData entity_data;
  EXPECT_CALL(*mock_processor(), Put)
      .WillOnce(
          DoAll(SaveArg<0>(&storage_key), SaveArgPointeeMove<1>(&entity_data)));
  CreateBridge();

  bridge()->SendPasswordGroup({MakePasswordForm()},
                              /*recipient=*/{kRecipientUserId});

  const OutgoingPasswordSharingInvitationSpecifics& invitation_specifics =
      entity_data.specifics.outgoing_password_sharing_invitation();
  EXPECT_EQ(invitation_specifics.guid(), storage_key);
  EXPECT_EQ(invitation_specifics.recipient_user_id(), kRecipientUserId);

  const sync_pb::PasswordSharingInvitationData::PasswordGroupData&
      password_group_data = invitation_specifics.client_only_unencrypted_data()
                                .password_group_data();
  EXPECT_EQ(password_group_data.username_value(), kUsernameValue);
  EXPECT_EQ(password_group_data.password_value(), kPasswordValue);

  const sync_pb::PasswordSharingInvitationData::PasswordGroupElementData&
      password_group_element_data = password_group_data.element_data()[0];
  EXPECT_EQ(password_group_element_data.scheme(),
            static_cast<int>(PasswordForm::Scheme::kHtml));
  EXPECT_EQ(password_group_element_data.signon_realm(), kSignonRealm);
  EXPECT_EQ(password_group_element_data.origin(), kOrigin);
  EXPECT_EQ(password_group_element_data.username_element(), kUsernameElement);
  EXPECT_EQ(password_group_element_data.password_element(), kPasswordElement);
  EXPECT_EQ(password_group_element_data.display_name(), kPasswordDisplayName);
  EXPECT_EQ(password_group_element_data.avatar_url(), kPasswordAvatarUrl);
}

TEST_F(OutgoingPasswordSharingInvitationSyncBridgeTest,
       ShouldReturnInFlightDataForRetry) {
  std::string storage_key;
  EXPECT_CALL(*mock_processor(), Put).WillOnce(SaveArg<0>(&storage_key));
  CreateBridge();
  bridge()->SendPasswordGroup({MakePasswordForm()},
                              /*recipient=*/{kRecipientUserId});

  ASSERT_THAT(storage_key, Not(IsEmpty()));
  std::unique_ptr<EntityData> get_data_result = GetDataFromBridge(storage_key);
  ASSERT_THAT(get_data_result, NotNull());
  EXPECT_TRUE(
      get_data_result->specifics.has_outgoing_password_sharing_invitation());

  const OutgoingPasswordSharingInvitationSpecifics& invitation_specifics =
      get_data_result->specifics.outgoing_password_sharing_invitation();
  EXPECT_EQ(invitation_specifics.guid(), storage_key);
  EXPECT_EQ(invitation_specifics.recipient_user_id(), kRecipientUserId);

  const sync_pb::PasswordSharingInvitationData::PasswordGroupData&
      password_group_data = invitation_specifics.client_only_unencrypted_data()
                                .password_group_data();
  EXPECT_EQ(password_group_data.username_value(), kUsernameValue);
  EXPECT_EQ(password_group_data.password_value(), kPasswordValue);

  const sync_pb::PasswordSharingInvitationData::PasswordGroupElementData&
      password_group_element_data = password_group_data.element_data()[0];
  EXPECT_EQ(password_group_element_data.scheme(),
            static_cast<int>(PasswordForm::Scheme::kHtml));
  EXPECT_EQ(password_group_element_data.signon_realm(), kSignonRealm);
  EXPECT_EQ(password_group_element_data.origin(), kOrigin);
  EXPECT_EQ(password_group_element_data.username_element(), kUsernameElement);
  EXPECT_EQ(password_group_element_data.password_element(), kPasswordElement);
  EXPECT_EQ(password_group_element_data.display_name(), kPasswordDisplayName);
  EXPECT_EQ(password_group_element_data.avatar_url(), kPasswordAvatarUrl);
}

TEST_F(OutgoingPasswordSharingInvitationSyncBridgeTest,
       ShouldSendEntityForCommitForPasswordGroup) {
  std::string storage_key;
  EntityData entity_data;
  EXPECT_CALL(*mock_processor(), Put)
      .WillOnce(
          DoAll(SaveArg<0>(&storage_key), SaveArgPointeeMove<1>(&entity_data)));
  CreateBridge();

  PasswordForm form = MakePasswordForm();
  PasswordForm psl_match_form = form;
  psl_match_form.signon_realm = kPslMatchSignonRealm;

  bridge()->SendPasswordGroup({form, psl_match_form},
                              /*recipient=*/{kRecipientUserId});

  const OutgoingPasswordSharingInvitationSpecifics& invitation_specifics =
      entity_data.specifics.outgoing_password_sharing_invitation();
  EXPECT_EQ(invitation_specifics.guid(), storage_key);
  EXPECT_EQ(invitation_specifics.recipient_user_id(), kRecipientUserId);

  const sync_pb::PasswordSharingInvitationData::PasswordGroupData&
      password_group_data = invitation_specifics.client_only_unencrypted_data()
                                .password_group_data();
  EXPECT_EQ(password_group_data.username_value(), kUsernameValue);
  EXPECT_EQ(password_group_data.password_value(), kPasswordValue);

  EXPECT_THAT(
      password_group_data.element_data(),
      UnorderedElementsAre(Property(&sync_pb::PasswordSharingInvitationData::
                                        PasswordGroupElementData::signon_realm,
                                    kSignonRealm),
                           Property(&sync_pb::PasswordSharingInvitationData::
                                        PasswordGroupElementData::signon_realm,
                                    kPslMatchSignonRealm)));
}

TEST_F(OutgoingPasswordSharingInvitationSyncBridgeTest,
       ShouldClearCommittedInFlightInvitations) {
  std::string storage_key;
  EXPECT_CALL(*mock_processor(), Put).WillOnce(SaveArg<0>(&storage_key));
  CreateBridge();
  bridge()->SendPasswordGroup({MakePasswordForm()},
                              /*recipient=*/{kRecipientUserId});

  // Verify that the invitation is still in flight.
  ASSERT_THAT(storage_key, Not(IsEmpty()));
  std::unique_ptr<EntityData> get_data_result = GetDataFromBridge(storage_key);
  ASSERT_THAT(get_data_result, NotNull());

  // Simulate successful Commit request.
  EntityChangeList entity_changes;
  entity_changes.push_back(EntityChange::CreateDelete(storage_key));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(entity_changes));

  // Verify that the given storage key has been removed from the bridge.
  EXPECT_THAT(GetDataFromBridge(storage_key), IsNull());
}

TEST_F(OutgoingPasswordSharingInvitationSyncBridgeTest,
       ShouldClearInFlightInvitationsWhenSyncDisabled) {
  std::string storage_key;
  EXPECT_CALL(*mock_processor(), Put).WillOnce(SaveArg<0>(&storage_key));
  CreateBridge();
  bridge()->SendPasswordGroup({MakePasswordForm()},
                              /*recipient=*/{kRecipientUserId});

  // Verify that the invitation is still in flight.
  ASSERT_THAT(storage_key, Not(IsEmpty()));
  std::unique_ptr<EntityData> get_data_result = GetDataFromBridge(storage_key);
  ASSERT_THAT(get_data_result, NotNull());

  // Simulate the data type is disabled.
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());

  // Verify that the given storage key has been removed from the bridge.
  EXPECT_THAT(GetDataFromBridge(storage_key), IsNull());
}

TEST_F(OutgoingPasswordSharingInvitationSyncBridgeTest,
       ShouldDropDataOnPermanentError) {
  std::string storage_key;
  EXPECT_CALL(*mock_processor(), Put).WillOnce(SaveArg<0>(&storage_key));
  CreateBridge();
  bridge()->SendPasswordGroup({MakePasswordForm()},
                              /*recipient=*/{kRecipientUserId});

  // Verify that the invitation is still in flight.
  ASSERT_THAT(storage_key, Not(IsEmpty()));
  std::unique_ptr<EntityData> get_data_result = GetDataFromBridge(storage_key);
  ASSERT_THAT(get_data_result, NotNull());

  // Simulate an invalid message error from the server.
  syncer::FailedCommitResponseData error_response;
  error_response.client_tag_hash = OutgoingPasswordSharingInvitationSyncBridge::
      GetClientTagHashFromStorageKeyForTest(storage_key);
  error_response.response_type = sync_pb::CommitResponse::INVALID_MESSAGE;
  EXPECT_CALL(*mock_processor(),
              UntrackEntityForClientTagHash(error_response.client_tag_hash));
  bridge()->OnCommitAttemptErrors({error_response});

  // Verify that the given storage key has been removed from the bridge.
  EXPECT_THAT(GetDataFromBridge(storage_key), IsNull());
}

}  // namespace
}  // namespace password_manager
