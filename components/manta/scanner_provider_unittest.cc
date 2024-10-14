// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/scanner_provider.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "components/manta/base_provider.h"
#include "components/manta/base_provider_test_helper.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/common.pb.h"
#include "components/manta/proto/manta.pb.h"
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

using ::base::test::EqualsProto;
using ::testing::Pointee;

constexpr char kMockEndpoint[] = "https://my-endpoint.com";

proto::ScannerInput CreateValidScannerInput() {
  proto::ScannerInput scanner_input;
  scanner_input.set_image("image");
  return scanner_input;
}

proto::ScannerOutput CreateValidScannerOutput() {
  // Create a ScannerOutput proto.
  proto::ScannerOutput scanner_output;
  proto::ScannerObject* scanner_object = scanner_output.add_objects();
  proto::ScannerAction* scanner_action = scanner_object->add_actions();

  proto::NewContactAction* new_contact_action =
      scanner_action->mutable_new_contact();
  new_contact_action->set_given_name("Foo");
  new_contact_action->set_family_name("Bar");
  return scanner_output;
}

std::unique_ptr<proto::Response> CreateValidResponseWithScannerOutput() {
  auto manta_response = std::make_unique<proto::Response>();

  // Populate a custom data within the Manta response.
  proto::OutputData* output_data = manta_response->add_output_data();
  proto::Proto3Any& custom_data = *output_data->mutable_custom();
  custom_data.set_type_url(kScannerOutputTypeUrl);
  auto scanner_output = CreateValidScannerOutput();
  custom_data.set_value(scanner_output.SerializeAsString());

  return manta_response;
}
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
  auto scanner_input = CreateValidScannerInput();
  auto manta_response = CreateValidResponseWithScannerOutput();

  base::HistogramTester histogram_tester;

  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/manta_response->SerializeAsString(),
                          net::HTTP_OK, net::OK);

  scanner_provider->Call(
      scanner_input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<proto::ScannerOutput> scanner_output,
              MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code, MantaStatusCode::kOk);

            EXPECT_THAT(scanner_output,
                        Pointee(EqualsProto(CreateValidScannerOutput())));
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
  histogram_tester.ExpectTotalCount("Ash.MantaService.ScannerProvider.TimeCost",
                                    1);
}

TEST_F(ScannerProviderTest, InvalidInputErrorIfNoImageProvided) {
  auto scanner_input = CreateValidScannerInput();
  scanner_input.clear_image();

  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/"", net::HTTP_OK, net::OK);

  scanner_provider->Call(
      scanner_input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<proto::ScannerOutput> scanner_output,
              MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code, MantaStatusCode::kInvalidInput);
            EXPECT_EQ(scanner_output, nullptr);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

TEST_F(ScannerProviderTest, BlockedOutputsErrorIfNoOutputData) {
  auto scanner_input = CreateValidScannerInput();
  auto manta_response = CreateValidResponseWithScannerOutput();
  manta_response->clear_output_data();
  auto* filtered_data = manta_response->add_filtered_data();
  filtered_data->set_is_output_data(true);
  filtered_data->set_reason(proto::FilteredReason::TEXT_SAFETY);

  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/manta_response->SerializeAsString(),
                          net::HTTP_OK, net::OK);

  scanner_provider->Call(
      scanner_input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<proto::ScannerOutput> scanner_output,
              MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code,
                      MantaStatusCode::kBlockedOutputs);
            EXPECT_EQ(manta_status.message, "filtered output for: TEXT_SAFETY");
            EXPECT_EQ(scanner_output, nullptr);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

TEST_F(ScannerProviderTest, MalformedResponseErrorIfMoreThanOneOutputData) {
  auto scanner_input = CreateValidScannerInput();
  auto manta_response = CreateValidResponseWithScannerOutput();
  manta_response->add_output_data();

  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/manta_response->SerializeAsString(),
                          net::HTTP_OK, net::OK);

  scanner_provider->Call(
      scanner_input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<proto::ScannerOutput> scanner_output,
              MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code,
                      MantaStatusCode::kMalformedResponse);
            EXPECT_EQ(scanner_output, nullptr);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

