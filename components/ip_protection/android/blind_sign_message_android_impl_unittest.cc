// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/android/blind_sign_message_android_impl.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/ip_protection/android/android_auth_client_lib/cpp/ip_protection_auth_client_interface.h"
#include "components/ip_protection/android/blind_sign_message_android_impl.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/auth_and_sign.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/get_initial_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"

namespace ip_protection {

namespace {

using ::base::test::EqualsProto;
using ::testing::_;
using ::testing::SizeIs;
using ::testing::StrictMock;
using ClientCreated =
    ::ip_protection::android::IpProtectionAuthClientInterface::ClientCreated;
using MockClientFactory = StrictMock<
    ::testing::MockFunction<BlindSignMessageAndroidImpl::ClientFactory>>;

const char kClientCreationHistogram[] =
    "NetworkService.IpProtection.AndroidAuthClient.CreationTime";
const char kGetInitialDataHistogram[] =
    "NetworkService.IpProtection.AndroidAuthClient.GetInitialDataTime";
const char kAuthAndSignHistogram[] =
    "NetworkService.IpProtection.AndroidAuthClient.AuthAndSignTime";

// These example protos aren't realistic, but they are not equal to a
// default-initialized value. The details are arbitrary.
privacy::ppn::GetInitialDataRequest ExampleGetInitialDataRequest() {
  privacy::ppn::GetInitialDataRequest request;
  request.set_service_type("getInitialData_testing");
  return request;
}

std::string ExampleGetInitialDataRequestString() {
  std::string str;
  CHECK(ExampleGetInitialDataRequest().SerializeToString(&str));
  return str;
}

privacy::ppn::AuthAndSignRequest ExampleAuthAndSignRequest() {
  privacy::ppn::AuthAndSignRequest request;
  request.set_service_type("authAndSign_testing");
  return request;
}

std::string ExampleAuthAndSignRequestString() {
  std::string str;
  CHECK(ExampleAuthAndSignRequest().SerializeToString(&str));
  return str;
}

privacy::ppn::GetInitialDataResponse ExampleGetInitialDataResponse() {
  privacy::ppn::GetInitialDataResponse response;
  response.mutable_privacy_pass_data()->set_token_key_id("test");
  return response;
}

privacy::ppn::AuthAndSignResponse ExampleAuthAndSignResponse() {
  privacy::ppn::AuthAndSignResponse response;
  response.add_blinded_token_signature(
      "authAndSign_example_blinded_token_signature");
  return response;
}

class MockIpProtectionAuthClient
    : public ip_protection::android::IpProtectionAuthClientInterface {
 public:
  MOCK_METHOD(void,
              GetInitialData,
              (const privacy::ppn::GetInitialDataRequest& request,
               ip_protection::android::GetInitialDataResponseCallback callback),
              (const, override));

  MOCK_METHOD(void,
              AuthAndSign,
              (const privacy::ppn::AuthAndSignRequest& request,
               ip_protection::android::AuthAndSignResponseCallback callback),
              (const override));

  MOCK_METHOD(void,
              GetProxyConfig,
              (const GetProxyConfigRequest& request,
               ip_protection::android::GetProxyConfigResponseCallback callback),
              (const override));

  base::WeakPtr<IpProtectionAuthClientInterface> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockIpProtectionAuthClient> weak_ptr_factory_{this};
};

class BlindSignMessageAndroidImplTest : public testing::Test {
 protected:
  void SetUp() override {
    fetcher_ = std::make_unique<BlindSignMessageAndroidImpl>();
    fetcher_->SetIpProtectionAuthClientFactoryForTesting(base::BindRepeating(
        &MockClientFactory::Call, base::Unretained(&client_factory)));
  }

  MockClientFactory client_factory;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<BlindSignMessageAndroidImpl> fetcher_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(BlindSignMessageAndroidImplTest,
       DoRequestReturnsFailureStatusIfTokenProvidedAfterInitialized) {
  // An auth client is not expected to be created. If it is attempted, the
  // default failing client_factory will be triggered.

  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      result_future;
  auto callback =
      [&result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        result_future.SetValue(std::move(response));
      };

  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kGetInitialData,
                      "OAuth Token", ExampleGetInitialDataRequestString(),
                      std::move(callback));

