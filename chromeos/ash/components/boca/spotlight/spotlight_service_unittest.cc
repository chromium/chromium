// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_service.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_id.h"
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
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace ash::boca {
namespace {
constexpr GaiaId::Literal kGaiaId("123");
constexpr char kUserEmail[] = "cat@gmail.com";
constexpr char kDeviceId[] = "device0";
constexpr char kStudentDeviceId[] = "device1";
constexpr char kStudentId[] = "student";
constexpr char kConnectionCode[] = "456";

::boca::Session GetCommonTestSession() {
  ::boca::Session session;
  session.set_session_state(::boca::Session::ACTIVE);
  session.set_session_id("session_id");
  auto* teacher = session.mutable_teacher();
  teacher->set_gaia_id(kGaiaId.ToString());
  ::boca::StudentStatus status;
  (*status.mutable_devices())[kStudentDeviceId] = ::boca::StudentDevice();
  (*session.mutable_student_statuses())[kStudentId] = std::move(status);
  return session;
}

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

class MockBocaAppClient : public BocaAppClient {
 public:
  MOCK_METHOD(BocaSessionManager*, GetSessionManager, (), (override));
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetURLLoaderFactory,
              (),
              (override));
  MOCK_METHOD(std::string, GetDeviceId, (), (override));
};

class MockSessionManager : public BocaSessionManager {
 public:
  explicit MockSessionManager(SessionClientImpl* session_client_impl)
      : BocaSessionManager(
            session_client_impl,
            /*pref_service=*/nullptr,
            AccountId::FromUserEmailGaiaId(kUserEmail, GaiaId(kGaiaId)),
            /*=is_producer*/ false) {}
  MOCK_METHOD((::boca::Session*), GetCurrentSession, (), (override));
  MOCK_METHOD((std::string), GetDeviceRobotEmail, (), (override));

  ~MockSessionManager() override = default;
};

class SpotlightServiceTest : public testing::Test {
 public:
  SpotlightServiceTest() = default;
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca},
        /*disabled_features=*/{ash::features::kBocaSpotlightRobotRequester});
    ON_CALL(boca_app_client_, GetIdentityManager())
        .WillByDefault(Return(identity_test_env_.identity_manager()));

    ON_CALL(boca_app_client_, GetDeviceId()).WillByDefault(Return(kDeviceId));
    boca_session_manager_ =
        std::make_unique<StrictMock<MockSessionManager>>(nullptr);
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&MockRequestHandler::HandleRequest,
                            base::Unretained(&request_handler_)));

    ON_CALL(boca_app_client_, GetSessionManager())
        .WillByDefault(Return(boca_session_manager_.get()));

    ASSERT_TRUE(test_server_.Start());
  }

 protected:
  std::unique_ptr<google_apis::RequestSender> MakeRequestSender() {
    return std::make_unique<google_apis::RequestSender>(
        std::make_unique<google_apis::DummyAuthService>(),
        test_shared_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(), "test-user-agent",
        TRAFFIC_ANNOTATION_FOR_TESTS);
  }
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::IO};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_loader_factory_ =
          base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
              /*network_service=*/nullptr,
              /*is_trusted=*/true);
  NiceMock<MockBocaAppClient> boca_app_client_;
  signin::IdentityTestEnvironment identity_test_env_;
  net::EmbeddedTestServer test_server_;
  testing::StrictMock<MockRequestHandler> request_handler_;
  std::unique_ptr<StrictMock<MockSessionManager>> boca_session_manager_;
  SpotlightService spotlight_service_{MakeRequestSender()};
};

TEST_F(SpotlightServiceTest, TestViewScreenSucceed) {
  auto session = GetCommonTestSession();
  EXPECT_CALL(*boca_session_manager_, GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler_, HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  spotlight_service_.ViewScreen(kStudentId, test_server_.base_url().spec(),
                                future.GetCallback());
  auto result = future.Get();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/sessions/session_id/viewScreen:initiate",
            http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"hostDevice\":{\"deviceInfo\":{\"deviceId\":\"device1\"},\"user\":{"
      "\"gaiaId\":\"student\"}},\"teacherClientDevice\":{\"deviceInfo\":{"
      "\"deviceId\":\"device0\"},\"user\":{\"gaiaId\":\"123\"}}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_TRUE(result.value());
}

TEST_F(SpotlightServiceTest, TestViewScreenSucceedWithRobotEmail) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{ash::features::kBoca,
                            ash::features::kBocaSpotlightRobotRequester},
      /*disabled_features=*/{});
  auto session = GetCommonTestSession();
  EXPECT_CALL(*boca_session_manager_, GetCurrentSession())
      .WillOnce(Return(&session));
  EXPECT_CALL(*boca_session_manager_, GetDeviceRobotEmail())
      .WillOnce(Return("robot@email.com"));
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler_, HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  spotlight_service_.ViewScreen(kStudentId, test_server_.base_url().spec(),
                                future.GetCallback());
  auto result = future.Get();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/sessions/session_id/viewScreen:initiate",
            http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"hostDevice\":{\"deviceInfo\":{\"deviceId\":\"device1\"},\"user\":{"
      "\"gaiaId\":\"student\"}},\"teacherClientDevice\":{\"deviceInfo\":{"
      "\"deviceId\":\"device0\"},\"serviceAccount\":{"
      "\"email\":\"robot@email.com\"},\"user\":{\"gaiaId\":\"123\"}}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_TRUE(result.value());
}

