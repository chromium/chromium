// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/create_session_request.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
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
#include "google_apis/gaia/gaia_id.h"
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
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Truly;

namespace {
const char kTestUserAgent[] = "test-user-agent";
const char kMaxStudentsExceededErrorMessage[] =
    "session.roster may not contain more than 100 students";

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

  static std::unique_ptr<HttpResponse> CreateMaxStudentsFailedResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_BAD_REQUEST);
    const std::string_view content = R"({
      "error": {
        "code": 400,
        "message": "%s",
        "status": "INVALID_ARGUMENT"
      } })";
    response->set_content(
        absl::StrFormat(content, kMaxStudentsExceededErrorMessage));
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

  void ExpectSuccessfulRequest(net::test_server::HttpRequest* http_request) {
    EXPECT_CALL(request_handler(), HandleRequest(_))
        .WillOnce(
            DoAll(SaveArg<0>(http_request),
                  Return(MockRequestHandler::CreateSuccessfulResponse())));
  }

  std::unique_ptr<CreateSessionRequest> CreateRequest(
      CreateSessionCallback callback,
      std::optional<::boca::UserIdentity> teacher = std::nullopt) {
    ::boca::UserIdentity default_teacher;
    default_teacher.set_gaia_id("1");
    ::boca::UserIdentity actual_teacher = teacher.value_or(default_teacher);
    base::TimeDelta session_duration = base::Seconds(120);
    auto request = std::make_unique<CreateSessionRequest>(
        request_sender(), "https://test", actual_teacher, session_duration,
        ::boca::Session::SessionState::Session_SessionState_ACTIVE,
        std::move(callback));
    request->OverrideURLForTesting(test_server_.base_url().spec());
    return request;
  }

 protected:
  using CreateSessionResult =
      base::expected<std::unique_ptr<::boca::Session>,
                     std::pair<google_apis::ApiErrorCode, std::string>>;

  net::EmbeddedTestServer test_server_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<google_apis::RequestSender> request_sender_;
  testing::StrictMock<MockRequestHandler> request_handler_;
  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_loader_factory_;
};

TEST_F(SessionApiRequestsTest, CreateSessionWithFullInputAndSucceed) {
  net::test_server::HttpRequest http_request;
  ExpectSuccessfulRequest(&http_request);
  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  teacher.set_email("teacher@gmail.com");
  teacher.set_full_name("teacher");
  base::test::TestFuture<CreateSessionResult> future;

  auto request = CreateRequest(future.GetCallback(), teacher);

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
      "{\"duration\":{\"seconds\":120},\"joinCode\":{\"enabled\":true},"
      "\"roster\":{\"studentGroups\":[{\"students\":[{\"email\":\"cat@gmail."
      "com\",\"fullName\":\"cat\",\"gaiaId\":\"2\",\"photoUrl\":\"data:image/"
      "123\"},{\"email\":\"dog@gmail.com\",\"fullName\":\"dog\",\"gaiaId\":"
      "\"3\",\"photoUrl\":\"data:image/"
      "123\"}],\"title\":\"main\"},{\"groupSource\":2,\"title\":\"accessCode\"}"
      "]},\"sessionState\":2,\"studentGroupConfigs\":{\"accessCode\":{"
      "\"captionsConfig\":{\"captionsEnabled\":true,\"translationsEnabled\":"
      "true},\"onTaskConfig\":{\"activeBundle\":{\"contentConfigs\":[{"
      "\"faviconUrl\":\"data:image/"
      "123\",\"lockedNavigationOptions\":{\"navigationType\":1},\"title\":"
      "\"google\",\"url\":\"https://google.com\"},{\"faviconUrl\":\"data:image/"
      "123\",\"lockedNavigationOptions\":{\"navigationType\":2},\"title\":"
      "\"youtube\",\"url\":\"https://"
      "youtube.com\"}],\"lockToAppHome\":false,\"locked\":true}}},\"main\":{"
      "\"captionsConfig\":{\"captionsEnabled\":true,\"translationsEnabled\":"
      "true},\"onTaskConfig\":{\"activeBundle\":{\"contentConfigs\":[{"
      "\"faviconUrl\":\"data:image/"
      "123\",\"lockedNavigationOptions\":{\"navigationType\":1},\"title\":"
      "\"google\",\"url\":\"https://google.com\"},{\"faviconUrl\":\"data:image/"
      "123\",\"lockedNavigationOptions\":{\"navigationType\":2},\"title\":"
      "\"youtube\",\"url\":\"https://"
      "youtube.com\"}],\"lockToAppHome\":false,\"locked\":true}}}},\"teacher\":"
      "{\"email\":\"teacher@gmail.com\",\"fullName\":\"teacher\",\"gaiaId\":"
      "\"1\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_EQ(true, result.has_value());
}

