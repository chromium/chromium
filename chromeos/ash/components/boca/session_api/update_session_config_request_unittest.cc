// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/update_session_config_request.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
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

class UpdateSessionConfigTest : public testing::Test {
 public:
  UpdateSessionConfigTest() = default;
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

TEST_F(UpdateSessionConfigTest,
       UpdateSessionConfigWithBothOnTaskAndCaptionConfigAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  const char session_id[] = "sessionId";
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  std::unique_ptr<UpdateSessionConfigRequest> request =
      std::make_unique<UpdateSessionConfigRequest>(
          request_sender(), "https://test", teacher, session_id,
          future.GetCallback());

  request->OverrideURLForTesting(test_server_.base_url().spec());

  ::boca::OnTaskConfig on_task_config;
  auto* active_bundle = on_task_config.mutable_active_bundle();
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

  ::boca::CaptionsConfig captions_config;
  captions_config.set_captions_enabled(true);
  captions_config.set_translations_enabled(true);
  request->set_captions_config(std::move(captions_config));
  request->set_group_ids({"1", "2"});

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_PATCH, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions/sessionId:updateConfig",
            http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  constexpr char kContentData[] =
      R"(
    {
      "sessionConfig": {
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
                  "navigationType": 1
                },
                "title": "google",
                "url": "https://google.com"
              },
              {
                "faviconUrl": "data:image/123",
                "lockedNavigationOptions": {
                  "navigationType": 2
                },
                "title": "youtube",
                "url": "https://youtube.com"
              }
            ],
            "lockToAppHome": false,
            "locked": true
          }
        }
      },
      "studentGroupIds": [
        "1",
        "2"
      ]
    }
  )";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(base::JSONReader::Read(kContentData).value(),
            base::JSONReader::Read(http_request.content));
  EXPECT_TRUE(result.value());
}

TEST_F(UpdateSessionConfigTest, UpdateSessionConfigWithOnTaskConfigAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  const char session_id[] = "sessionId";
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  std::unique_ptr<UpdateSessionConfigRequest> request =
      std::make_unique<UpdateSessionConfigRequest>(
          request_sender(), "https://test", teacher, session_id,
          future.GetCallback());

  request->OverrideURLForTesting(test_server_.base_url().spec());

  ::boca::OnTaskConfig on_task_config;
  auto* active_bundle = on_task_config.mutable_active_bundle();
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

  request->set_group_ids({"1", "2"});
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_PATCH, http_request.method);

  EXPECT_EQ(
      "/v1/teachers/1/sessions/sessionId:updateConfig?updateMask=onTaskConfig",
      http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  constexpr char kContentData[] = R"(
    {
      "sessionConfig": {
        "onTaskConfig": {
          "activeBundle": {
            "contentConfigs": [
              {
                "faviconUrl": "data:image/123",
                "lockedNavigationOptions": {
                  "navigationType": 1
                },
                "title": "google",
                "url": "https://google.com"
              },
              {
                "faviconUrl": "data:image/123",
                "lockedNavigationOptions": {
                  "navigationType": 2
                },
                "title": "youtube",
                "url": "https://youtube.com"
              }
            ],
            "lockToAppHome": false,
            "locked": true
          }
        }
      },
      "studentGroupIds": [
        "1",
        "2"
      ]
    }
  )";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(base::JSONReader::Read(kContentData).value(),
            base::JSONReader::Read(http_request.content));
  EXPECT_TRUE(result.value());
}

TEST_F(UpdateSessionConfigTest,
       UpdateSessionConfigWithCaptionConfigAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  const char session_id[] = "sessionId";
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  std::unique_ptr<UpdateSessionConfigRequest> request =
      std::make_unique<UpdateSessionConfigRequest>(
          request_sender(), "https://test", teacher, session_id,
          future.GetCallback());

  request->OverrideURLForTesting(test_server_.base_url().spec());

  ::boca::CaptionsConfig captions_config;
  captions_config.set_captions_enabled(true);
  captions_config.set_translations_enabled(true);
  request->set_captions_config(std::move(captions_config));
  request->set_group_ids({"1", "2"});

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_EQ(net::test_server::METHOD_PATCH, http_request.method);

  EXPECT_EQ(
      "/v1/teachers/1/sessions/"
      "sessionId:updateConfig?updateMask=captionsConfig",
      http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  constexpr char kContentData[] = R"(
    {
      "sessionConfig": {
        "captionsConfig": {
          "captionsEnabled": true,
          "translationsEnabled": true
        }
      },
      "studentGroupIds": [
        "1",
        "2"
      ]
    }
  )";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(base::JSONReader::Read(kContentData).value(),
            base::JSONReader::Read(http_request.content));
  EXPECT_TRUE(result.value());
}

TEST_F(UpdateSessionConfigTest, UpdateSessionConfigWithNoConfigFail) {
  net::test_server::HttpRequest http_request;

  ::boca::UserIdentity teacher;
  teacher.set_gaia_id("1");
  const char session_id[] = "sessionId";
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  std::unique_ptr<UpdateSessionConfigRequest> request =
      std::make_unique<UpdateSessionConfigRequest>(
          request_sender(), "https://test", teacher, session_id,
          future.GetCallback());
  request->OverrideURLForTesting(test_server_.base_url().spec());
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  EXPECT_TRUE(result.error());
}

}  // namespace ash::boca
