// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/orca_provider.h"

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/manta/base_provider.h"
#include "components/manta/base_provider_test_helper.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/rpc_status.pb.h"
#include "components/manta/provider_params.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace manta {

namespace {
constexpr char kMockEndpoint[] = "https://my-endpoint.com";
}

class FakeOrcaProvider : public OrcaProvider, public FakeBaseProvider {
 public:
  FakeOrcaProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : BaseProvider(test_url_loader_factory, identity_manager),
        OrcaProvider(test_url_loader_factory,
                     identity_manager,
                     ProviderParams()),
        FakeBaseProvider(test_url_loader_factory, identity_manager) {}
};

class OrcaProviderTest : public BaseProviderTest {
 public:
  OrcaProviderTest() = default;

  OrcaProviderTest(const OrcaProviderTest&) = delete;
  OrcaProviderTest& operator=(const OrcaProviderTest&) = delete;

  ~OrcaProviderTest() override = default;

  std::unique_ptr<FakeOrcaProvider> CreateOrcaProvider() {
    return std::make_unique<FakeOrcaProvider>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_->identity_manager());
  }
};

// Test OrcaProvider rejects invalid input data. Currently we require the
// input must contain a valid tone.
TEST_F(OrcaProviderTest, PrepareRequestFailure) {
  std::map<std::string, std::string> input = {{"data", "simple post data"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_OK, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response, MantaStatus manta_status) {
                   EXPECT_EQ(manta_status.status_code,
                             MantaStatusCode::kInvalidInput);
                   quit_closure.Run();
                 }));

  task_environment_.RunUntilQuit();
}

// Test that responses with http_status_code != net::HTTP_OK are captured.
TEST_F(OrcaProviderTest, CaptureUnexcpetedStatusCode) {
  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_BAD_REQUEST, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response, MantaStatus manta_status) {
                   EXPECT_EQ(manta_status.status_code,
                             MantaStatusCode::kBackendFailure);
                   quit_closure.Run();
                 }));
  task_environment_.RunUntilQuit();
}

TEST_F(OrcaProviderTest, CaptureNetError) {
  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_OK, net::ERR_FAILED);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response, MantaStatus manta_status) {
                   EXPECT_EQ(manta_status.status_code,
                             MantaStatusCode::kNoInternetConnection);
                   quit_closure.Run();
                 }));
  task_environment_.RunUntilQuit();
}

// Test that malformed proto data can be captured with proper error.
TEST_F(OrcaProviderTest, ParseMalformedSerializedProto) {
  std::string post_data = "{invalid proto";

  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/post_data,
                          net::HTTP_OK, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response, MantaStatus manta_status) {
                   EXPECT_EQ(manta_status.status_code,
                             MantaStatusCode::kMalformedResponse);
                   EXPECT_EQ(manta_status.message, "");
                   EXPECT_EQ(manta_status.locale, "");
                   quit_closure.Run();
                 }));

  task_environment_.RunUntilQuit();
}

// Test that an unexpected response with a serialized RpcStatus proto can be
// handled properly.
TEST_F(OrcaProviderTest, ParseRpcStatusFromFailedResponse) {
  proto::RpcStatus rpc_status;
  rpc_status.set_code(3);  // Will be mapped to kInvalidInput.
  rpc_status.set_message("foo");

  proto::RpcLocalizedMessage localize_message;
  localize_message.set_message("bar");
  localize_message.set_locale("en");

  auto* detail = rpc_status.add_details();
  detail->set_type_url("type.googleapis.com/google.rpc.LocalizedMessage");
  detail->set_value(localize_message.SerializeAsString());

  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/rpc_status.SerializeAsString(),
                          net::HTTP_BAD_REQUEST, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response, MantaStatus manta_status) {
                   EXPECT_EQ(manta_status.status_code,
                             MantaStatusCode::kInvalidInput);
                   EXPECT_EQ(manta_status.message, "bar");
                   EXPECT_EQ(manta_status.locale, "en");
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

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response, MantaStatus manta_status) {
                   EXPECT_EQ(manta_status.status_code,
                             MantaStatusCode::kRestrictedCountry);
                   EXPECT_EQ(manta_status.message, "bar");
                   EXPECT_EQ(manta_status.locale, "en");
                   quit_closure.Run();
                 }));

  task_environment_.RunUntilQuit();
}

// Test a successful response can be parsed as base::Value::Dict.
TEST_F(OrcaProviderTest, ParseSuccessfulResponse) {
  proto::Response response;
  proto::OutputData& output_data = *response.add_output_data();
  output_data.set_text("foo");

  base::HistogramTester histogram_tester;

  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/response.SerializeAsString(),
                          net::HTTP_OK, net::OK);

  orca_provider->Call(
      input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              base::Value::Dict response, MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code, MantaStatusCode::kOk);

            EXPECT_TRUE(response.contains("outputData"));

            const auto* output_data_list = response.FindList("outputData");
            EXPECT_EQ(output_data_list->size(), 1u);

            const base::Value& front_element = output_data_list->front();
            EXPECT_TRUE(front_element.is_dict());
            EXPECT_EQ(*(front_element.GetDict().FindString("text")), "foo");

            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
  histogram_tester.ExpectTotalCount("Ash.MantaService.OrcaProvider.TimeCost",
                                    1);
}

TEST_F(OrcaProviderTest, EmptyResponseAfterIdentityManagerShutdown) {
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  identity_test_env_.reset();

  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict dict, MantaStatus manta_status) {
                   ASSERT_TRUE(dict.empty());
                   ASSERT_EQ(MantaStatusCode::kNoIdentityManager,
                             manta_status.status_code);
                   quit_closure.Run();
                 }));
  task_environment_.RunUntilQuit();
}

}  // namespace manta
