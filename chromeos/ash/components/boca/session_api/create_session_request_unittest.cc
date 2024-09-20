// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/create_session_request.h"

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

class SessionApiRequestsTest : public testing::Test {
 public:
  SessionApiRequestsTest() = default;
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

TEST_F(SessionApiRequestsTest, CreateSessionWithFullInputAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  teacher.set_email("teacher@gmail.com");
  teacher.set_full_name("teacher");
  base::TimeDelta session_duration = base::Seconds(120);
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;

  std::unique_ptr<CreateSessionRequest> request =
      std::make_unique<CreateSessionRequest>(
          request_sender(), teacher, session_duration,
          ::boca::Session::SessionState::Session_SessionState_ACTIVE,
          future.GetCallback());

  request->OverrideURLForTesting(test_server_.base_url().spec());

  auto roster = std::make_unique<::boca::Roster>();
  auto* student_groups = roster->mutable_student_groups()->Add();
  auto* student_1 = student_groups->mutable_students()->Add();
  student_1->set_gaia_id("2");
  student_1->set_email("cat@gmail.com");
  student_1->set_full_name("cat");
  student_1->set_photo_url("data:image/123");
  auto* student_2 = student_groups->mutable_students()->Add();
  student_2->set_gaia_id("3");
  student_2->set_email("dog@gmail.com");
  student_2->set_full_name("dog");
  student_2->set_photo_url("data:image/123");

  request->set_roster(std::move(roster));

  auto on_task_config = std::make_unique<::boca::OnTaskConfig>();
  auto* active_bundle = on_task_config->mutable_active_bundle();
  active_bundle->set_locked(true);

  auto* content_config_1 = active_bundle->mutable_content_configs()->Add();
  content_config_1->set_title("google");
  content_config_1->set_url("https://google.com");
  content_config_1->set_favicon_url("data:image/123");
  content_config_1->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType::
          LockedNavigationOptions_NavigationType_OPEN_NAVIGATION);

  auto* content_config_2 = active_bundle->mutable_content_configs()->Add();
  content_config_2->set_title("youtube");
  content_config_2->set_url("https://youtube.com");
  content_config_2->set_favicon_url("data:image/123");
  auto navigation_options_2 =
      std::make_unique<::boca::LockedNavigationOptions>();
  content_config_2->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType::
          LockedNavigationOptions_NavigationType_BLOCK_NAVIGATION);

  request->set_on_task_config(std::move(on_task_config));

  auto captions_config = std::make_unique<::boca::CaptionsConfig>();
  captions_config->set_captions_enabled(true);
  captions_config->set_translations_enabled(true);
  request->set_captions_config(std::move(captions_config));

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions", http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"duration\":{\"seconds\":120},\"roster\":{\"studentGroups\":{"
      "\"students\":[{\"email\":\"cat@gmail.com\",\"fullName\":\"cat\","
      "\"gaiaId\":\"2\",\"photoUrl\":\"data:image/"
      "123\"},{\"email\":\"dog@gmail.com\",\"fullName\":\"dog\",\"gaiaId\":"
      "\"3\",\"photoUrl\":\"data:image/"
      "123\"}],\"title\":\"main\"}},\"sessionState\":2,\"studentGroupConfigs\":"
      "{\"main\":{\"captionsConfig\":{\"captionsEnabled\":true,"
      "\"translationsEnabled\":true},\"onTaskConfig\":{\"activeBundle\":{"
      "\"contentConfigs\":[{\"faviconUrl\":\"data:image/"
      "123\",\"lockedNavigationOptions\":{\"navigationType\":1},\"title\":"
      "\"google\",\"url\":\"https://google.com\"},{\"faviconUrl\":\"data:image/"
      "123\",\"lockedNavigationOptions\":{\"navigationType\":2},\"title\":"
      "\"youtube\",\"url\":\"https://"
      "youtube.com\"}],\"locked\":true}}}},\"teacher\":{\"email\":\"teacher@"
      "gmail.com\",\"fullName\":\"teacher\",\"gaiaId\":\"1\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_EQ(true, result.has_value());
}

TEST_F(SessionApiRequestsTest, CreateSessionWithCriticalInputAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));

  std::string gaia_id = "1";
  base::TimeDelta session_duration = base::Seconds(120);

  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  std::unique_ptr<CreateSessionRequest> request =
      std::make_unique<CreateSessionRequest>(
          request_sender(), teacher, session_duration,
          ::boca::Session::SessionState::Session_SessionState_ACTIVE,
          future.GetCallback());

  request->OverrideURLForTesting(test_server_.base_url().spec());

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions", http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"duration\":{\"seconds\":120},\"sessionState\":2,\"studentGroup"
      "Configs\":{\"main\":{}},\"teacher\":{\"email\":\"\",\"fullName\":\"\","
      "\"gaiaId\":\"1\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_EQ(true, result.has_value());
}

TEST_F(SessionApiRequestsTest, CreateSessionWithCriticalInputAndFail) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateFailedResponse())));

  std::string gaia_id = "1";
  base::TimeDelta session_duration = base::Seconds(120);

  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;
  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  std::unique_ptr<CreateSessionRequest> request =
      std::make_unique<CreateSessionRequest>(
          request_sender(), teacher, session_duration,
          ::boca::Session::SessionState::Session_SessionState_ACTIVE,
          future.GetCallback());

  request->OverrideURLForTesting(test_server_.base_url().spec());

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions", http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"duration\":{\"seconds\":120},\"sessionState\":2,\"studentGroup"
      "Configs\":{\"main\":{}},\"teacher\":{\"email\":\"\",\"fullName\":\"\","
      "\"gaiaId\":\"1\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_EQ(google_apis::HTTP_INTERNAL_SERVER_ERROR, result.error());
}

}  // namespace ash::boca