  ASSERT_FALSE(result_future.Get().ok());
  EXPECT_EQ(result_future.Get().status().code(), absl::StatusCode::kInternal);
  histogram_tester_.ExpectTotalCount(kClientCreationHistogram, 0);
  histogram_tester_.ExpectTotalCount(kGetInitialDataHistogram, 0);
}

TEST_F(BlindSignMessageAndroidImplTest,
       DoRequestSendsCorrectRequestForGetInitialData) {
  EXPECT_CALL(client_factory, Call)
      .WillOnce([](base::OnceCallback<ClientCreated> callback) {
        auto mock_ip_protection_auth_client =
            std::make_unique<StrictMock<MockIpProtectionAuthClient>>();
        EXPECT_CALL(
            *mock_ip_protection_auth_client,
            GetInitialData(EqualsProto(ExampleGetInitialDataRequest()), _))
            .WillOnce([](const privacy::ppn::GetInitialDataRequest& request,
                         auto&& callback) {
              base::expected<privacy::ppn::GetInitialDataResponse,
                             ip_protection::android::AuthRequestError>
                  response(ExampleGetInitialDataResponse());
              std::move(callback).Run(std::move(response));
            });
        std::move(callback).Run(std::move(mock_ip_protection_auth_client));
      });
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      result_future;
  auto callback =
      [&result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        result_future.SetValue(std::move(response));
      };

  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kGetInitialData,
                      /*authorization_header=*/std::nullopt,
                      ExampleGetInitialDataRequestString(),
                      std::move(callback));

  ASSERT_TRUE(result_future.Get().ok());
  EXPECT_EQ(result_future.Get()->status_code(), absl::StatusCode::kOk);
  histogram_tester_.ExpectTotalCount(kClientCreationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kGetInitialDataHistogram, 1);
  histogram_tester_.ExpectTotalCount(kAuthAndSignHistogram, 0);
}

TEST_F(BlindSignMessageAndroidImplTest,
       DoRequestSendsCorrectRequestForAuthAndSign) {
  EXPECT_CALL(client_factory, Call)
      .WillOnce([](base::OnceCallback<ClientCreated> callback) {
        auto mock_ip_protection_auth_client =
            std::make_unique<StrictMock<MockIpProtectionAuthClient>>();
        EXPECT_CALL(*mock_ip_protection_auth_client,
                    AuthAndSign(EqualsProto(ExampleAuthAndSignRequest()), _))
            .WillOnce([](const privacy::ppn::AuthAndSignRequest& request,
                         auto&& callback) {
              base::expected<privacy::ppn::AuthAndSignResponse,
                             ip_protection::android::AuthRequestError>
                  response(ExampleAuthAndSignResponse());
              std::move(callback).Run(std::move(response));
            });
        std::move(callback).Run(std::move(mock_ip_protection_auth_client));
      });
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      result_future;
  auto callback =
      [&result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        result_future.SetValue(std::move(response));
      };

  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kAuthAndSign,
                      /*authorization_header=*/std::nullopt,
                      ExampleAuthAndSignRequestString(), std::move(callback));

  ASSERT_TRUE(result_future.Get().ok());
  EXPECT_EQ(result_future.Get()->status_code(), absl::StatusCode::kOk);
  histogram_tester_.ExpectTotalCount(kClientCreationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kGetInitialDataHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAuthAndSignHistogram, 1);
}

