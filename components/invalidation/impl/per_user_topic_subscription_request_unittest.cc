// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_subscription_request.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/values.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace invalidation {

namespace {

using testing::_;
using testing::DoAll;
using testing::SaveArg;

using RequestType = PerUserTopicSubscriptionRequest::RequestType;

network::mojom::URLResponseHeadPtr CreateHeadersForTest(int responce_code) {
  auto head = network::mojom::URLResponseHead::New();
  head->headers = new net::HttpResponseHeaders(base::StringPrintf(
      "HTTP/1.1 %d OK\nContent-type: text/html\n\n", responce_code));
  head->mime_type = "text/html";
  return head;
}

}  // namespace

class PerUserTopicSubscriptionRequestTest : public testing::Test {
 public:
  PerUserTopicSubscriptionRequestTest() = default;
  ~PerUserTopicSubscriptionRequestTest() override = default;

  GURL url(PerUserTopicSubscriptionRequest* request) {
    return request->GetUrlForTesting();
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(PerUserTopicSubscriptionRequestTest,
       ShouldNotInvokeCallbackWhenCancelled) {
  const std::string token = "1234567890";
  const std::string url = "http://valid-url.test";
  const std::string topic = "test";
  const std::string project_id = "smarty-pants-12345";
  const RequestType type = RequestType::kSubscribe;

  base::MockCallback<PerUserTopicSubscriptionRequest::CompletedCallback>
      callback;
  EXPECT_CALL(callback, Run(_, _)).Times(0);

  PerUserTopicSubscriptionRequest::Builder builder;
  std::unique_ptr<PerUserTopicSubscriptionRequest> request =
      builder.SetInstanceIdToken(token)
          .SetScope(url)
          .SetPublicTopicName(topic)
          .SetProjectId(project_id)
          .SetType(type)
          .Build();
  request->Start(callback.Get(), url_loader_factory());
  base::RunLoop().RunUntilIdle();

  // Destroy the request before getting any response.
  request.reset();
}

TEST_F(PerUserTopicSubscriptionRequestTest, ShouldSubscribeWithoutErrors) {
  const std::string token = "1234567890";
  const std::string base_url = "http://valid-url.test";
  const std::string topic = "test";
  const std::string project_id = "smarty-pants-12345";
  const RequestType type = RequestType::kSubscribe;

  base::MockCallback<PerUserTopicSubscriptionRequest::CompletedCallback>
      callback;
  Status status(StatusCode::FAILED, "initial");
  std::string private_topic;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce([&](Status status_arg, std::string private_topic_arg) {
        status = status_arg;
        private_topic = private_topic_arg;
        run_loop.Quit();
      });

  PerUserTopicSubscriptionRequest::Builder builder;
  std::unique_ptr<PerUserTopicSubscriptionRequest> request =
      builder.SetInstanceIdToken(token)
          .SetScope(base_url)
          .SetPublicTopicName(topic)
          .SetProjectId(project_id)
          .SetType(type)
          .Build();
  std::string response_body = R"(
    {
      "privateTopicName": "test-pr"
    }
  )";

  network::URLLoaderCompletionStatus response_status(net::OK);
  response_status.decoded_body_length = response_body.size();

  url_loader_factory()->AddResponse(url(request.get()),
                                    CreateHeadersForTest(net::HTTP_OK),
                                    response_body, response_status);
  request->Start(callback.Get(), url_loader_factory());
  run_loop.Run();

  EXPECT_EQ(status.code, StatusCode::SUCCESS);
  EXPECT_EQ(private_topic, "test-pr");
}

TEST_F(PerUserTopicSubscriptionRequestTest,
       ShouleNotSubscribeWhenNetworkProblem) {
  const std::string token = "1234567890";
  const std::string base_url = "http://valid-url.test";
  const std::string topic = "test";
  const std::string project_id = "smarty-pants-12345";
  const RequestType type = RequestType::kSubscribe;

  base::MockCallback<PerUserTopicSubscriptionRequest::CompletedCallback>
      callback;
  Status status(StatusCode::FAILED, "initial");
  std::string private_topic;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce([&](Status status_arg, std::string private_topic_arg) {
        status = status_arg;
        private_topic = private_topic_arg;
        run_loop.Quit();
      });

  PerUserTopicSubscriptionRequest::Builder builder;
  std::unique_ptr<PerUserTopicSubscriptionRequest> request =
      builder.SetInstanceIdToken(token)
          .SetScope(base_url)
          .SetPublicTopicName(topic)
          .SetProjectId(project_id)
          .SetType(type)
          .Build();
  std::string response_body = R"(
    {
      "privateTopicName": "test-pr"
    }
  )";

