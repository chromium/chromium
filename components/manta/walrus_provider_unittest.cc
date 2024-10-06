// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/walrus_provider.h"

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
#include "components/manta/proto/manta.pb.h"
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

class FakeWalrusProvider : public WalrusProvider, public FakeBaseProvider {
 public:
  FakeWalrusProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : BaseProvider(test_url_loader_factory, identity_manager),
        WalrusProvider(test_url_loader_factory,
                       identity_manager,
                       ProviderParams()),
        FakeBaseProvider(test_url_loader_factory, identity_manager) {}
};

class WalrusProviderTest : public BaseProviderTest {
 public:
  WalrusProviderTest() = default;

  WalrusProviderTest(const WalrusProviderTest&) = delete;
  WalrusProviderTest& operator=(const WalrusProviderTest&) = delete;

  ~WalrusProviderTest() override = default;

  std::unique_ptr<FakeWalrusProvider> CreateWalrusProvider() {
    return std::make_unique<FakeWalrusProvider>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_->identity_manager());
  }
};

// Test that responses with http_status_code != net::HTTP_OK are captured.
TEST_F(WalrusProviderTest, CaptureUnexcpetedStatusCode) {
  std::unique_ptr<FakeWalrusProvider> walrus_provider = CreateWalrusProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_BAD_REQUEST, net::OK);
  std::optional<std::string> text_prompt = "text pompt";
  std::vector<std::vector<uint8_t>> images;

  walrus_provider->Filter(
      text_prompt, images,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              base::Value::Dict response, MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code,
                      MantaStatusCode::kBackendFailure);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

// Test that responses with network errors are captured.
TEST_F(WalrusProviderTest, CaptureNetError) {
  std::unique_ptr<FakeWalrusProvider> walrus_provider = CreateWalrusProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_OK, net::ERR_FAILED);
  std::optional<std::string> text_prompt = "text pompt";
  std::vector<std::vector<uint8_t>> images;

  walrus_provider->Filter(
      text_prompt, images,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              base::Value::Dict response, MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code,
                      MantaStatusCode::kNoInternetConnection);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

// Test Manta Provider rejects invalid input data. Currently we require the
// input must contain a valid text prompt or an image.
TEST_F(WalrusProviderTest, InvalidInput) {
  std::unique_ptr<FakeWalrusProvider> walrus_provider = CreateWalrusProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_OK, net::OK);
  std::optional<std::string> text_prompt;
  std::vector<std::vector<uint8_t>> images;

  walrus_provider->Filter(
      text_prompt, images,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              base::Value::Dict response, MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code, MantaStatusCode::kInvalidInput);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

// Test the response when the text prompt / image is safe.
TEST_F(WalrusProviderTest, SuccessfulResponse) {
  std::string image_bytes = "image_bytes";
  base::HistogramTester histogram_tester;
  manta::proto::Response response;
  auto* output_data = response.add_output_data();
  output_data->set_text("text pompt");
  output_data = response.add_output_data();
  output_data->mutable_image()->set_serialized_bytes(image_bytes);

  std::string response_data;
  response.SerializeToString(&response_data);

  SetEndpointMockResponse(GURL{kMockEndpoint}, response_data, net::HTTP_OK,
                          net::OK);
  std::unique_ptr<FakeWalrusProvider> walrus_provider = CreateWalrusProvider();
  auto quit_closure = task_environment_.QuitClosure();
  std::optional<std::string> text_prompt = "text pompt";
  std::vector<std::vector<uint8_t>> images = {
      std::vector<uint8_t>(image_bytes.begin(), image_bytes.end())};

  walrus_provider->Filter(
      text_prompt, images,
      base::BindLambdaForTesting([&quit_closure](base::Value::Dict response,
                                                 MantaStatus manta_status) {
        // Even though the response has text and image, walrus just
        // returns the status code
        ASSERT_EQ(MantaStatusCode::kOk, manta_status.status_code);
        ASSERT_TRUE(response.empty());
        quit_closure.Run();
      }));
  task_environment_.RunUntilQuit();

  // Metric is logged when response is successfully parsed.
  histogram_tester.ExpectTotalCount("Ash.MantaService.WalrusProvider.TimeCost",
                                    1);
}