TEST_F(BlindSignMessageAndroidImplTest, DoRequestHandlesPersistentError) {
  EXPECT_CALL(client_factory, Call)
      .WillOnce([](base::OnceCallback<ClientCreated> callback) {
        auto mock_ip_protection_auth_client =
            std::make_unique<StrictMock<MockIpProtectionAuthClient>>();
        EXPECT_CALL(
            *mock_ip_protection_auth_client,
            GetInitialData(EqualsProto(ExampleGetInitialDataRequest()), _))
            .WillOnce([](const privacy::ppn::GetInitialDataRequest& request,
                         auto&& callback) {
              base::unexpected<ip_protection::android::AuthRequestError>
                  persistent_error(
                      ip_protection::android::AuthRequestError::kPersistent);
              base::expected<privacy::ppn::GetInitialDataResponse,
                             ip_protection::android::AuthRequestError>
                  response(std::move(persistent_error));
              std::move(callback).Run(std::move(response));
            });
        EXPECT_CALL(*mock_ip_protection_auth_client,
                    AuthAndSign(EqualsProto(ExampleAuthAndSignRequest()), _))
            .WillOnce([](const privacy::ppn::AuthAndSignRequest& request,
                         auto&& callback) {
              base::unexpected<ip_protection::android::AuthRequestError>
                  persistent_error(
                      ip_protection::android::AuthRequestError::kPersistent);
              base::expected<privacy::ppn::AuthAndSignResponse,
                             ip_protection::android::AuthRequestError>
                  response(std::move(persistent_error));
              std::move(callback).Run(std::move(response));
            });
        std::move(callback).Run(std::move(mock_ip_protection_auth_client));
      });
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      get_initial_data_result_future;
  auto get_initial_data_callback =
      [&get_initial_data_result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        get_initial_data_result_future.SetValue(std::move(response));
      };
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      auth_and_sign_result_future;
  auto auth_and_sign_callback =
      [&auth_and_sign_result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        auth_and_sign_result_future.SetValue(std::move(response));
      };

  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kGetInitialData,
                      /*authorization_header=*/std::nullopt,
                      ExampleGetInitialDataRequestString(),
                      std::move(get_initial_data_callback));
  auto get_initial_data_result = get_initial_data_result_future.Get();
  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kAuthAndSign,
                      /*authorization_header=*/std::nullopt,
                      ExampleAuthAndSignRequestString(),
                      std::move(auth_and_sign_callback));
  auto auth_and_sign_result = auth_and_sign_result_future.Get();

  ASSERT_FALSE(get_initial_data_result.ok());
  EXPECT_EQ(get_initial_data_result_future.Get().status().code(),
            absl::StatusCode::kFailedPrecondition);
  ASSERT_FALSE(auth_and_sign_result.ok());
  EXPECT_EQ(auth_and_sign_result.status().code(),
            absl::StatusCode::kFailedPrecondition);
  histogram_tester_.ExpectTotalCount(kClientCreationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kGetInitialDataHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAuthAndSignHistogram, 0);
}