TEST_F(SessionApiRequestsTest, CreateSessionWithCriticalInputAndSucceed) {
  net::test_server::HttpRequest http_request;
  ExpectSuccessfulRequest(&http_request);
  base::test::TestFuture<CreateSessionResult> future;

  auto request = CreateRequest(future.GetCallback());

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions", http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"duration\":{\"seconds\":120},\"joinCode\":{\"enabled\":true},"
      "\"sessionState\":2,\"studentGroupConfigs\":{\"accessCode\":{},\"main\":{"
      "}},\"teacher\":{\"email\":\"\",\"fullName\":\"\",\"gaiaId\":\"1\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_EQ(true, result.has_value());
}

TEST_F(SessionApiRequestsTest, CreateSessionWithCriticalInputAndFail) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateFailedResponse())));

  base::test::TestFuture<CreateSessionResult> future;

  auto request = CreateRequest(future.GetCallback());

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions", http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"duration\":{\"seconds\":120},\"joinCode\":{\"enabled\":true},"
      "\"sessionState\":2,\"studentGroupConfigs\":{\"accessCode\":{},\"main\":{"
      "}},\"teacher\":{\"email\":\"\",\"fullName\":\"\",\"gaiaId\":\"1\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  const google_apis::ApiErrorCode error_code = result.error().first;
  EXPECT_EQ(google_apis::HTTP_INTERNAL_SERVER_ERROR, error_code);
}

TEST_F(SessionApiRequestsTest, CreateSessionWithTooManyStudentsAndFail) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(
          DoAll(SaveArg<0>(&http_request),
                Return(MockRequestHandler::CreateMaxStudentsFailedResponse())));

  base::test::TestFuture<CreateSessionResult> future;

  auto request = CreateRequest(future.GetCallback());

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  const google_apis::ApiErrorCode error_code = result.error().first;
  const std::string& error_msg = result.error().second;
  EXPECT_EQ(google_apis::HTTP_BAD_REQUEST, error_code);
  EXPECT_EQ(kMaxStudentsExceededErrorMessage, error_msg);
}

struct CreateSessionUrlTypeTestParam {
  std::string test_name;
  ::boca::UrlType url_type;
};

class CreateSessionUrlTypeTest
    : public SessionApiRequestsTest,
      public testing::WithParamInterface<CreateSessionUrlTypeTestParam> {};

TEST_P(CreateSessionUrlTypeTest, CreateSessionWithUrlTypeAndSucceed) {
  net::test_server::HttpRequest http_request;
  ExpectSuccessfulRequest(&http_request);
  base::test::TestFuture<CreateSessionResult> future;

  auto request = CreateRequest(future.GetCallback());

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
  content_config_2->set_title("special");
  content_config_2->set_url("https://specialurl.com");
  content_config_2->set_favicon_url("data:image/123");
  content_config_2->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions_NavigationType::
          LockedNavigationOptions_NavigationType_BLOCK_NAVIGATION);
  content_config_2->set_url_type(GetParam().url_type);

  request->set_on_task_config(std::move(on_task_config));

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  std::optional<base::Value> actual_root =
      base::JSONReader::Read(http_request.content, base::JSON_PARSE_RFC);
  ASSERT_TRUE(actual_root.has_value() && actual_root->is_dict());

  auto* content_configs = actual_root->GetDict().FindListByDottedPath(
      "studentGroupConfigs.main.onTaskConfig.activeBundle.contentConfigs");
  ASSERT_TRUE(content_configs);
  ASSERT_EQ(content_configs->size(), 2u);

  auto url_type_val = (*content_configs)[1].GetIfDict()->FindInt("urlType");
  ASSERT_TRUE(url_type_val.has_value());
  EXPECT_EQ(url_type_val.value(), GetParam().url_type);
}

INSTANTIATE_TEST_SUITE_P(
    CreateSessionUrlTypeTests,
    CreateSessionUrlTypeTest,
    testing::Values(
        CreateSessionUrlTypeTestParam{"GeminiRegular",
                                      ::boca::URL_TYPE_GEMINI_REGULAR},
        CreateSessionUrlTypeTestParam{"GeminiGuidedLearning",
                                      ::boca::URL_TYPE_GEMINI_GUIDED_LEARNING}),
    [](const testing::TestParamInfo<CreateSessionUrlTypeTest::ParamType>&
           info) { return info.param.test_name; });

}  // namespace ash::boca
