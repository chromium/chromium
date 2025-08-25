// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/add_students_request.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
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

class AddStudentsRequestTest : public testing::Test {
 public:
  AddStudentsRequestTest() = default;
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

TEST_F(AddStudentsRequestTest, AddMultipleStudentAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));

  std::string session_id = "session_id";
  GaiaId gaia_id("1");
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  std::unique_ptr<AddStudentsRequest> request =
      std::make_unique<AddStudentsRequest>(request_sender(), "https://test",
                                           gaia_id, session_id,
                                           future.GetCallback());

  request->OverrideURLForTesting(test_server_.base_url().spec());
  std::vector<::boca::UserIdentity> students;
  ::boca::UserIdentity student_1;
  student_1.set_gaia_id("2");
  student_1.set_email("cat@gmail.com");
  student_1.set_full_name("cat");
  student_1.set_photo_url("data:image/123");
  students.push_back(student_1);

  ::boca::UserIdentity student_2;
  student_2.set_gaia_id("3");
  student_2.set_email("dog@gmail.com");
  student_2.set_full_name("dog");
  student_2.set_photo_url("data:image/123");
  students.push_back(student_2);
  request->set_students(std::move(students));
  request->set_student_group_id("groupid");
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Get();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions/session_id/students:add",
            http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"studentGroups\":[{\"studentGroupId\":\"groupid\",\"students\":[{"
      "\"email\":\"cat@gmail.com\",\"fullName\":\"cat\",\"gaiaId\":\"2\","
      "\"photoUrl\":\"data:image/"
      "123\"},{\"email\":\"dog@gmail.com\",\"fullName\":\"dog\",\"gaiaId\":"
      "\"3\",\"photoUrl\":\"data:image/123\"}]}]}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_EQ(true, result.value());
}

TEST_F(AddStudentsRequestTest, AddEmptyStudentListAndSucceed) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateSuccessfulResponse())));
  GaiaId gaia_id("1");
  std::string session_id = "session_id";

  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  std::unique_ptr<AddStudentsRequest> request =
      std::make_unique<AddStudentsRequest>(request_sender(), "https://test",
                                           gaia_id, session_id,
                                           future.GetCallback());

  request->OverrideURLForTesting(test_server_.base_url().spec());

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Get();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions/session_id/students:add",
            http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"studentGroups\":[{\"studentGroupId\":\"\",\"students\":[]}]}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_EQ(true, result.value());
}

TEST_F(AddStudentsRequestTest, AddStudentsAndFail) {
  net::test_server::HttpRequest http_request;
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(DoAll(SaveArg<0>(&http_request),
                      Return(MockRequestHandler::CreateFailedResponse())));
  GaiaId gaia_id("1");
  std::string session_id = "session_id";

  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  std::unique_ptr<AddStudentsRequest> request =
      std::make_unique<AddStudentsRequest>(request_sender(), "https://test",
                                           gaia_id, session_id,
                                           future.GetCallback());

  request->OverrideURLForTesting(test_server_.base_url().spec());

  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  auto result = future.Get();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);

  EXPECT_EQ("/v1/teachers/1/sessions/session_id/students:add",
            http_request.relative_url);
  EXPECT_EQ("application/json", http_request.headers["Content-Type"]);
  auto* contentData =
      "{\"studentGroups\":[{\"studentGroupId\":\"\",\"students\":[]}]}";
  ASSERT_TRUE(http_request.has_content);
  EXPECT_EQ(contentData, http_request.content);
  EXPECT_EQ(google_apis::HTTP_INTERNAL_SERVER_ERROR, result.error());
}

}  // namespace ash::boca