TEST_F(SpotlightServiceTest, TestViewScreenWithEmptySession) {
  EXPECT_CALL(*boca_session_manager_, GetCurrentSession())
      .WillOnce(Return(nullptr));
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  spotlight_service_.ViewScreen(kStudentId, test_server_.base_url().spec(),
                                future.GetCallback());
  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(google_apis::ApiErrorCode::CANCELLED, result.error());
}

TEST_F(SpotlightServiceTest, TestViewScreenWithInvalidStudent) {
  auto session = GetCommonTestSession();
  EXPECT_CALL(*boca_session_manager_, GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  spotlight_service_.ViewScreen(
      "differentStudent", test_server_.base_url().spec(), future.GetCallback());
  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(google_apis::ApiErrorCode::CANCELLED, result.error());
}

TEST_F(SpotlightServiceTest, TestViewScreenWithEmptyDeviceList) {
  auto session = GetCommonTestSession();
  (*session.mutable_student_statuses())[kStudentId] = ::boca::StudentStatus();
  EXPECT_CALL(*boca_session_manager_, GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;
  spotlight_service_.ViewScreen(kStudentId, test_server_.base_url().spec(),
                                future.GetCallback());
  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(google_apis::ApiErrorCode::CANCELLED, result.error());
}

TEST_F(SpotlightServiceTest, TestRegisterScreenSucceed) {
  auto session = GetCommonTestSession();
  EXPECT_CALL(*boca_session_manager_, GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler_, HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));

  spotlight_service_.RegisterScreen(
      kConnectionCode, test_server_.base_url().spec(), future.GetCallback());
  auto result = future.Get();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/sessions/session_id/viewScreen:register",
            http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"connectionParam\":{\"connectionCode\":\"456\"},\"hostDevice\":{"
      "\"deviceInfo\":{"
      "\"deviceId\":\"device0\"},\"user\":{\"gaiaId\":\"123\"}}}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_TRUE(result.value());
}

TEST_F(SpotlightServiceTest, TestRegisterScreenWithEmptySession) {
  EXPECT_CALL(*boca_session_manager_, GetCurrentSession())
      .WillOnce(Return(nullptr));
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  spotlight_service_.RegisterScreen(
      kConnectionCode, test_server_.base_url().spec(), future.GetCallback());
  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(google_apis::ApiErrorCode::CANCELLED, result.error());
}

TEST_F(SpotlightServiceTest, TestUpdateViewScreenStateSucceed) {
  auto session = GetCommonTestSession();
  EXPECT_CALL(*boca_session_manager_, GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler_, HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  spotlight_service_.UpdateViewScreenState(
      kStudentId, ::boca::ViewScreenConfig::INACTIVE,
      test_server_.base_url().spec(), future.GetCallback());
  auto result = future.Get();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/sessions/session_id/viewScreen:updateState",
            http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"hostDevice\":{\"deviceInfo\":{\"deviceId\":\"device1\"},\"user\":{"
      "\"gaiaId\":\"student\"}},\"teacherClientDevice\":{\"deviceInfo\":{"
      "\"deviceId\":\"device0\"},\"user\":{\"gaiaId\":\"123\"}},"
      "\"viewScreenState\":4}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_TRUE(result.value());
}

TEST_F(SpotlightServiceTest, TestUpdateViewScreenStateWithEmptySession) {
  EXPECT_CALL(*boca_session_manager_, GetCurrentSession())
      .WillOnce(Return(nullptr));
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  spotlight_service_.UpdateViewScreenState(
      kStudentId, ::boca::ViewScreenConfig::INACTIVE,
      test_server_.base_url().spec(), future.GetCallback());
  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(google_apis::ApiErrorCode::CANCELLED, result.error());
}

TEST_F(SpotlightServiceTest, TestUpdateViewScreenStateWithInvalidStudent) {
  auto session = GetCommonTestSession();
  EXPECT_CALL(*boca_session_manager_, GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  spotlight_service_.UpdateViewScreenState(
      "differentStudent", ::boca::ViewScreenConfig::INACTIVE,
      test_server_.base_url().spec(), future.GetCallback());
  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(google_apis::ApiErrorCode::CANCELLED, result.error());
}

TEST_F(SpotlightServiceTest, TestUpdateViewScreenStateWithEmptyDeviceList) {
  auto session = GetCommonTestSession();
  (*session.mutable_student_statuses())[kStudentId] = ::boca::StudentStatus();
  EXPECT_CALL(*boca_session_manager_, GetCurrentSession())
      .WillOnce(Return(&session));
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;
  spotlight_service_.UpdateViewScreenState(
      kStudentId, ::boca::ViewScreenConfig::INACTIVE,
      test_server_.base_url().spec(), future.GetCallback());
  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(google_apis::ApiErrorCode::CANCELLED, result.error());
}
}  // namespace

}  // namespace ash::boca
