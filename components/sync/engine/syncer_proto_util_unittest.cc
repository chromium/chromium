// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/syncer_proto_util.h"

#include <memory>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/cycle/sync_cycle_context.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/data_type_test_util.h"
#include "components/sync/test/fake_sync_scheduler.h"
#include "components/sync/test/mock_connection_manager.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

using sync_pb::ClientToServerMessage;
using sync_pb::ClientToServerResponse;
using sync_pb::CommitResponse_EntryResponse;

namespace syncer {

namespace {

class MockSyncScheduler : public FakeSyncScheduler {
 public:
  MockSyncScheduler() = default;
  ~MockSyncScheduler() override = default;

  MOCK_METHOD(void,
              OnReceivedGuRetryDelay,
              (const base::TimeDelta&),
              (override));
};

ClientToServerResponse DefaultGetUpdatesResponse() {
  ClientToServerResponse response;
  response.set_store_birthday("birthday");
  response.set_error_code(sync_pb::SyncEnums::SUCCESS);
  return response;
}

ClientToServerMessage DefaultGetUpdatesRequest() {
  ClientToServerMessage msg;
  SyncerProtoUtil::SetProtocolVersion(&msg);
  msg.set_share("required");
  msg.set_message_contents(ClientToServerMessage::GET_UPDATES);
  msg.set_store_birthday("birthday");
  msg.mutable_bag_of_chips();
  msg.set_api_key("api_key");
  msg.mutable_client_status();

  return msg;
}

}  // namespace

// Builds a ClientToServerResponse with some data type ids, including
// invalid ones.  GetTypesToMigrate() should return only the valid
// data types.
TEST(SyncerProtoUtil, GetTypesToMigrate) {
  sync_pb::ClientToServerResponse response;
  response.add_migrated_data_type_id(
      GetSpecificsFieldNumberFromDataType(BOOKMARKS));
  response.add_migrated_data_type_id(
      GetSpecificsFieldNumberFromDataType(HISTORY_DELETE_DIRECTIVES));
  response.add_migrated_data_type_id(-1);
  EXPECT_EQ(DataTypeSet({BOOKMARKS, HISTORY_DELETE_DIRECTIVES}),
            GetTypesToMigrate(response));
}

// Builds a ClientToServerResponse_Error with some error data type
// ids, including invalid ones.  ConvertErrorPBToSyncProtocolError() should
// return a SyncProtocolError with only the valid data types.
TEST(SyncerProtoUtil, ConvertErrorPBToSyncProtocolError) {
  sync_pb::ClientToServerResponse_Error error_pb;
  error_pb.set_error_type(sync_pb::SyncEnums::THROTTLED);
  error_pb.add_error_data_type_ids(
      GetSpecificsFieldNumberFromDataType(BOOKMARKS));
  error_pb.add_error_data_type_ids(
      GetSpecificsFieldNumberFromDataType(HISTORY_DELETE_DIRECTIVES));
  error_pb.add_error_data_type_ids(-1);
  SyncProtocolError error = ConvertErrorPBToSyncProtocolError(error_pb);
  EXPECT_EQ(DataTypeSet({BOOKMARKS, HISTORY_DELETE_DIRECTIVES}),
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
        /*data_type_registry=*/nullptr,
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

class FakeConnectionManager : public ServerConnectionManager {
 public:
  explicit FakeConnectionManager(
      const sync_pb::ClientToServerResponse& response)
      : response_(response) {}

  HttpResponse PostBuffer(const std::string& buffer_in,
                          const std::string& access_token,
                          std::string* buffer_out) override {
    if (send_error_) {
      return HttpResponse::ForNetError(net::ERR_FAILED);
    }

    response_.SerializeToString(buffer_out);

    return HttpResponse::ForSuccessForTest();
  }

  void set_send_error(bool send) { send_error_ = send; }

 private:
  const sync_pb::ClientToServerResponse response_;
  bool send_error_ = false;
};

TEST_F(SyncerProtoUtilTest, PostAndProcessHeaders) {
  FakeConnectionManager dcm(ClientToServerResponse{});
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

TEST_F(SyncerProtoUtilTest, ShouldHandleGetUpdatesRetryDelay) {
  ClientToServerResponse response_to_return = DefaultGetUpdatesResponse();
  response_to_return.mutable_client_command()->set_gu_retry_delay_seconds(900);
  FakeConnectionManager dcm(response_to_return);

  testing::NiceMock<MockSyncScheduler> mock_sync_scheduler;
  EXPECT_CALL(mock_sync_scheduler, OnReceivedGuRetryDelay(base::Seconds(900)));

  SyncCycleContext context(&dcm,
                           /*extensions_activity=*/nullptr,
                           /*listeners=*/{},
                           /*debug_info_getter=*/nullptr,
                           /*data_type_registry=*/nullptr, "cache_guid",
                           "birthday",
                           /*bag_of_chips=*/"", base::Seconds(100));
  SyncCycle cycle(&context, &mock_sync_scheduler);

  ClientToServerResponse response;
  DataTypeSet partial_failure_data_types;
  SyncerError error = SyncerProtoUtil::PostClientToServerMessage(
      DefaultGetUpdatesRequest(), &response, &cycle,
      &partial_failure_data_types);
  EXPECT_EQ(error.type(), SyncerError::Type::kSuccess);
}

TEST_F(SyncerProtoUtilTest, ShouldIgnoreGetUpdatesRetryDelay) {
  base::test::ScopedFeatureList feature_overrides;
  feature_overrides.InitAndEnableFeature(
      syncer::kSyncIgnoreGetUpdatesRetryDelay);

  ClientToServerResponse response_to_return = DefaultGetUpdatesResponse();
  response_to_return.mutable_client_command()->set_gu_retry_delay_seconds(900);
  FakeConnectionManager dcm(response_to_return);

  // Verify that OnReceivedGuRetryDelay is not called despite
  // gu_retry_delay_seconds command.
  testing::NiceMock<MockSyncScheduler> mock_sync_scheduler;
  EXPECT_CALL(mock_sync_scheduler, OnReceivedGuRetryDelay).Times(0);

  SyncCycleContext context(&dcm,
                           /*extensions_activity=*/nullptr,
                           /*listeners=*/{},
                           /*debug_info_getter=*/nullptr,
                           /*data_type_registry=*/nullptr, "cache_guid",
                           "birthday",
                           /*bag_of_chips=*/"", base::Seconds(100));
  SyncCycle cycle(&context, &mock_sync_scheduler);

  ClientToServerResponse response;
  DataTypeSet partial_failure_data_types;
  SyncerError error = SyncerProtoUtil::PostClientToServerMessage(
      DefaultGetUpdatesRequest(), &response, &cycle,
      &partial_failure_data_types);
  EXPECT_EQ(error.type(), SyncerError::Type::kSuccess);
}

}  // namespace syncer