  network::URLLoaderCompletionStatus response_status(net::ERR_TIMED_OUT);
  response_status.decoded_body_length = response_body.size();

  url_loader_factory()->AddResponse(url(request.get()),
                                    CreateHeadersForTest(net::HTTP_OK),
                                    response_body, response_status);
  request->Start(callback.Get(), url_loader_factory());
  run_loop.Run();

  EXPECT_EQ(status.code, StatusCode::FAILED);
}

TEST_F(PerUserTopicSubscriptionRequestTest,
       ShouldNotSubscribeWhenWrongResponse) {
  const std::string token = "1234567890";
  const std::string base_url = "http://valid-url.test";
  const std::string topic = "test";
  const std::string project_id = "smarty-pants-12345";
  const RequestType type = RequestType::kSubscribe;

  base::MockCallback<PerUserTopicSubscriptionRequest::CompletedCallback>
      callback;
  Status status(StatusCode::SUCCESS, "initial");
  std::string private_topic;

  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce([&](Status status_arg, std::string private_topic_arg) {
        status = status_arg;
        private_topic = private_topic_arg;
        run_loop.Quit();
      });

  PerUserTopicSubscriptionRequest::Builder builder;
  std::unique_ptr<PerUserTopicSubscriptionRequest> request =
      builder.SetInstanceIdToken(token)
          .SetScope(base_url)
          .SetPublicTopicName(topic)
          .SetProjectId(project_id)
          .SetType(type)
          .Build();
  std::string response_body = R"(
    {}
  )";

  network::URLLoaderCompletionStatus response_status(net::OK);
  response_status.decoded_body_length = response_body.size();

  url_loader_factory()->AddResponse(url(request.get()),
                                    CreateHeadersForTest(net::HTTP_OK),
                                    response_body, response_status);
  request->Start(callback.Get(), url_loader_factory());
  run_loop.Run();

  EXPECT_EQ(status.code, StatusCode::FAILED);
  EXPECT_EQ(status.message, "Missing topic name");
}

TEST_F(PerUserTopicSubscriptionRequestTest, ShouldUnsubscribe) {
  const std::string token = "1234567890";
  const std::string base_url = "http://valid-url.test";
  const std::string topic = "test";
  const std::string project_id = "smarty-pants-12345";
  const RequestType type = RequestType::kUnsubscribe;

  base::MockCallback<PerUserTopicSubscriptionRequest::CompletedCallback>
      callback;
  Status status(StatusCode::FAILED, "initial");
  std::string private_topic;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce([&](Status status_arg, std::string private_topic_arg) {
        status = status_arg;
        private_topic = private_topic_arg;
        run_loop.Quit();
      });

  PerUserTopicSubscriptionRequest::Builder builder;
  std::unique_ptr<PerUserTopicSubscriptionRequest> request =
      builder.SetInstanceIdToken(token)
          .SetScope(base_url)
          .SetPublicTopicName(topic)
          .SetProjectId(project_id)
          .SetType(type)
          .Build();
  std::string response_body = R"(
    {}
  )";

  network::URLLoaderCompletionStatus response_status(net::OK);
  response_status.decoded_body_length = response_body.size();

  url_loader_factory()->AddResponse(url(request.get()),
                                    CreateHeadersForTest(net::HTTP_OK),
                                    response_body, response_status);
  request->Start(callback.Get(), url_loader_factory());
  run_loop.Run();

  EXPECT_EQ(status.code, StatusCode::SUCCESS);
  EXPECT_EQ(status.message, std::string());
}