TEST_F(BlindSignMessageAndroidImplTest, DoRequestHandlesTransientError) {
  EXPECT_CALL(client_factory, Call)
      .WillOnce([](base::OnceCallback<ClientCreated> callback) {
        auto mock_ip_protection_auth_client =
            std::make_unique<StrictMock<MockIpProtectionAuthClient>>();
        EXPECT_CALL(
            *mock_ip_protection_auth_client,
            GetInitialData(EqualsProto(ExampleGetInitialDataRequest()), _))
            .WillOnce([](const privacy::ppn::GetInitialDataRequest& request,
                         auto&& callback) {
              base::unexpected<ip_protection::android::AuthRequestError>
                  transient_error(
                      ip_protection::android::AuthRequestError::kTransient);
              base::expected<privacy::ppn::GetInitialDataResponse,
                             ip_protection::android::AuthRequestError>
                  response(std::move(transient_error));
              std::move(callback).Run(std::move(response));
            });
        EXPECT_CALL(*mock_ip_protection_auth_client,
                    AuthAndSign(EqualsProto(ExampleAuthAndSignRequest()), _))
            .WillOnce([](const privacy::ppn::AuthAndSignRequest& request,
                         auto&& callback) {
              base::unexpected<ip_protection::android::AuthRequestError>
                  transient_error(
                      ip_protection::android::AuthRequestError::kTransient);
              base::expected<privacy::ppn::AuthAndSignResponse,
                             ip_protection::android::AuthRequestError>
                  response(std::move(transient_error));
              std::move(callback).Run(std::move(response));
            });
        std::move(callback).Run(std::move(mock_ip_protection_auth_client));
      });
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      get_initial_data_result_future;
  auto get_initial_data_callback =
      [&get_initial_data_result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        get_initial_data_result_future.SetValue(std::move(response));
      };
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      auth_and_sign_result_future;
  auto auth_and_sign_callback =
      [&auth_and_sign_result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        auth_and_sign_result_future.SetValue(std::move(response));
      };

  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kGetInitialData,
                      /*authorization_header=*/std::nullopt,
                      ExampleGetInitialDataRequestString(),
                      std::move(get_initial_data_callback));
  auto get_initial_data_result = get_initial_data_result_future.Get();
  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kAuthAndSign,
                      /*authorization_header=*/std::nullopt,
                      ExampleAuthAndSignRequestString(),
                      std::move(auth_and_sign_callback));
  auto auth_and_sign_result = auth_and_sign_result_future.Get();

  ASSERT_FALSE(get_initial_data_result.ok());
  EXPECT_EQ(get_initial_data_result.status().code(),
            absl::StatusCode::kUnavailable);
  ASSERT_FALSE(auth_and_sign_result.ok());
  EXPECT_EQ(auth_and_sign_result.status().code(),
            absl::StatusCode::kUnavailable);
  histogram_tester_.ExpectTotalCount(kClientCreationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kGetInitialDataHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAuthAndSignHistogram, 0);
}

TEST_F(BlindSignMessageAndroidImplTest, DoRequestHandlesOtherErrors) {
  EXPECT_CALL(client_factory, Call)
      .WillOnce([](base::OnceCallback<ClientCreated> callback) {
        auto mock_ip_protection_auth_client =
            std::make_unique<StrictMock<MockIpProtectionAuthClient>>();
        EXPECT_CALL(
            *mock_ip_protection_auth_client,
            GetInitialData(EqualsProto(ExampleGetInitialDataRequest()), _))
            .WillOnce([](const privacy::ppn::GetInitialDataRequest& request,
                         auto&& callback) {
              base::unexpected<ip_protection::android::AuthRequestError>
                  other_error(ip_protection::android::AuthRequestError::kOther);
              base::expected<privacy::ppn::GetInitialDataResponse,
                             ip_protection::android::AuthRequestError>
                  response(std::move(other_error));
              std::move(callback).Run(std::move(response));
            });
        std::move(callback).Run(std::move(mock_ip_protection_auth_client));
      });
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      get_initial_data_result_future;
  auto get_initial_data_callback =
      [&get_initial_data_result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        get_initial_data_result_future.SetValue(std::move(response));
      };

  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kGetInitialData,
                      /*authorization_header=*/std::nullopt,
                      ExampleGetInitialDataRequestString(),
                      std::move(get_initial_data_callback));

  ASSERT_FALSE(get_initial_data_result_future.Get().ok());
  EXPECT_EQ(get_initial_data_result_future.Get().status().code(),
            absl::StatusCode::kInternal);

  // The auth client should have been torn down due to the error.
  EXPECT_EQ(fetcher_->GetIpProtectionAuthClientForTesting(), nullptr);
  // Create the next one with new expected mock calls.
  EXPECT_CALL(client_factory, Call)
      .WillOnce([](base::OnceCallback<ClientCreated> callback) {
        auto mock_ip_protection_auth_client =
            std::make_unique<StrictMock<MockIpProtectionAuthClient>>();
        EXPECT_CALL(*mock_ip_protection_auth_client,
                    AuthAndSign(EqualsProto(ExampleAuthAndSignRequest()), _))
            .WillOnce([](const privacy::ppn::AuthAndSignRequest& request,
                         auto&& callback) {
              base::unexpected<ip_protection::android::AuthRequestError>
                  other_error(ip_protection::android::AuthRequestError::kOther);
              base::expected<privacy::ppn::AuthAndSignResponse,
                             ip_protection::android::AuthRequestError>
                  response(std::move(other_error));
              std::move(callback).Run(std::move(response));
            });
        std::move(callback).Run(std::move(mock_ip_protection_auth_client));
      });
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      auth_and_sign_result_future;
  auto auth_and_sign_callback =
      [&auth_and_sign_result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        auth_and_sign_result_future.SetValue(std::move(response));
      };

  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kAuthAndSign,
                      /*authorization_header=*/std::nullopt,
                      ExampleAuthAndSignRequestString(),
                      std::move(auth_and_sign_callback));

  ASSERT_FALSE(auth_and_sign_result_future.Get().ok());
  EXPECT_EQ(auth_and_sign_result_future.Get().status().code(),
            absl::StatusCode::kInternal);
  // The auth client should have been torn down again.
  EXPECT_EQ(fetcher_->GetIpProtectionAuthClientForTesting(), nullptr);
  histogram_tester_.ExpectTotalCount(kClientCreationHistogram, 2);
  histogram_tester_.ExpectTotalCount(kGetInitialDataHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAuthAndSignHistogram, 0);
}

