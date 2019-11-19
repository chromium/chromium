// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_channel_status_request.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/sync/protocol/experiment_status.pb.h"
#include "components/sync/protocol/experiments_specifics.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {
const char kTestURL[] = "http://channel.status.request.com/";
}

class GCMChannelStatusRequestTest : public testing::Test {
 public:
  GCMChannelStatusRequestTest();
  ~GCMChannelStatusRequestTest() override;

 protected:
  enum GCMStatus {
    NOT_SPECIFIED,
    GCM_ENABLED,
    GCM_DISABLED,
  };

  void CreateRequest();
  void SetResponseStatusAndString(net::HttpStatusCode status_code,
                                  const std::string& response_body);
  void SetResponseProtoData(GCMStatus status, int poll_interval_seconds);
  void StartRequest();
  void OnRequestCompleted(bool update_received,
                          bool enabled,
                          int poll_interval_seconds);
  network::TestURLLoaderFactory* test_url_loader_factory();

  std::unique_ptr<GCMChannelStatusRequest> request_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  bool request_callback_invoked_;
  bool update_received_;
  bool enabled_;
  int poll_interval_seconds_;
};

GCMChannelStatusRequestTest::GCMChannelStatusRequestTest()
    : test_shared_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      request_callback_invoked_(false),
      update_received_(false),
      enabled_(true),
      poll_interval_seconds_(0) {}

GCMChannelStatusRequestTest::~GCMChannelStatusRequestTest() {
}

void GCMChannelStatusRequestTest::CreateRequest() {
  request_.reset(new GCMChannelStatusRequest(
      test_shared_loader_factory_, kTestURL, "user agent string",
      base::Bind(&GCMChannelStatusRequestTest::OnRequestCompleted,
                 base::Unretained(this))));
  test_url_loader_factory_.ClearResponses();
}

void GCMChannelStatusRequestTest::SetResponseStatusAndString(
    net::HttpStatusCode status_code,
    const std::string& response_body) {
  test_url_loader_factory_.AddResponse(kTestURL, response_body, status_code);
}

void GCMChannelStatusRequestTest::SetResponseProtoData(
    GCMStatus status, int poll_interval_seconds) {
  sync_pb::ExperimentStatusResponse response_proto;
  if (status != NOT_SPECIFIED) {
    sync_pb::ExperimentsSpecifics* experiment_specifics =
        response_proto.add_experiment();
    experiment_specifics->mutable_gcm_channel()->set_enabled(status ==
                                                             GCM_ENABLED);
  }

  // Zero |poll_interval_seconds| means the optional field is not set.
  if (poll_interval_seconds)
    response_proto.set_poll_interval_seconds(poll_interval_seconds);

  std::string response_string;
  response_proto.SerializeToString(&response_string);
  SetResponseStatusAndString(net::HTTP_OK, response_string);
}

void GCMChannelStatusRequestTest::StartRequest() {
  request_->Start();
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void GCMChannelStatusRequestTest::OnRequestCompleted(
    bool update_received, bool enabled, int poll_interval_seconds) {
  request_callback_invoked_ = true;
  update_received_ = update_received;
  enabled_ = enabled;
  poll_interval_seconds_ = poll_interval_seconds;
}

network::TestURLLoaderFactory*
GCMChannelStatusRequestTest::test_url_loader_factory() {
  return &test_url_loader_factory_;
}

TEST_F(GCMChannelStatusRequestTest, RequestData) {
  CreateRequest();

  GURL intercepted_url;
  net::HttpRequestHeaders intercepted_headers;
  std::string upload_data;
  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        intercepted_url = request.url;
        intercepted_headers = request.headers;
        upload_data = network::GetUploadData(request);
      }));
  StartRequest();

  EXPECT_EQ(GURL(request_->channel_status_request_url_), intercepted_url);

  std::string user_agent_header;
  intercepted_headers.GetHeader("User-Agent", &user_agent_header);
  EXPECT_FALSE(user_agent_header.empty());
  EXPECT_EQ(request_->user_agent_, user_agent_header);

  EXPECT_FALSE(upload_data.empty());
  sync_pb::ExperimentStatusRequest proto_data;
  proto_data.ParseFromString(upload_data);
  EXPECT_EQ(1, proto_data.experiment_name_size());
  EXPECT_EQ("gcm_channel", proto_data.experiment_name(0));
}

