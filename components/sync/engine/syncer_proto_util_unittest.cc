// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/syncer_proto_util.h"

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/sync/engine/cycle/sync_cycle_context.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/mock_connection_manager.h"
#include "components/sync/test/model_type_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

using sync_pb::ClientToServerMessage;
using sync_pb::CommitResponse_EntryResponse;

namespace syncer {

// Builds a ClientToServerResponse with some data type ids, including
// invalid ones.  GetTypesToMigrate() should return only the valid
// model types.
TEST(SyncerProtoUtil, GetTypesToMigrate) {
  sync_pb::ClientToServerResponse response;
  response.add_migrated_data_type_id(
      GetSpecificsFieldNumberFromModelType(BOOKMARKS));
  response.add_migrated_data_type_id(
      GetSpecificsFieldNumberFromModelType(HISTORY_DELETE_DIRECTIVES));
  response.add_migrated_data_type_id(-1);
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, HISTORY_DELETE_DIRECTIVES),
            GetTypesToMigrate(response));
}

// Builds a ClientToServerResponse_Error with some error data type
// ids, including invalid ones.  ConvertErrorPBToSyncProtocolError() should
// return a SyncProtocolError with only the valid model types.
TEST(SyncerProtoUtil, ConvertErrorPBToSyncProtocolError) {
  sync_pb::ClientToServerResponse_Error error_pb;
  error_pb.set_error_type(sync_pb::SyncEnums::THROTTLED);
  error_pb.add_error_data_type_ids(
      GetSpecificsFieldNumberFromModelType(BOOKMARKS));
  error_pb.add_error_data_type_ids(
      GetSpecificsFieldNumberFromModelType(HISTORY_DELETE_DIRECTIVES));
  error_pb.add_error_data_type_ids(-1);
  SyncProtocolError error = ConvertErrorPBToSyncProtocolError(error_pb);
  EXPECT_EQ(ModelTypeSet(BOOKMARKS, HISTORY_DELETE_DIRECTIVES),
            error.error_data_types);
}

class SyncerProtoUtilTest : public testing::Test {
 public:
  void SetUp() override {
    context_ = std::make_unique<SyncCycleContext>(
        /*connection_manager=*/nullptr,
        /*extensions_activity=*/nullptr,
        /*listeners=*/std::vector<SyncEngineEventListener*>(),
        /*debug_info_getter=*/nullptr,
        /*model_type_registry=*/nullptr,
        /*invalidator_client_id=*/"",
        /*cache_guid=*/"",
        /*birthday=*/"",
        /*bag_of_chips=*/"",
        /*poll_internal=*/base::Seconds(1));
  }

  SyncCycleContext* context() { return context_.get(); }

  // Helper function to call GetProtocolErrorFromResponse. Allows not adding
  // individual tests as friends to SyncerProtoUtil.
  static SyncProtocolError CallGetProtocolErrorFromResponse(
      const sync_pb::ClientToServerResponse& response,
      SyncCycleContext* context) {
    return SyncerProtoUtil::GetProtocolErrorFromResponse(response, context);
  }

 protected:
  std::unique_ptr<SyncCycleContext> context_;
};

TEST_F(SyncerProtoUtilTest, VerifyResponseBirthday) {
  // Both sides empty
  ASSERT_TRUE(context()->birthday().empty());
  sync_pb::ClientToServerResponse response;
  SyncProtocolError sync_protocol_error;
  response.set_error_code(sync_pb::SyncEnums::SUCCESS);

  sync_protocol_error = CallGetProtocolErrorFromResponse(response, context());
  EXPECT_EQ(NOT_MY_BIRTHDAY, sync_protocol_error.error_type);
  EXPECT_EQ(DISABLE_SYNC_ON_CLIENT, sync_protocol_error.action);

  // Remote set, local empty
  response.set_store_birthday("flan");
  sync_protocol_error = CallGetProtocolErrorFromResponse(response, context());
  EXPECT_EQ(SYNC_SUCCESS, sync_protocol_error.error_type);
  EXPECT_EQ(UNKNOWN_ACTION, sync_protocol_error.action);
  EXPECT_EQ(context()->birthday(), "flan");

  // Remote empty, local set.
  response.clear_store_birthday();
  sync_protocol_error = CallGetProtocolErrorFromResponse(response, context());
  EXPECT_EQ(SYNC_SUCCESS, sync_protocol_error.error_type);
  EXPECT_EQ(UNKNOWN_ACTION, sync_protocol_error.action);
  EXPECT_EQ(context()->birthday(), "flan");

  // Doesn't match
  response.set_store_birthday("meat");
  response.set_error_code(sync_pb::SyncEnums::NOT_MY_BIRTHDAY);
  sync_protocol_error = CallGetProtocolErrorFromResponse(response, context());
  EXPECT_EQ(NOT_MY_BIRTHDAY, sync_protocol_error.error_type);
  EXPECT_EQ(DISABLE_SYNC_ON_CLIENT, sync_protocol_error.action);

  // Doesn't match. CLIENT_DATA_OBSOLETE error is set.
  response.set_error_code(sync_pb::SyncEnums::CLIENT_DATA_OBSOLETE);
  sync_protocol_error = CallGetProtocolErrorFromResponse(response, context());
  EXPECT_EQ(CLIENT_DATA_OBSOLETE, sync_protocol_error.error_type);
  EXPECT_EQ(RESET_LOCAL_SYNC_DATA, sync_protocol_error.action);
}

