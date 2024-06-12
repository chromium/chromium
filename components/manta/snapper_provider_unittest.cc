// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/snapper_provider.h"

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/manta/base_provider_test_helper.h"
#include "components/manta/manta_status.h"
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
}  // namespace

class FakeSnapperProvider : public SnapperProvider, public FakeBaseProvider {
 public:
  FakeSnapperProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : BaseProvider(test_url_loader_factory, identity_manager),
        SnapperProvider(test_url_loader_factory, identity_manager),
        FakeBaseProvider(test_url_loader_factory, identity_manager) {}
};

class SnapperProviderTest : public BaseProviderTest {
 public:
  SnapperProviderTest() = default;

  SnapperProviderTest(const SnapperProviderTest&) = delete;
  SnapperProviderTest& operator=(const SnapperProviderTest&) = delete;

  ~SnapperProviderTest() override = default;

  std::unique_ptr<FakeSnapperProvider> CreateSnapperProvider() {
    return std::make_unique<FakeSnapperProvider>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_->identity_manager());
  }
};

// Test POST data is correctly passed from SnapperProvider to EndpointFetcher.
TEST_F(SnapperProviderTest, SimpleRequestPayload) {
  base::HistogramTester histogram_tester;
  manta::proto::Response response;
  manta::proto::OutputData& output_data = *response.add_output_data();
  manta::proto::Image& image = *output_data.mutable_image();
  image.set_serialized_bytes("foo");
  std::string response_data;
  response.SerializeToString(&response_data);

  SetEndpointMockResponse(GURL{kMockEndpoint}, response_data, net::HTTP_OK,
                          net::OK);
  std::unique_ptr<FakeSnapperProvider> snapper_provider =
      CreateSnapperProvider();
  auto quit_closure = task_environment_.QuitClosure();

  manta::proto::Request request;
  snapper_provider->Call(
      request, TRAFFIC_ANNOTATION_FOR_TESTS,
      base::BindLambdaForTesting(
          [&quit_closure](std::unique_ptr<manta::proto::Response> response,
                          MantaStatus manta_status) {
            ASSERT_EQ(manta_status.status_code, MantaStatusCode::kOk);
            ASSERT_TRUE(response);
            ASSERT_EQ(1, response->output_data_size());
            ASSERT_TRUE(response->output_data(0).has_image());
            ASSERT_TRUE(
                response->output_data(0).image().has_serialized_bytes());
            EXPECT_EQ("foo",
                      response->output_data(0).image().serialized_bytes());
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();

  // Metric is logged when respose is successfully parsed.
  histogram_tester.ExpectTotalCount("Ash.MantaService.SnapperProvider.TimeCost",
                                    1);
}

TEST_F(SnapperProviderTest, EmptyResponseAfterIdentityManagerShutdown) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<FakeSnapperProvider> snapper_provider =
      CreateSnapperProvider();

  identity_test_env_.reset();

  manta::proto::Request request;
  snapper_provider->Call(
      request, TRAFFIC_ANNOTATION_FOR_TESTS,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<manta::proto::Response> response,
              MantaStatus manta_status) {
            ASSERT_FALSE(response);
            ASSERT_EQ(MantaStatusCode::kNoIdentityManager,
                      manta_status.status_code);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();

  // No metric logged.
  histogram_tester.ExpectTotalCount("Ash.MantaService.SnapperProvider.TimeCost",
                                    0);
}

}  // namespace manta