TEST_F(GCMChannelStatusRequestTest, ResponseHttpStatusNotOK) {
  CreateRequest();
  SetResponseStatusAndString(net::HTTP_UNAUTHORIZED, "");
  StartRequest();

  EXPECT_FALSE(request_callback_invoked_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseEmpty) {
  CreateRequest();
  SetResponseStatusAndString(net::HTTP_OK, "");
  StartRequest();

  EXPECT_TRUE(request_callback_invoked_);
  EXPECT_FALSE(update_received_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseNotInProtoFormat) {
  CreateRequest();
  SetResponseStatusAndString(net::HTTP_OK, "foo");
  StartRequest();

  EXPECT_FALSE(request_callback_invoked_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseEmptyProtoData) {
  CreateRequest();
  SetResponseProtoData(NOT_SPECIFIED, 0);
  StartRequest();

  EXPECT_TRUE(request_callback_invoked_);
  EXPECT_FALSE(update_received_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseWithDisabledStatus) {
  CreateRequest();
  SetResponseProtoData(GCM_DISABLED, 0);
  StartRequest();

  EXPECT_TRUE(request_callback_invoked_);
  EXPECT_TRUE(update_received_);
  EXPECT_FALSE(enabled_);
  EXPECT_EQ(
      GCMChannelStatusRequest::default_poll_interval_seconds(),
      poll_interval_seconds_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseWithEnabledStatus) {
  CreateRequest();
  SetResponseProtoData(GCM_ENABLED, 0);
  StartRequest();

  EXPECT_TRUE(request_callback_invoked_);
  EXPECT_TRUE(update_received_);
  EXPECT_TRUE(enabled_);
  EXPECT_EQ(
      GCMChannelStatusRequest::default_poll_interval_seconds(),
      poll_interval_seconds_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseWithPollInterval) {
  // Setting a poll interval 15 minutes longer than the minimum interval we
  // enforce.
  int poll_interval_seconds =
      GCMChannelStatusRequest::min_poll_interval_seconds() + 15 * 60;
  CreateRequest();
  SetResponseProtoData(NOT_SPECIFIED, poll_interval_seconds);
  StartRequest();

  EXPECT_TRUE(request_callback_invoked_);
  EXPECT_TRUE(update_received_);
  EXPECT_TRUE(enabled_);
  EXPECT_EQ(poll_interval_seconds, poll_interval_seconds_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseWithShortPollInterval) {
  // Setting a poll interval 15 minutes shorter than the minimum interval we
  // enforce.
  int poll_interval_seconds =
      GCMChannelStatusRequest::min_poll_interval_seconds() - 15 * 60;
  CreateRequest();
  SetResponseProtoData(NOT_SPECIFIED, poll_interval_seconds);
  StartRequest();

  EXPECT_TRUE(request_callback_invoked_);
  EXPECT_TRUE(update_received_);
  EXPECT_TRUE(enabled_);
  EXPECT_EQ(GCMChannelStatusRequest::min_poll_interval_seconds(),
            poll_interval_seconds_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseWithDisabledStatusAndPollInterval) {
  int poll_interval_seconds =
      GCMChannelStatusRequest::min_poll_interval_seconds() + 15 * 60;
  CreateRequest();
  SetResponseProtoData(GCM_DISABLED, poll_interval_seconds);
  StartRequest();

  EXPECT_TRUE(request_callback_invoked_);
  EXPECT_TRUE(update_received_);
  EXPECT_FALSE(enabled_);
  EXPECT_EQ(poll_interval_seconds, poll_interval_seconds_);
}

}  // namespace gcm
