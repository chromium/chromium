// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/scanner_provider.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/manta/base_provider.h"
#include "components/manta/base_provider_test_helper.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/rpc_status.pb.h"
#include "components/manta/proto/scanner.pb.h"
#include "components/manta/provider_params.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace manta {

namespace {
constexpr char kMockEndpoint[] = "https://my-endpoint.com";
}

class FakeScannerProvider : public ScannerProvider, public FakeBaseProvider {
 public:
  FakeScannerProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : BaseProvider(test_url_loader_factory, identity_manager),
        ScannerProvider(test_url_loader_factory,
                        identity_manager,
                        ProviderParams()),
        FakeBaseProvider(test_url_loader_factory, identity_manager) {}
};

class ScannerProviderTest : public BaseProviderTest {
 public:
  ScannerProviderTest() = default;

  ScannerProviderTest(const ScannerProviderTest&) = delete;
  ScannerProviderTest& operator=(const ScannerProviderTest&) = delete;

  ~ScannerProviderTest() override = default;

  std::unique_ptr<FakeScannerProvider> CreateScannerProvider() {
    return std::make_unique<FakeScannerProvider>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_->identity_manager());
  }
};

TEST_F(ScannerProviderTest, ParseSuccessfulResponse) {
  proto::Response response;
  proto::OutputData& output_data = *response.add_output_data();
  output_data.set_text("foo");

  base::HistogramTester histogram_tester;

  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/response.SerializeAsString(),
                          net::HTTP_OK, net::OK);

  scanner_provider->Call(base::BindLambdaForTesting(
      [quit_closure = task_environment_.QuitClosure()](
          std::unique_ptr<proto::ScannerResponse> scanner_response,
          MantaStatus manta_status) {
        EXPECT_EQ(manta_status.status_code, MantaStatusCode::kOk);
        // TODO: b/363101024 - Check scanner_response has been parsed.
        quit_closure.Run();
      }));
  task_environment_.RunUntilQuit();
  histogram_tester.ExpectTotalCount("Ash.MantaService.ScannerProvider.TimeCost",
                                    1);
}

// Test that responses with http_status_code != net::HTTP_OK are captured.
TEST_F(ScannerProviderTest, CaptureUnexpectedStatusCode) {
  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_BAD_REQUEST, net::OK);

  scanner_provider->Call(base::BindLambdaForTesting(
      [quit_closure = task_environment_.QuitClosure()](
          std::unique_ptr<proto::ScannerResponse> scanner_response,
          MantaStatus manta_status) {
        EXPECT_EQ(manta_status.status_code, MantaStatusCode::kBackendFailure);
        EXPECT_EQ(scanner_response, nullptr);
        quit_closure.Run();
      }));
  task_environment_.RunUntilQuit();
}

TEST_F(ScannerProviderTest, CaptureNetError) {
  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_OK, net::ERR_FAILED);

  scanner_provider->Call(base::BindLambdaForTesting(
      [quit_closure = task_environment_.QuitClosure()](
          std::unique_ptr<proto::ScannerResponse> scanner_response,
          MantaStatus manta_status) {
        EXPECT_EQ(manta_status.status_code,
                  MantaStatusCode::kNoInternetConnection);
        EXPECT_EQ(scanner_response, nullptr);
        quit_closure.Run();
      }));
  task_environment_.RunUntilQuit();
}

// Test that an unexpected response with a serialized RpcStatus proto can be
// handled properly.
TEST_F(ScannerProviderTest, ParseRpcStatusFromFailedResponse) {
  proto::RpcStatus rpc_status;
  rpc_status.set_code(3);  // Will be mapped to kInvalidInput.
  rpc_status.set_message("foo");

  proto::RpcLocalizedMessage localize_message;
  localize_message.set_message("bar");
  localize_message.set_locale("en");

  auto* detail = rpc_status.add_details();
  detail->set_type_url("type.googleapis.com/google.rpc.LocalizedMessage");
  detail->set_value(localize_message.SerializeAsString());

  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/rpc_status.SerializeAsString(),
                          net::HTTP_BAD_REQUEST, net::OK);

  scanner_provider->Call(base::BindLambdaForTesting(
      [quit_closure = task_environment_.QuitClosure()](
          std::unique_ptr<proto::ScannerResponse> scanner_response,
          MantaStatus manta_status) {
        EXPECT_EQ(manta_status.status_code, MantaStatusCode::kInvalidInput);
        EXPECT_EQ(manta_status.message, "bar");
        EXPECT_EQ(manta_status.locale, "en");
        EXPECT_EQ(scanner_response, nullptr);
        quit_closure.Run();
      }));

  task_environment_.RunUntilQuit();

  // ErrorInfo.reason can be mapped to kRestrictedCountry and override the manta
  // status code.
  proto::RpcErrorInfo error_info;
  error_info.set_reason("RESTRICTED_COUNTRY");
  error_info.set_domain("aratea-pa.googleapis.com");
  detail = rpc_status.add_details();
  detail->set_type_url("type.googleapis.com/google.rpc.ErrorInfo");
  detail->set_value(error_info.SerializeAsString());

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/rpc_status.SerializeAsString(),
                          net::HTTP_BAD_REQUEST, net::OK);

  scanner_provider->Call(base::BindLambdaForTesting(
      [quit_closure = task_environment_.QuitClosure()](
          std::unique_ptr<proto::ScannerResponse> scanner_response,
          MantaStatus manta_status) {
        EXPECT_EQ(manta_status.status_code,
                  MantaStatusCode::kRestrictedCountry);
        EXPECT_EQ(manta_status.message, "bar");
        EXPECT_EQ(manta_status.locale, "en");
        EXPECT_EQ(scanner_response, nullptr);
        quit_closure.Run();
      }));

  task_environment_.RunUntilQuit();
}

TEST_F(ScannerProviderTest, EmptyResponseAfterIdentityManagerShutdown) {
  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  identity_test_env_.reset();

  scanner_provider->Call(base::BindLambdaForTesting(
      [quit_closure = task_environment_.QuitClosure()](
          std::unique_ptr<proto::ScannerResponse> scanner_response,
          MantaStatus manta_status) {
        ASSERT_EQ(MantaStatusCode::kNoIdentityManager,
                  manta_status.status_code);
        EXPECT_EQ(scanner_response, nullptr);
        quit_closure.Run();
      }));
  task_environment_.RunUntilQuit();
}

}  // namespace manta