// Test the response when the text prompt is blocked.
TEST_F(WalrusProviderTest, TextBlocked) {
  std::string image_bytes = "image_bytes";
  base::HistogramTester histogram_tester;
  manta::proto::Response response;
  manta::proto::FilteredData& filtered_data = *response.add_filtered_data();
  filtered_data.set_reason(manta::proto::FilteredReason::TEXT_SAFETY);
  std::string response_data;
  response.SerializeToString(&response_data);

  SetEndpointMockResponse(GURL{kMockEndpoint}, response_data, net::HTTP_OK,
                          net::OK);
  std::unique_ptr<FakeWalrusProvider> walrus_provider = CreateWalrusProvider();
  auto quit_closure = task_environment_.QuitClosure();
  std::optional<std::string> text_prompt = "text pompt";
  std::vector<std::vector<uint8_t>> images = {
      std::vector<uint8_t>(image_bytes.begin(), image_bytes.end())};

  walrus_provider->Filter(
      text_prompt, images,
      base::BindLambdaForTesting([&quit_closure](base::Value::Dict response,
                                                 MantaStatus manta_status) {
        // Even though the response has text and image, walrus just
        // returns the status code.
        ASSERT_EQ(MantaStatusCode::kBlockedOutputs, manta_status.status_code);
        ASSERT_EQ(response.size(), 1u);
        ASSERT_TRUE(response.FindBool("text_blocked"));
        quit_closure.Run();
      }));
  task_environment_.RunUntilQuit();

  // Metric is logged when response is successfully parsed.
  histogram_tester.ExpectTotalCount("Ash.MantaService.WalrusProvider.TimeCost",
                                    1);
}

// Test the response when the text prompt and images is blocked.
TEST_F(WalrusProviderTest, TextImageBothBlocked) {
  std::string image_bytes = "image_bytes";
  base::HistogramTester histogram_tester;
  manta::proto::Response response;
  auto* filtered_data = response.add_filtered_data();
  filtered_data->set_reason(manta::proto::FilteredReason::TEXT_SAFETY);
  filtered_data = response.add_filtered_data();
  filtered_data->set_reason(manta::proto::FilteredReason::IMAGE_SAFETY);
  filtered_data = response.add_filtered_data();
  filtered_data->set_reason(manta::proto::FilteredReason::IMAGE_SAFETY);
  std::string response_data;
  response.SerializeToString(&response_data);

  SetEndpointMockResponse(GURL{kMockEndpoint}, response_data, net::HTTP_OK,
                          net::OK);
  std::unique_ptr<FakeWalrusProvider> walrus_provider = CreateWalrusProvider();
  auto quit_closure = task_environment_.QuitClosure();
  std::optional<std::string> text_prompt = "text pompt";
  std::vector<std::vector<uint8_t>> images = {
      std::vector<uint8_t>(image_bytes.begin(), image_bytes.end()),
      std::vector<uint8_t>(image_bytes.begin(), image_bytes.end())};

  walrus_provider->Filter(
      text_prompt, images,
      base::BindLambdaForTesting([&quit_closure](base::Value::Dict response,
                                                 MantaStatus manta_status) {
        // Even though the response has text and image, walrus just
        // returns the status code
        ASSERT_EQ(MantaStatusCode::kBlockedOutputs, manta_status.status_code);
        ASSERT_EQ(response.size(), 2u);
        ASSERT_TRUE(response.FindBool("text_blocked"));
        ASSERT_TRUE(response.FindBool("image_blocked"));
        quit_closure.Run();
      }));
  task_environment_.RunUntilQuit();

  // Metric is logged when response is successfully parsed.
  histogram_tester.ExpectTotalCount("Ash.MantaService.WalrusProvider.TimeCost",
                                    1);
}

TEST_F(WalrusProviderTest, EmptyResponseAfterIdentityManagerShutdown) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<FakeWalrusProvider> wallrus_provider = CreateWalrusProvider();

  identity_test_env_.reset();

  std::string text_prompt = "text pompt";
  wallrus_provider->Filter(
      text_prompt, base::BindLambdaForTesting(
                       [quit_closure = task_environment_.QuitClosure()](
                           base::Value::Dict dict, MantaStatus manta_status) {
                         ASSERT_TRUE(dict.empty());
                         ASSERT_EQ(MantaStatusCode::kNoIdentityManager,
                                   manta_status.status_code);
                         quit_closure.Run();
                       }));
  task_environment_.RunUntilQuit();

  // No metric logged.
  histogram_tester.ExpectTotalCount("Ash.MantaService.WalrusProvider.TimeCost",
                                    0);
}

}  // namespace manta