TEST_F(BlindSignMessageAndroidImplTest,
       RequestsAreQueuedUntilConnectedInstance) {
  std::optional<base::OnceCallback<ClientCreated>> create_callback;
  EXPECT_CALL(client_factory, Call)
      .WillOnce([&create_callback](base::OnceCallback<ClientCreated> callback) {
        ASSERT_FALSE(create_callback.has_value());
        create_callback.emplace(std::move(callback));
      });
  int remaining_tasks = 2;
  base::RunLoop creation_run_loop;
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      get_initial_data_result_future;
  auto get_initial_data_callback =
      [&get_initial_data_result_future, &creation_run_loop, &remaining_tasks](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        get_initial_data_result_future.SetValue(std::move(response));
        if (--remaining_tasks == 0) {
          creation_run_loop.Quit();
        }
      };
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      auth_and_sign_result_future;
  auto auth_and_sign_callback =
      [&auth_and_sign_result_future, &creation_run_loop, &remaining_tasks](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        auth_and_sign_result_future.SetValue(std::move(response));
        if (--remaining_tasks == 0) {
          creation_run_loop.Quit();
        }
      };

  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kGetInitialData,
                      /*authorization_header=*/std::nullopt,
                      ExampleGetInitialDataRequestString(),
                      std::move(get_initial_data_callback));
  EXPECT_THAT(fetcher_->GetPendingRequestsForTesting(), SizeIs(1));
  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kAuthAndSign,
                      /*authorization_header=*/std::nullopt,
                      ExampleAuthAndSignRequestString(),
                      std::move(auth_and_sign_callback));
  EXPECT_THAT(fetcher_->GetPendingRequestsForTesting(), SizeIs(2));
  histogram_tester_.ExpectTotalCount(kClientCreationHistogram, 0);
  // The creation callback should be waiting to be run.
  // Set up the mock and complete the creation, which will release the queue.
  ASSERT_TRUE(create_callback.has_value());
  std::unique_ptr<MockIpProtectionAuthClient> mock_ip_protection_auth_client =
      std::make_unique<StrictMock<MockIpProtectionAuthClient>>();
  EXPECT_CALL(*mock_ip_protection_auth_client,
              GetInitialData(EqualsProto(ExampleGetInitialDataRequest()), _))
      .WillOnce([](const privacy::ppn::GetInitialDataRequest& request,
                   auto&& callback) {
        base::expected<privacy::ppn::GetInitialDataResponse,
                       ip_protection::android::AuthRequestError>
            response(ExampleGetInitialDataResponse());
        std::move(callback).Run(std::move(response));
      });
  EXPECT_CALL(*mock_ip_protection_auth_client,
              AuthAndSign(EqualsProto(ExampleAuthAndSignRequest()), _))
      .WillOnce(
          [](const privacy::ppn::AuthAndSignRequest& request, auto&& callback) {
            base::expected<privacy::ppn::AuthAndSignResponse,
                           ip_protection::android::AuthRequestError>
                response(ExampleAuthAndSignResponse());
            std::move(callback).Run(std::move(response));
          });
  std::move(*create_callback).Run(std::move(mock_ip_protection_auth_client));
  creation_run_loop.Run();

  EXPECT_THAT(fetcher_->GetPendingRequestsForTesting(), SizeIs(0));
  ASSERT_TRUE(get_initial_data_result_future.Get().ok());
  ASSERT_TRUE(auth_and_sign_result_future.Get().ok());
  EXPECT_EQ(get_initial_data_result_future.Get()->status_code(),
            absl::StatusCode::kOk);
  EXPECT_EQ(auth_and_sign_result_future.Get()->status_code(),
            absl::StatusCode::kOk);
  histogram_tester_.ExpectTotalCount(kClientCreationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kGetInitialDataHistogram, 1);
  histogram_tester_.ExpectTotalCount(kAuthAndSignHistogram, 1);
}

