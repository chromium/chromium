// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/session_client_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/session_api/renotify_student_request.h"
#include "chromeos/ash/components/boca/session_api/update_student_activities_request.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
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
using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;
using ::testing::StrictMock;
using GetSessionCallback =
    base::OnceCallback<void(base::expected<std::unique_ptr<::boca::Session>,
                                           google_apis::ApiErrorCode> result)>;

namespace ash::boca {

namespace {

const char kTestUserAgent[] = "test-user-agent";

class MockRequestHandler {
 public:
  MOCK_METHOD(std::unique_ptr<HttpResponse>,
              HandleRequest,
              (const HttpRequest&));
};

}  // namespace

class SessionClientImplTest : public testing::Test {
 protected:
  SessionClientImplTest() = default;
  void SetUp() override {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
            /*network_service=*/nullptr,
            /*is_trusted=*/true);
    auto request_sender = std::make_unique<google_apis::RequestSender>(
        std::make_unique<google_apis::DummyAuthService>(),
        test_shared_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(), kTestUserAgent,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    request_sender_ = request_sender.get();
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&MockRequestHandler::HandleRequest,
                            base::Unretained(&request_handler_)));

    session_client_impl_ =
        std::make_unique<SessionClientImpl>(std::move(request_sender));
    ASSERT_TRUE(test_server_.Start());
  }

  std::unique_ptr<GetSessionRequest> CreateGetSessionRequest(
      GetSessionCallback callback) {
    auto request = std::make_unique<GetSessionRequest>(
        request_sender_, "https://test",
        /*is_producer=*/true, GaiaId("123"), std::move(callback));
    request->OverrideURLForTesting(test_server_.base_url().spec());
    return request;
  }

  std::unique_ptr<UpdateStudentActivitiesRequest> CreateInsertActivityRequest(
      SessionClientImpl::UpdateStudentActivitiesCallback callback) {
    auto request = std::make_unique<UpdateStudentActivitiesRequest>(
        request_sender_, "https://test", "1", GaiaId("2"), "device_id",
        std::move(callback));
    request->OverrideURLForTesting(test_server_.base_url().spec());
    return request;
  }

  net::EmbeddedTestServer test_server_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  testing::StrictMock<MockRequestHandler> request_handler_;
  std::unique_ptr<SessionClientImpl> session_client_impl_;
  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_loader_factory_;
  raw_ptr<google_apis::RequestSender> request_sender_;
};

TEST_F(SessionClientImplTest, ConcurrentGetSessionAllowDedupe) {
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future_1;
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future_2;
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future_3;

  EXPECT_CALL(request_handler_, HandleRequest(_)).Times(2);
  session_client_impl_->GetSession(
      CreateGetSessionRequest(future_1.GetCallback()),
      /*can_skip_duplicate_request=*/true);

  // Allow de-dup, but because there is no pending request, always enqueue.
  session_client_impl_->GetSession(
      CreateGetSessionRequest(future_2.GetCallback()),
      /*can_skip_duplicate_request=*/true);

  // Request being de-dup
  session_client_impl_->GetSession(
      CreateGetSessionRequest(future_3.GetCallback()),
      /*can_skip_duplicate_request=*/true);
  EXPECT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_2.Wait());
}

TEST_F(SessionClientImplTest, ConcurrentGetSessionDisallowDedupe) {
  EXPECT_CALL(request_handler_, HandleRequest(_)).Times(3);
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future_1;
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future_2;
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future_3;

  session_client_impl_->GetSession(
      CreateGetSessionRequest(future_1.GetCallback()),
      /*can_skip_duplicate_request=*/false);
  session_client_impl_->GetSession(
      CreateGetSessionRequest(future_2.GetCallback()),
      /*can_skip_duplicate_request=*/false);
  session_client_impl_->GetSession(
      CreateGetSessionRequest(future_3.GetCallback()),
      /*can_skip_duplicate_request=*/false);

  EXPECT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_2.Wait());
  EXPECT_TRUE(future_3.Wait());
}

TEST_F(SessionClientImplTest, SequentialGetSessionRequestRunInOrder) {
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future_1;
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future_2;
  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future_3;

  EXPECT_CALL(request_handler_, HandleRequest(_)).Times(1);
  session_client_impl_->GetSession(
      CreateGetSessionRequest(future_1.GetCallback()),
      /*can_skip_duplicate_request=*/false);
  EXPECT_TRUE(future_1.Wait());
  EXPECT_CALL(request_handler_, HandleRequest(_)).Times(1);
  session_client_impl_->GetSession(
      CreateGetSessionRequest(future_2.GetCallback()),
      /*can_skip_duplicate_request=*/false);
  EXPECT_TRUE(future_2.Wait());
  EXPECT_CALL(request_handler_, HandleRequest(_)).Times(1);
  session_client_impl_->GetSession(
      CreateGetSessionRequest(future_3.GetCallback()),
      /*can_skip_duplicate_request=*/false);
  EXPECT_TRUE(future_3.Wait());
}

TEST_F(SessionClientImplTest, RenotifyStudent) {
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future;

  std::unique_ptr<RenotifyStudentRequest> request =
      std::make_unique<RenotifyStudentRequest>(request_sender_, "https://test",
                                               GaiaId("gaia_id"), "session_id",
                                               future.GetCallback());

  request->OverrideURLForTesting(test_server_.base_url().spec());

  EXPECT_CALL(request_handler_, HandleRequest(_)).Times(1);
  session_client_impl_->RenotifyStudent(std::move(request));
  EXPECT_TRUE(future.Wait());
}

TEST_F(SessionClientImplTest, ConcurrentInsertActivityDisallowDedupe) {
  EXPECT_CALL(request_handler_, HandleRequest(_)).Times(2);
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future_1;
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future_2;

  session_client_impl_->UpdateStudentActivity(
      CreateInsertActivityRequest(future_1.GetCallback()));
  session_client_impl_->UpdateStudentActivity(
      CreateInsertActivityRequest(future_2.GetCallback()));

  EXPECT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_2.Wait());
}

TEST_F(SessionClientImplTest, SequentialInsertActivityRequestRunInOrder) {
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future_1;
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future_2;
  EXPECT_CALL(request_handler_, HandleRequest(_)).Times(1);
  session_client_impl_->UpdateStudentActivity(
      CreateInsertActivityRequest(future_1.GetCallback()));
  EXPECT_TRUE(future_1.Wait());
  EXPECT_CALL(request_handler_, HandleRequest(_)).Times(1);
  session_client_impl_->UpdateStudentActivity(
      CreateInsertActivityRequest(future_2.GetCallback()));
  EXPECT_TRUE(future_2.Wait());
}

TEST_F(SessionClientImplTest, GetSessionAfterInsertActivityShouldBeExecuted) {
  base::test::TestFuture<base::expected<bool, google_apis::ApiErrorCode>>
      future_1;

  EXPECT_CALL(request_handler_, HandleRequest(_)).Times(2);
  session_client_impl_->UpdateStudentActivity(
      CreateInsertActivityRequest(future_1.GetCallback()));
  EXPECT_TRUE(future_1.Wait());

  base::test::TestFuture<base::expected<std::unique_ptr<::boca::Session>,
                                        google_apis::ApiErrorCode>>
      future_2;

  session_client_impl_->GetSession(
      CreateGetSessionRequest(future_2.GetCallback()),
      /*can_skip_duplicate_request=*/false);
  EXPECT_TRUE(future_2.Wait());
}

}  // namespace ash::boca