TEST_F(SyncerProtoUtilTest, VerifyDisabledByAdmin) {
  // No error code
  sync_pb::ClientToServerResponse response;
  SyncProtocolError sync_protocol_error;
  context()->set_birthday("flan");
  response.set_error_code(sync_pb::SyncEnums::SUCCESS);

  sync_protocol_error = CallGetProtocolErrorFromResponse(response, context());
  ASSERT_EQ(SYNC_SUCCESS, sync_protocol_error.error_type);
  ASSERT_EQ(UNKNOWN_ACTION, sync_protocol_error.action);

  // Has error code, but not disabled
  response.set_error_code(sync_pb::SyncEnums::NOT_MY_BIRTHDAY);
  sync_protocol_error = CallGetProtocolErrorFromResponse(response, context());
  EXPECT_EQ(NOT_MY_BIRTHDAY, sync_protocol_error.error_type);
  EXPECT_NE(UNKNOWN_ACTION, sync_protocol_error.action);

  // Has error code, and is disabled by admin
  response.set_error_code(sync_pb::SyncEnums::DISABLED_BY_ADMIN);
  sync_protocol_error = CallGetProtocolErrorFromResponse(response, context());
  EXPECT_EQ(DISABLED_BY_ADMIN, sync_protocol_error.error_type);
  EXPECT_EQ(STOP_SYNC_FOR_DISABLED_ACCOUNT, sync_protocol_error.action);
}

TEST_F(SyncerProtoUtilTest, VerifyUpgradeClient) {
  ASSERT_TRUE(context()->birthday().empty());
  sync_pb::ClientToServerResponse response;
  response.set_error_code(sync_pb::SyncEnums::SUCCESS);
  response.mutable_error()->set_error_type(sync_pb::SyncEnums::THROTTLED);
  response.mutable_error()->set_action(sync_pb::SyncEnums::UPGRADE_CLIENT);
  response.mutable_error()->set_error_description(
      "Legacy client needs to be upgraded.");

  SyncProtocolError sync_protocol_error =
      CallGetProtocolErrorFromResponse(response, context());
  EXPECT_EQ(THROTTLED, sync_protocol_error.error_type);
  EXPECT_EQ(UPGRADE_CLIENT, sync_protocol_error.action);
}

TEST_F(SyncerProtoUtilTest, VerifyEncryptionObsolete) {
  sync_pb::ClientToServerResponse response;
  response.set_error_code(sync_pb::SyncEnums::ENCRYPTION_OBSOLETE);
  response.set_store_birthday("flan");

  SyncProtocolError sync_protocol_error =
      CallGetProtocolErrorFromResponse(response, context());
  EXPECT_EQ(ENCRYPTION_OBSOLETE, sync_protocol_error.error_type);
  EXPECT_EQ(DISABLE_SYNC_ON_CLIENT, sync_protocol_error.action);
}

class DummyConnectionManager : public ServerConnectionManager {
 public:
  DummyConnectionManager() = default;

  HttpResponse PostBuffer(const std::string& buffer_in,
                          const std::string& access_token,
                          bool allow_batching,
                          std::string* buffer_out) override {
    if (send_error_) {
      return HttpResponse::ForIoError();
    }

    sync_pb::ClientToServerResponse client_to_server_response;
    client_to_server_response.SerializeToString(buffer_out);

    return HttpResponse::ForSuccess();
  }

  void set_send_error(bool send) { send_error_ = send; }

 private:
  bool send_error_ = false;
};

TEST_F(SyncerProtoUtilTest, PostAndProcessHeaders) {
  DummyConnectionManager dcm;
  ClientToServerMessage msg;
  SyncerProtoUtil::SetProtocolVersion(&msg);
  msg.set_share("required");
  msg.set_message_contents(ClientToServerMessage::GET_UPDATES);
  sync_pb::ClientToServerResponse response;

  base::HistogramTester histogram_tester;
  dcm.set_send_error(true);
  EXPECT_FALSE(SyncerProtoUtil::PostAndProcessHeaders(&dcm, msg, &response));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Sync.PostedClientToServerMessage",
                   /*sample=*/ClientToServerMessage::GET_UPDATES));

  dcm.set_send_error(false);
  EXPECT_TRUE(SyncerProtoUtil::PostAndProcessHeaders(&dcm, msg, &response));
  EXPECT_EQ(2, histogram_tester.GetBucketCount(
                   "Sync.PostedClientToServerMessage",
                   /*sample=*/ClientToServerMessage::GET_UPDATES));
}

}  // namespace syncer
