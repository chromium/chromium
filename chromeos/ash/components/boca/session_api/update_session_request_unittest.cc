// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/update_session_request.h"

#include <memory>

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

  static std::unique_ptr<HttpResponse> CreateResponseWithUrlType(
      const std::string& url_type) {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(base::ReplaceStringPlaceholders(
        R"(
          {
            "sessionId": "111",
            "duration": {
              "seconds": 120
            },
            "sessionState": "ACTIVE",
            "studentGroupConfigs": {
              "main": {
                "onTaskConfig": {
                  "activeBundle": {
                    "contentConfigs": [
                      {
                        "title": "gemini",
                        "url": "https://gemini.google.com",
                        "urlType": "$1"
                      }
                    ]
                  }
                }
              }
            },
            "teacher": {
              "gaiaId": "1"
            }
          }
        )",
        {url_type}, nullptr));
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

class UpdateSessionTest : public testing::Test {
 public:
  using UpdateSessionResult = base::expected<std::unique_ptr<::boca::Session>,
                                             google_apis::ApiErrorCode>;
  UpdateSessionTest() = default;
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

TEST_F(UpdateSessionTest, UpdateSessionWithEndSessionInputAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  const char session_id[] = "sessionId";
  base::test::TestFuture<UpdateSessionResult> future;

  std::unique_ptr<UpdateSessionRequest> request =
      std::make_unique<UpdateSessionRequest>(request_sender(), "https://test",
                                             teacher, session_id,
                                             future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());
  request->set_session_state(
      std::make_unique<::boca::Session::SessionState>(::boca::Session::PAST));
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_PATCH, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions/sessionId?updateMask=sessionState",
            http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"sessionId\":\"sessionId\",\"sessionState\":3,\"teacher\":{\"gaiaId\":"
      "\"1\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_TRUE(result.has_value());
}

TEST_F(UpdateSessionTest, UpdateSessionWithExendSessionInputAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  const char session_id[] = "sessionId";
  base::test::TestFuture<UpdateSessionResult> future;

  std::unique_ptr<UpdateSessionRequest> request =
      std::make_unique<UpdateSessionRequest>(request_sender(), "https://test",
                                             teacher, session_id,
                                             future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());
  request->set_duration(std::make_unique<base::TimeDelta>(base::Minutes(2)));
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_PATCH, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions/sessionId?updateMask=duration",
            http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"duration\":{\"seconds\":120},\"sessionId\":\"sessionId\",\"teacher\":"
      "{\"gaiaId\":\"1\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_TRUE(result.has_value());
}

TEST_F(UpdateSessionTest,
       UpdateSessionWithUpdateStateAndDurationInputAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  const char session_id[] = "sessionId";
  base::test::TestFuture<UpdateSessionResult> future;

  std::unique_ptr<UpdateSessionRequest> request =
      std::make_unique<UpdateSessionRequest>(request_sender(), "https://test",
                                             teacher, session_id,
                                             future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());
  request->set_session_state(
      std::make_unique<::boca::Session::SessionState>(::boca::Session::PAST));
  request->set_duration(std::make_unique<base::TimeDelta>(base::Minutes(1)));
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_PATCH, http_request.method);

  EXPECT_EQ(
      "/v1/teachers/1/sessions/sessionId?updateMask=sessionState,duration",
      http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"duration\":{\"seconds\":60},\"sessionId\":\"sessionId\","
      "\"sessionState\":3,\"teacher\":{\"gaiaId\":\"1\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_TRUE(result.has_value());
}

TEST_F(UpdateSessionTest, UpdateSessionWithUpdateSessionConfigAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  const char session_id[] = "sessionId";
  base::test::TestFuture<UpdateSessionResult> future;

  std::unique_ptr<UpdateSessionRequest> request =
      std::make_unique<UpdateSessionRequest>(request_sender(), "https://test",
                                             teacher, session_id,
                                             future.GetCallback());

  request->OverrideURLForTesting(test_server_.base_url().spec());

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
  EXPECT_EQ(net::test_server::METHOD_PATCH, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions/sessionId?updateMask=studentGroupConfigs",
            http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"sessionId\":\"sessionId\",\"studentGroupConfigs\":{\"accessCode\":{"
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
      "{\"gaiaId\":\"1\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_TRUE(result.value());
}

TEST_F(UpdateSessionTest, UpdateSessionWithDurationAndFail) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateFailedResponse())));
  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  const char session_id[] = "sessionId";
  base::test::TestFuture<UpdateSessionResult> future;

  std::unique_ptr<UpdateSessionRequest> request =
      std::make_unique<UpdateSessionRequest>(request_sender(), "https://test",
                                             teacher, session_id,
                                             future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());
  request->set_duration(std::make_unique<base::TimeDelta>(base::Minutes(1)));
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_PATCH, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions/sessionId?updateMask=duration",
            http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"duration\":{\"seconds\":60},\"sessionId\":\"sessionId\",\"teacher\":{"
      "\"gaiaId\":\"1\"}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_EQ(google_apis::HTTP_INTERNAL_SERVER_ERROR, result.error());
}

struct UpdateSessionUrlTypeTestParam {
  std::string test_name;
  ::boca::UrlType url_type;
  std::string url_type_str;
};

class UpdateSessionUrlTypeTest
    : public UpdateSessionTest,
      public testing::WithParamInterface<UpdateSessionUrlTypeTestParam> {};

TEST_P(UpdateSessionUrlTypeTest, UpdateSessionWithUrlTypeAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateResponseWithUrlType(
                          GetParam().url_type_str))));
  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  const char session_id[] = "sessionId";
  base::test::TestFuture<UpdateSessionResult> future;

  std::unique_ptr<UpdateSessionRequest> request =
      std::make_unique<UpdateSessionRequest>(request_sender(), "https://test",
                                             teacher, session_id,
                                             future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());

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

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());

  // Verify outgoing JSON request content mapping.
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

  // Verify incoming response.
  std::unique_ptr<::boca::Session> session = std::move(result.value());
  ASSERT_TRUE(session->student_group_configs().contains(kMainStudentGroupName));
  auto response_content_config = std::move(session->student_group_configs()
                                               .at(kMainStudentGroupName)
                                               .on_task_config()
                                               .active_bundle()
                                               .content_configs());
  ASSERT_EQ(1, response_content_config.size());
  EXPECT_EQ("gemini", response_content_config[0].title());
  EXPECT_EQ("https://gemini.google.com", response_content_config[0].url());
  EXPECT_EQ(GetParam().url_type, response_content_config[0].url_type());
}

INSTANTIATE_TEST_SUITE_P(
    UpdateSessionUrlTypeTests,
    UpdateSessionUrlTypeTest,
    testing::Values(
        UpdateSessionUrlTypeTestParam{"GeminiRegular",
                                      ::boca::URL_TYPE_GEMINI_REGULAR,
                                      "URL_TYPE_GEMINI_REGULAR"},
        UpdateSessionUrlTypeTestParam{"GeminiGuidedLearning",
                                      ::boca::URL_TYPE_GEMINI_GUIDED_LEARNING,
                                      "URL_TYPE_GEMINI_GUIDED_LEARNING"}),
    [](const testing::TestParamInfo<UpdateSessionUrlTypeTest::ParamType>&
           info) { return info.param.test_name; });

}  // namespace ash::boca