TEST_F(ScannerProviderTest, MalformedResponseErrorIfNoCustomOutputData) {
  auto scanner_input = CreateValidScannerInput();
  auto manta_response = CreateValidResponseWithScannerOutput();
  // Clear the custom field.
  manta_response->mutable_output_data(0)->clear_custom();

  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/manta_response->SerializeAsString(),
                          net::HTTP_OK, net::OK);

  scanner_provider->Call(
      scanner_input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<proto::ScannerOutput> scanner_output,
              MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code,
                      MantaStatusCode::kMalformedResponse);
            EXPECT_EQ(scanner_output, nullptr);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

TEST_F(ScannerProviderTest, MalformedResponseErrorIfInvalidProtoTypeUrl) {
  auto scanner_input = CreateValidScannerInput();
  auto manta_response = CreateValidResponseWithScannerOutput();
  // Clear the custom field.
  manta_response->mutable_output_data(0)->mutable_custom()->set_type_url("foo");

  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/manta_response->SerializeAsString(),
                          net::HTTP_OK, net::OK);

  scanner_provider->Call(
      scanner_input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<proto::ScannerOutput> scanner_output,
              MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code,
                      MantaStatusCode::kMalformedResponse);
            EXPECT_EQ(scanner_output, nullptr);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

// Test that responses with http_status_code != net::HTTP_OK are captured.
TEST_F(ScannerProviderTest, CaptureUnexpectedStatusCode) {
  auto scanner_input = CreateValidScannerInput();

  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/"", net::HTTP_BAD_REQUEST, net::OK);

  scanner_provider->Call(
      scanner_input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<proto::ScannerOutput> scanner_output,
              MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code,
                      MantaStatusCode::kBackendFailure);
            EXPECT_EQ(scanner_output, nullptr);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

TEST_F(ScannerProviderTest, CaptureNetError) {
  auto scanner_input = CreateValidScannerInput();

  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_OK, net::ERR_FAILED);

  scanner_provider->Call(
      scanner_input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<proto::ScannerOutput> scanner_output,
              MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code,
                      MantaStatusCode::kNoInternetConnection);
            EXPECT_EQ(scanner_output, nullptr);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

// Test that an unexpected response with a serialized RpcStatus proto can be
// handled properly.
TEST_F(ScannerProviderTest, ParseRpcStatusFromFailedResponse) {
  auto scanner_input = CreateValidScannerInput();

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

  scanner_provider->Call(
      scanner_input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<proto::ScannerOutput> scanner_output,
              MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code, MantaStatusCode::kInvalidInput);
            EXPECT_EQ(manta_status.message, "bar");
            EXPECT_EQ(manta_status.locale, "en");
            EXPECT_EQ(scanner_output, nullptr);
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

  scanner_provider->Call(
      scanner_input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<proto::ScannerOutput> scanner_output,
              MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code,
                      MantaStatusCode::kRestrictedCountry);
            EXPECT_EQ(manta_status.message, "bar");
            EXPECT_EQ(manta_status.locale, "en");
            EXPECT_EQ(scanner_output, nullptr);
            quit_closure.Run();
          }));

  task_environment_.RunUntilQuit();
}

TEST_F(ScannerProviderTest, EmptyResponseAfterIdentityManagerShutdown) {
  auto scanner_input = CreateValidScannerInput();

  std::unique_ptr<FakeScannerProvider> scanner_provider =
      CreateScannerProvider();

  identity_test_env_.reset();

  scanner_provider->Call(
      scanner_input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<proto::ScannerOutput> scanner_output,
              MantaStatus manta_status) {
            ASSERT_EQ(MantaStatusCode::kNoIdentityManager,
                      manta_status.status_code);
            EXPECT_EQ(scanner_output, nullptr);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

}  // namespace manta