TEST_F(BlindSignMessageAndroidImplTest,
       DoRequestReturnsInternalErrorIfFailureToBindToService) {
  std::optional<base::OnceCallback<ClientCreated>> create_callback;
  EXPECT_CALL(client_factory, Call)
      .WillOnce([&create_callback](base::OnceCallback<ClientCreated> callback) {
        ASSERT_FALSE(create_callback.has_value());
        create_callback.emplace(std::move(callback));
      });
  int remaining_tasks = 2;
  base::RunLoop creation_run_loop;
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      get_initial_data_result_future;
  auto get_initial_data_callback =
      [&get_initial_data_result_future, &creation_run_loop, &remaining_tasks](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        get_initial_data_result_future.SetValue(std::move(response));
        if (--remaining_tasks == 0) {
          creation_run_loop.Quit();
        }
      };
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      auth_and_sign_result_future;
  auto auth_and_sign_callback =
      [&auth_and_sign_result_future, &creation_run_loop, &remaining_tasks](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        auth_and_sign_result_future.SetValue(std::move(response));
        if (--remaining_tasks == 0) {
          creation_run_loop.Quit();
        }
      };

  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kGetInitialData,
                      /*authorization_header=*/std::nullopt,
                      ExampleGetInitialDataRequestString(),
                      std::move(get_initial_data_callback));
  EXPECT_THAT(fetcher_->GetPendingRequestsForTesting(), SizeIs(1));
  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kAuthAndSign,
                      /*authorization_header=*/std::nullopt,
                      ExampleAuthAndSignRequestString(),
                      std::move(auth_and_sign_callback));
  EXPECT_THAT(fetcher_->GetPendingRequestsForTesting(), SizeIs(2));
  // The creation callback should be waiting to be run.
  // Finish create connected instance request and assert an internal error is
  // returned for pending requests when failing to create a connected instance
  // to the service.
  ASSERT_TRUE(create_callback.has_value());
  std::move(*create_callback)
      .Run(base::unexpected("Auth client creation failed"));
  creation_run_loop.Run();

  EXPECT_THAT(fetcher_->GetPendingRequestsForTesting(), SizeIs(0));
  ASSERT_FALSE(get_initial_data_result_future.Get().ok());
  ASSERT_FALSE(auth_and_sign_result_future.Get().ok());
  EXPECT_EQ(get_initial_data_result_future.Get().status().code(),
            absl::StatusCode::kInternal);
  EXPECT_EQ(auth_and_sign_result_future.Get().status().code(),
            absl::StatusCode::kInternal);
  histogram_tester_.ExpectTotalCount(kClientCreationHistogram, 0);
  histogram_tester_.ExpectTotalCount(kGetInitialDataHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAuthAndSignHistogram, 0);
}

