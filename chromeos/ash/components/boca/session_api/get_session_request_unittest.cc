// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/ash/components/boca/session_api/get_session_request.h"

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
#include "net/http/http_status_code.h"
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
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Return;

namespace {
const char kTestUserAgent[] = "test-user-agent";

class MockRequestHandler {
 public:
  static std::unique_ptr<HttpResponse> CreateFullProducerResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(
        R"(
  {
  "startTime":{
    "seconds": "1723773909",
    "nanos": 1234
  },
  "sessionId": "111",
  "duration": {
    "seconds": "120"
  },
  "studentStatuses": {
    "2": {
      "state": "ADDED"
    },
    "3": {
      "state": "ACTIVE"
    }
  },
  "roster": {
    "studentGroups": [{
      "students": [
        {
          "email": "cat@gmail.com",
          "fullName": "cat",
          "gaiaId": "2",
          "photoUrl": "data:image/123"
        },
        {
          "email": "dog@gmail.com",
          "fullName": "dog",
          "gaiaId": "3",
          "photoUrl": "data:image/123"
        }
      ],
      "title": "main"
    }]
  },
  "sessionState": "ACTIVE",
  "studentGroupConfigs": {
    "main": {
      "captionsConfig": {
        "captionsEnabled": true,
        "translationsEnabled": true
      },
      "onTaskConfig": {
        "activeBundle": {
          "contentConfigs": [
            {
              "faviconUrl": "data:image/123",
              "lockedNavigationOptions": {
                "navigationType": "OPEN_NAVIGATION"
              },
              "title": "google",
              "url": "https://google.com"
            },
            {
              "faviconUrl": "data:image/123",
              "lockedNavigationOptions": {
                "navigationType": "BLOCK_NAVIGATION"
              },
              "title": "youtube",
              "url": "https://youtube.com"
            }
          ],
          "locked": true
        }
      }
    }
  },
  "teacher":
  {
          "email": "teacher@gmail.com",
          "fullName": "teacher",
          "gaiaId": "1",
          "photoUrl": "data:image/123"
        }
}
       )");
    response->set_content_type("application/json");
    return response;
  }