// Regression test for crbug.com/1054590, |completed_callback| destroys
// |request|.
TEST_F(PerUserTopicSubscriptionRequestTest, ShouldDestroyOnFailure) {
  const std::string token = "1234567890";
  const std::string base_url = "http://valid-url.test";
  const std::string topic = "test";
  const std::string project_id = "smarty-pants-12345";
  const RequestType type = RequestType::kSubscribe;

  std::unique_ptr<PerUserTopicSubscriptionRequest> request;
  base::RunLoop run_loop;
  bool callback_called = false;
  auto completed_callback = base::BindLambdaForTesting(
      [&](const Status& status, const std::string& topic_name) {
        request.reset();
        callback_called = true;
        run_loop.Quit();
      });

  PerUserTopicSubscriptionRequest::Builder builder;
  request = builder.SetInstanceIdToken(token)
                .SetScope(base_url)
                .SetPublicTopicName(topic)
                .SetProjectId(project_id)
                .SetType(type)
                .Build();
  std::string response_body = R"(
    {
      "privateTopicName": "test-pr"
    }
  )";

  network::URLLoaderCompletionStatus response_status(net::ERR_TIMED_OUT);
  response_status.decoded_body_length = response_body.size();

  url_loader_factory()->AddResponse(url(request.get()),
                                    CreateHeadersForTest(net::HTTP_OK),
                                    response_body, response_status);
  request->Start(std::move(completed_callback), url_loader_factory());
  run_loop.Run();

  EXPECT_TRUE(callback_called);
  // The main expectation is that there is no crash.
}

class PerUserTopicSubscriptionRequestParamTest
    : public PerUserTopicSubscriptionRequestTest,
      public testing::WithParamInterface<net::HttpStatusCode> {
 public:
  PerUserTopicSubscriptionRequestParamTest() = default;

  PerUserTopicSubscriptionRequestParamTest(
      const PerUserTopicSubscriptionRequestParamTest&) = delete;
  PerUserTopicSubscriptionRequestParamTest& operator=(
      const PerUserTopicSubscriptionRequestParamTest&) = delete;

  ~PerUserTopicSubscriptionRequestParamTest() override = default;
};

TEST_P(PerUserTopicSubscriptionRequestParamTest,
       ShouldNotSubscribeWhenNonRepeatableError) {
  const std::string token = "1234567890";
  const std::string base_url = "http://valid-url.test";
  const std::string topic = "test";
  const std::string project_id = "smarty-pants-12345";
  const RequestType type = RequestType::kSubscribe;

  base::MockCallback<PerUserTopicSubscriptionRequest::CompletedCallback>
      callback;
  Status status(StatusCode::FAILED, "initial");
  std::string private_topic;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce([&](Status status_arg, std::string private_topic_arg) {
        status = status_arg;
        private_topic = private_topic_arg;
        run_loop.Quit();
      });

  PerUserTopicSubscriptionRequest::Builder builder;
  std::unique_ptr<PerUserTopicSubscriptionRequest> request =
      builder.SetInstanceIdToken(token)
          .SetScope(base_url)
          .SetPublicTopicName(topic)
          .SetProjectId(project_id)
          .SetType(type)
          .Build();
  network::URLLoaderCompletionStatus response_status(net::OK);

  url_loader_factory()->AddResponse(
      url(request.get()), CreateHeadersForTest(GetParam()),
      /* response_body */ std::string(), response_status);
  request->Start(callback.Get(), url_loader_factory());
  run_loop.Run();

  EXPECT_EQ(status.code, StatusCode::FAILED_NON_RETRIABLE);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PerUserTopicSubscriptionRequestParamTest,
                         testing::Values(net::HTTP_BAD_REQUEST,
                                         net::HTTP_FORBIDDEN,
                                         net::HTTP_NOT_FOUND));

}  // namespace invalidation