TEST_F(BlindSignMessageAndroidImplTest,
       RetryCreateConnectedInstanceOnNextRequestIfServiceDisconnected) {
  EXPECT_CALL(client_factory, Call)
      .WillOnce([](base::OnceCallback<ClientCreated> callback) {
        auto mock_ip_protection_auth_client =
            std::make_unique<StrictMock<MockIpProtectionAuthClient>>();
        EXPECT_CALL(
            *mock_ip_protection_auth_client,
            GetInitialData(EqualsProto(ExampleGetInitialDataRequest()), _))
            .WillOnce([](const privacy::ppn::GetInitialDataRequest& request,
                         auto&& callback) {
              base::unexpected<ip_protection::android::AuthRequestError>
                  other_error(ip_protection::android::AuthRequestError::kOther);
              base::expected<privacy::ppn::GetInitialDataResponse,
                             ip_protection::android::AuthRequestError>
                  response(std::move(other_error));
              std::move(callback).Run(std::move(response));
            });
        std::move(callback).Run(std::move(mock_ip_protection_auth_client));
      });
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      get_initial_data_result_future;
  auto get_initial_data_callback =
      [&get_initial_data_result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        get_initial_data_result_future.SetValue(std::move(response));
      };

  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kGetInitialData,
                      /*authorization_header=*/std::nullopt,
                      ExampleGetInitialDataRequestString(),
                      std::move(get_initial_data_callback));

  ASSERT_FALSE(get_initial_data_result_future.Get().ok());
  EXPECT_EQ(get_initial_data_result_future.Get().status().code(),
            absl::StatusCode::kInternal);
  EXPECT_EQ(fetcher_->GetIpProtectionAuthClientForTesting(), nullptr);
  histogram_tester_.ExpectTotalCount(kClientCreationHistogram, 1);

  // The auth client should have been torn down due to the error.
  EXPECT_EQ(fetcher_->GetIpProtectionAuthClientForTesting(), nullptr);
  // Create a new one with new expected mock calls when the request comes in.
  EXPECT_CALL(client_factory, Call)
      .WillOnce([](base::OnceCallback<ClientCreated> callback) {
        auto mock_ip_protection_auth_client =
            std::make_unique<StrictMock<MockIpProtectionAuthClient>>();
        EXPECT_CALL(
            *mock_ip_protection_auth_client,
            GetInitialData(EqualsProto(ExampleGetInitialDataRequest()), _))
            .WillOnce([](const privacy::ppn::GetInitialDataRequest& request,
                         auto&& callback) {
              base::expected<privacy::ppn::GetInitialDataResponse,
                             ip_protection::android::AuthRequestError>
                  response(ExampleGetInitialDataResponse());
              std::move(callback).Run(std::move(response));
            });
        std::move(callback).Run(std::move(mock_ip_protection_auth_client));
      });
  base::test::TestFuture<absl::StatusOr<quiche::BlindSignMessageResponse>>
      get_initial_data_2_result_future;
  auto get_initial_data_2_callback =
      [&get_initial_data_2_result_future](
          absl::StatusOr<quiche::BlindSignMessageResponse> response) {
        get_initial_data_2_result_future.SetValue(std::move(response));
      };

  fetcher_->DoRequest(quiche::BlindSignMessageRequestType::kGetInitialData,
                      /*authorization_header=*/std::nullopt,
                      ExampleGetInitialDataRequestString(),
                      std::move(get_initial_data_2_callback));

  ASSERT_TRUE(get_initial_data_2_result_future.Get().ok());
  EXPECT_EQ(get_initial_data_2_result_future.Get()->status_code(),
            absl::StatusCode::kOk);
  EXPECT_NE(fetcher_->GetIpProtectionAuthClientForTesting(), nullptr);
  histogram_tester_.ExpectTotalCount(kClientCreationHistogram, 2);
  histogram_tester_.ExpectTotalCount(kGetInitialDataHistogram, 1);
  histogram_tester_.ExpectTotalCount(kAuthAndSignHistogram, 0);
}

}  // namespace

}  // namespace ip_protection
