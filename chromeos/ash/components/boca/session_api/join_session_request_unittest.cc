// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/join_session_request.h"

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/update_session_request.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/test_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpMethod;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Truly;

namespace {
const char kTestUserAgent[] = "test-user-agent";

class MockRequestHandler {
 public:
  static std::unique_ptr<HttpResponse> CreateSuccessfulResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(R"(
     {
    "sessionId": "111",
    "duration": {
        "seconds": 120
    },
    "studentStatuses": {},
    "roster": {
        "studentGroups": []
    },
    "sessionState": "ACTIVE",
    "studentGroupConfigs": {
        "main": {
            "captionsConfig": {},
            "onTaskConfig": {
                "activeBundle": {
                    "contentConfigs": []
                }
            }
        }
    },
    "teacher": {
        "gaiaId": "1"
    }
}
       )");
    return response;
  }

  static std::unique_ptr<HttpResponse> CreateFailedResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }

  MOCK_METHOD(std::unique_ptr<HttpResponse>,
              HandleRequest,
              (const HttpRequest&));
};

}  // namespace

namespace ash::boca {

class JoinSessionTest : public testing::Test {
 public:
  JoinSessionTest() = default;
  void SetUp() override {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
            /*network_service=*/nullptr,
            /*is_trusted=*/true);
    request_sender_ = std::make_unique<google_apis::RequestSender>(
        std::make_unique<google_apis::DummyAuthService>(),
        test_shared_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(), kTestUserAgent,
        TRAFFIC_ANNOTATION_FOR_TESTS);

    test_server_.RegisterRequestHandler(
        base::BindRepeating(&MockRequestHandler::HandleRequest,
                            base::Unretained(&request_handler_)));

    ASSERT_TRUE(test_server_.Start());
  }

  MockRequestHandler& request_handler() { return request_handler_; }
  google_apis::RequestSender* request_sender() { return request_sender_.get(); }

 protected:
  // net::test_server::HttpRequest http_request;
  net::EmbeddedTestServer test_server_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<google_apis::RequestSender> request_sender_;
  testing::StrictMock<MockRequestHandler> request_handler_;
  std::unique_ptr<GaiaUrlsOverriderForTesting> urls_overrider_;
  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_loader_factory_;
};

TEST_F(JoinSessionTest, JoinSessionWithAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  ::boca::UserIdentity student;
  student.set_gaia_id("1");
  student.set_full_name("Student one");
  student.set_email("user@gmail.com");
  student.set_photo_url("http://photo");

  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;

  std::unique_ptr<JoinSessionRequest> request =
      std::make_unique<JoinSessionRequest>(request_sender(), std::move(student),
                                           "deviceId", "code",
                                           future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/students/1/session:join", http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"deviceInfo\":{\"deviceId\":\"deviceId\"},\"sessionJoinCode\":"
      "\"code\",\"student\":{\"email\":\"user@gmail.com\",\"fullName\":"
      "\"Student one\",\"gaiaId\":\"1\",\"photoUrl\":\"http://photo\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_TRUE(result.has_value());
}

TEST_F(JoinSessionTest, JoinSessionAndFail) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateFailedResponse())));
  ::boca::UserIdentity student;
  student.set_gaia_id("1");
  student.set_full_name("Student one");
  student.set_email("user@gmail.com");
  student.set_photo_url("http://photo");

  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;

  std::unique_ptr<JoinSessionRequest> request =
      std::make_unique<JoinSessionRequest>(request_sender(), std::move(student),
                                           "deviceId", "code",
                                           future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/students/1/session:join", http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"deviceInfo\":{\"deviceId\":\"deviceId\"},\"sessionJoinCode\":"
      "\"code\",\"student\":{\"email\":\"user@gmail.com\",\"fullName\":"
      "\"Student one\",\"gaiaId\":\"1\",\"photoUrl\":\"http://photo\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_EQ(google_apis::HTTP_INTERNAL_SERVER_ERROR, result.error());
}

}  // namespace ash::boca