  static std::unique_ptr<HttpResponse> CreateDefaultResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(R"(
     {
    "sessionId": "111",
    "duration": {
        "seconds": "120"
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
    response->set_content_type("application/json");
    return response;
  }

  static std::unique_ptr<HttpResponse> CreateConsumerResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(
        R"(
  {
  "startTime":{
    "seconds": "1723773909",
    "nanos": 1234
  },
  "sessionId": "111",
  "duration": {
    "seconds": "120"
  },
  "sessionState": "ACTIVE",
  "studentGroupConfigs": {
    "encodedstring": {
      "captionsConfig": {
        "captionsEnabled": true,
        "translationsEnabled": true
      },
      "onTaskConfig": {
        "activeBundle": {
          "contentConfigs": [
            {
              "faviconUrl": "data:image/123",
              "lockedNavigationOptions": {
                "navigationType": "OPEN_NAVIGATION"
              },
              "title": "google",
              "url": "https://google.com"
            }
          ],
          "locked": true
        }
      }
    }
  },
  "teacher":
  {
          "email": "teacher@gmail.com",
          "fullName": "teacher",
          "gaiaId": "1",
          "photoUrl": "data:image/123"
        }
}
       )");
    response->set_content_type("application/json");
    return response;
  }

  static std::unique_ptr<HttpResponse> CreateEmptyResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content("{}");
    response->set_content_type("application/json");
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

class GetSessionRequestTest : public testing::Test {
 public:
  GetSessionRequestTest() = default;
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

TEST_F(GetSessionRequestTest, GetSessionWithFullProducerInputAndSucceed) {
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  AllOf(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET)),
                        Field(&HttpRequest::relative_url,
                              Eq("/v1/users/123/sessions:getActive")))))
      .WillOnce(
          Return(ByMove(MockRequestHandler::CreateFullProducerResponse())));

  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;

  const std::string gaia_id = "123";
  std::unique_ptr<GetSessionRequest> request =
      std::make_unique<GetSessionRequest>(request_sender(), gaia_id,
                                          future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());

  std::unique_ptr<::boca::Session> session = std::move(result.value());
  EXPECT_EQ(1723773909, session->start_time().seconds());
  EXPECT_EQ(1234, session->start_time().nanos());
  EXPECT_EQ("111", session->session_id());
  EXPECT_EQ(120, session->duration().seconds());

  ASSERT_EQ(2u, session->student_statuses().size());
  EXPECT_EQ(::boca::StudentStatus::ADDED,
            session->student_statuses().at("2").state());
  EXPECT_EQ(::boca::StudentStatus::ACTIVE,
            session->student_statuses().at("3").state());

  ASSERT_EQ(2, session->roster().student_groups()[0].students().size());
  EXPECT_EQ(kMainStudentGroupName,
            session->roster().student_groups()[0].title());

  EXPECT_EQ("cat@gmail.com",
            session->roster().student_groups()[0].students()[0].email());
  EXPECT_EQ("cat",
            session->roster().student_groups()[0].students()[0].full_name());
  EXPECT_EQ("2", session->roster().student_groups()[0].students()[0].gaia_id());
  EXPECT_EQ("data:image/123",
            session->roster().student_groups()[0].students()[0].photo_url());

  EXPECT_EQ("dog@gmail.com",
            session->roster().student_groups()[0].students()[1].email());
  EXPECT_EQ("dog",
            session->roster().student_groups()[0].students()[1].full_name());
  EXPECT_EQ("3", session->roster().student_groups()[0].students()[1].gaia_id());
  EXPECT_EQ("data:image/123",
            session->roster().student_groups()[0].students()[1].photo_url());

  EXPECT_EQ(::boca::Session::ACTIVE, session->session_state());

  ASSERT_EQ(1u, session->student_group_configs().size());
  EXPECT_TRUE(session->student_group_configs()
                  .at(kMainStudentGroupName)
                  .captions_config()
                  .captions_enabled());
  EXPECT_TRUE(session->student_group_configs()
                  .at(kMainStudentGroupName)
                  .captions_config()
                  .translations_enabled());

  EXPECT_TRUE(session->student_group_configs()
                  .at(kMainStudentGroupName)
                  .on_task_config()
                  .active_bundle()
                  .locked());

  auto content_config = std::move(session->student_group_configs()
                                      .at(kMainStudentGroupName)
                                      .on_task_config()
                                      .active_bundle()
                                      .content_configs());
  ASSERT_EQ(2, content_config.size());

  EXPECT_EQ("data:image/123", content_config[0].favicon_url());
  EXPECT_EQ("google", content_config[0].title());
  EXPECT_EQ("https://google.com", content_config[0].url());
  EXPECT_EQ(::boca::LockedNavigationOptions::OPEN_NAVIGATION,
            content_config[0].locked_navigation_options().navigation_type());

  EXPECT_EQ("data:image/123", content_config[1].favicon_url());
  EXPECT_EQ("youtube", content_config[1].title());
  EXPECT_EQ("https://youtube.com", content_config[1].url());
  EXPECT_EQ(::boca::LockedNavigationOptions::BLOCK_NAVIGATION,
            content_config[1].locked_navigation_options().navigation_type());

  EXPECT_EQ("teacher@gmail.com", session->teacher().email());
  EXPECT_EQ("teacher", session->teacher().full_name());
  EXPECT_EQ("1", session->teacher().gaia_id());
  EXPECT_EQ("data:image/123", session->teacher().photo_url());
}

TEST_F(GetSessionRequestTest, GetSessionWithFullConsumerInputAndSucceed) {
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  AllOf(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET)),
                        Field(&HttpRequest::relative_url,
                              Eq("/v1/users/123/sessions:getActive")))))
      .WillOnce(Return(ByMove(MockRequestHandler::CreateConsumerResponse())));

  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;

  const std::string gaia_id = "123";
  std::unique_ptr<GetSessionRequest> request =
      std::make_unique<GetSessionRequest>(request_sender(), gaia_id,
                                          future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());

  std::unique_ptr<::boca::Session> session = std::move(result.value());
  EXPECT_EQ(1723773909, session->start_time().seconds());
  EXPECT_EQ(1234, session->start_time().nanos());
  EXPECT_EQ("111", session->session_id());
  EXPECT_EQ(120, session->duration().seconds());

  EXPECT_EQ(::boca::Session::ACTIVE, session->session_state());

  ASSERT_EQ(1u, session->student_group_configs().size());
  EXPECT_TRUE(session->student_group_configs()
                  .at(kMainStudentGroupName)
                  .captions_config()
                  .captions_enabled());
  EXPECT_TRUE(session->student_group_configs()
                  .at(kMainStudentGroupName)
                  .captions_config()
                  .translations_enabled());

  EXPECT_TRUE(session->student_group_configs()
                  .at(kMainStudentGroupName)
                  .on_task_config()
                  .active_bundle()
                  .locked());

  auto content_config = std::move(session->student_group_configs()
                                      .at(kMainStudentGroupName)
                                      .on_task_config()
                                      .active_bundle()
                                      .content_configs());
  ASSERT_EQ(1, content_config.size());

  EXPECT_EQ("data:image/123", content_config[0].favicon_url());
  EXPECT_EQ("google", content_config[0].title());
  EXPECT_EQ("https://google.com", content_config[0].url());
  EXPECT_EQ(::boca::LockedNavigationOptions::OPEN_NAVIGATION,
            content_config[0].locked_navigation_options().navigation_type());

  EXPECT_EQ("teacher@gmail.com", session->teacher().email());
  EXPECT_EQ("teacher", session->teacher().full_name());
  EXPECT_EQ("1", session->teacher().gaia_id());
  EXPECT_EQ("data:image/123", session->teacher().photo_url());
}

TEST_F(GetSessionRequestTest, CreateSessionWithDefaultInputAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  AllOf(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET)),
                        Field(&HttpRequest::relative_url,
                              Eq("/v1/users/123/sessions:getActive")))))
      .WillOnce(Return(ByMove(MockRequestHandler::CreateDefaultResponse())));

  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;

  const std::string gaia_id = "123";
  std::unique_ptr<GetSessionRequest> request =
      std::make_unique<GetSessionRequest>(request_sender(), gaia_id,
                                          future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());

  std::unique_ptr<::boca::Session> session = std::move(result.value());
  EXPECT_EQ("111", session->session_id());
  EXPECT_EQ(120, session->duration().seconds());

  EXPECT_EQ(0u, session->student_statuses().size());

  ASSERT_EQ(0, session->roster().student_groups().size());

  EXPECT_EQ(2, session->session_state());

  ASSERT_EQ(1u, session->student_group_configs().size());

  auto content_config = std::move(session->student_group_configs()
                                      .at(kMainStudentGroupName)
                                      .on_task_config()
                                      .active_bundle()
                                      .content_configs());
  ASSERT_EQ(0, content_config.size());

  EXPECT_FALSE(session->student_group_configs()
                   .at(kMainStudentGroupName)
                   .captions_config()
                   .captions_enabled());
  EXPECT_FALSE(session->student_group_configs()
                   .at(kMainStudentGroupName)
                   .captions_config()
                   .translations_enabled());

  EXPECT_FALSE(session->student_group_configs()
                   .at(kMainStudentGroupName)
                   .on_task_config()
                   .active_bundle()
                   .locked());
}

TEST_F(GetSessionRequestTest, CreateSessionWithEmptyInputAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  AllOf(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET)),
                        Field(&HttpRequest::relative_url,
                              Eq("/v1/users/123/sessions:getActive")))))
      .WillOnce(Return(ByMove(MockRequestHandler::CreateEmptyResponse())));

  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;

  const std::string gaia_id = "123";
  std::unique_ptr<GetSessionRequest> request =
      std::make_unique<GetSessionRequest>(request_sender(), gaia_id,
                                          future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
}

TEST_F(GetSessionRequestTest, CreateSessionWithFailedResponse) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(),
              HandleRequest(
                  AllOf(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET)),
                        Field(&HttpRequest::relative_url,
                              Eq("/v1/users/123/sessions:getActive")))))
      .WillOnce(Return(ByMove(MockRequestHandler::CreateFailedResponse())));

  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future;

  const std::string gaia_id = "123";
  std::unique_ptr<GetSessionRequest> request =
      std::make_unique<GetSessionRequest>(request_sender(), gaia_id,
                                          future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), google_apis::HTTP_INTERNAL_SERVER_ERROR);
}
}  // namespace ash::boca
