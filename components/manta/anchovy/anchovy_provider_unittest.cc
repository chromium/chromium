// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/anchovy/anchovy_provider.h"

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "components/manta/anchovy/anchovy_proto_helper.h"
#include "components/manta/anchovy/anchovy_requests.h"
#include "components/manta/base_provider_test_helper.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/anchovy.pb.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/provider_params.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace manta {

namespace {

constexpr char kMockEndpoint[] = "https://my-endpoint.com";

void AddAnchovyOutputData(proto::Response* response,
                          proto::AnchovyDescription::Type type,
                          const std::string& text,
                          float score) {
  auto* output_data = response->add_output_data();
  output_data->set_score(score);
  proto::AnchovyDescription desc;
  desc.set_type(type);
  desc.set_text(text);
  output_data->mutable_custom()->set_type_url(
      "type.googleapis.com/manta.proto.AnchovyDescription");
  output_data->mutable_custom()->set_value(desc.SerializeAsString());
}

}  // namespace

class FakeAnchovyProvider : public AnchovyProvider, public FakeBaseProvider {
 public:
  FakeAnchovyProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : BaseProvider(test_url_loader_factory, identity_manager),
        AnchovyProvider(test_url_loader_factory,
                        identity_manager,
                        ProviderParams()),
        FakeBaseProvider(test_url_loader_factory, identity_manager) {}
};

class AnchovyProviderTest : public BaseProviderTest {
 public:
  AnchovyProviderTest() = default;
  AnchovyProviderTest(const AnchovyProviderTest&) = delete;
  AnchovyProviderTest& operator=(const AnchovyProviderTest&) = delete;
  ~AnchovyProviderTest() override = default;

  std::unique_ptr<FakeAnchovyProvider> CreateAnchovyProvider() {
    return std::make_unique<FakeAnchovyProvider>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_->identity_manager());
  }
};

// Verifies that ComposeRequest populates the feature name,
// ImageDescriptionContext type URL and language tag, and raw image bytes.
TEST(AnchovyProtoHelperTest, ComposeRequest) {
  const std::vector<uint8_t> kImageBytes = {10, 20, 30, 40};
  anchovy::ImageDescriptionRequest request("https://example.com/img.jpg", "fr",
                                           kImageBytes);

  proto::Request request_proto =
      anchovy::AnchovyProtoHelper::ComposeRequest(request);

  EXPECT_EQ(request_proto.feature_name(),
            proto::FeatureName::ACCESSIBILITY_IMAGE_DESCRIPTION);
  ASSERT_TRUE(request_proto.has_request_config());
  ASSERT_TRUE(request_proto.request_config().has_specific_options());
  EXPECT_EQ(request_proto.request_config().specific_options().type_url(),
            "type.googleapis.com/image_repository.api.ImageDescriptionContext");

  proto::AnchovyContext context;
  ASSERT_TRUE(context.ParseFromString(
      request_proto.request_config().specific_options().value()));
  EXPECT_EQ(context.lang_id(), "fr");
  EXPECT_TRUE(context.optional_inputs().input1());

  ASSERT_EQ(request_proto.input_data_size(), 1);
  ASSERT_TRUE(request_proto.input_data(0).has_image());
  EXPECT_EQ(request_proto.input_data(0).image().serialized_bytes(),
            std::string(kImageBytes.begin(), kImageBytes.end()));
}

// Verifies that HandleImageDescriptionResponse distinguishes PRIMARY_CAPTION,
// SECONDARY_CAPTION, LABEL, and OCR entries.
TEST(AnchovyProtoHelperTest, HandleImageDescriptionResponseValidDescriptions) {
  auto response = std::make_unique<proto::Response>();
  AddAnchovyOutputData(response.get(),
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION,
                       "A person riding a bicycle", 0.85f);
  AddAnchovyOutputData(response.get(),
                       proto::AnchovyDescription_Type_SECONDARY_CAPTION,
                       "Mountain View", 0.75f);
  AddAnchovyOutputData(response.get(), proto::AnchovyDescription_Type_LABEL,
                       "bicycle", 0.95f);
  AddAnchovyOutputData(response.get(), proto::AnchovyDescription_Type_OCR,
                       "STOP", 0.90f);

  base::test::TestFuture<base::DictValue, MantaStatus> future;
  anchovy::AnchovyProtoHelper::HandleImageDescriptionResponse(
      future.GetCallback(), std::move(response), {MantaStatusCode::kOk, "ok"});

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get<1>().status_code, MantaStatusCode::kOk);
  const base::ListValue* results = future.Get<0>().FindList("results");
  ASSERT_NE(results, nullptr);
  ASSERT_EQ(results->size(), 4u);

  EXPECT_EQ(*(*results)[0].GetDict().FindString("type"),
            anchovy::kTypePrimaryCaption);
  EXPECT_EQ(*(*results)[0].GetDict().FindString("text"),
            "A person riding a bicycle");
  EXPECT_NEAR(*(*results)[0].GetDict().FindDouble("score"), 0.85, 1e-5);

  EXPECT_EQ(*(*results)[1].GetDict().FindString("type"),
            anchovy::kTypeSecondaryCaption);
  EXPECT_EQ(*(*results)[1].GetDict().FindString("text"), "Mountain View");
  EXPECT_NEAR(*(*results)[1].GetDict().FindDouble("score"), 0.75, 1e-5);

  EXPECT_EQ(*(*results)[2].GetDict().FindString("type"), anchovy::kTypeLabel);
  EXPECT_EQ(*(*results)[2].GetDict().FindString("text"), "bicycle");
  EXPECT_NEAR(*(*results)[2].GetDict().FindDouble("score"), 0.95, 1e-5);

  EXPECT_EQ(*(*results)[3].GetDict().FindString("type"), anchovy::kTypeOcr);
  EXPECT_EQ(*(*results)[3].GetDict().FindString("text"), "STOP");
  EXPECT_NEAR(*(*results)[3].GetDict().FindDouble("score"), 0.90, 1e-5);
}

// Verifies that missing custom fields on earlier entries, malformed Any
// payloads, empty text, and UNKNOWN/UNUSED enum types are safely skipped
// without dropping valid subsequent entries.
TEST(AnchovyProtoHelperTest,
     HandleImageDescriptionResponseSkipsInvalidEntries) {
  auto response = std::make_unique<proto::Response>();

  // Entry 0: no custom proto field (should not abort the rest of the response).
  response->add_output_data()->set_text("non-custom text");

  // Entry 1: malformed protobuf wire bytes in custom.value.
  auto* malformed_data = response->add_output_data();
  malformed_data->set_score(0.9f);
  malformed_data->mutable_custom()->set_value("\xFF\xFF\xFF\xFF");

  // Entry 2: UNKNOWN description type.
  AddAnchovyOutputData(response.get(), proto::AnchovyDescription_Type_UNKNOWN,
                       "unknown type", 0.8f);

  // Entry 3: UNUSED description type.
  AddAnchovyOutputData(response.get(), proto::AnchovyDescription_Type_UNUSED,
                       "unused type", 0.8f);

  // Entry 4: unrecognized proto3 open enum integer value.
  AddAnchovyOutputData(response.get(),
                       static_cast<proto::AnchovyDescription::Type>(99),
                       "unrecognized enum value", 0.8f);

  // Entry 5: empty text string.
  AddAnchovyOutputData(
      response.get(), proto::AnchovyDescription_Type_PRIMARY_CAPTION, "", 0.8f);

  // Entry 6: non-finite (NaN and infinite) float scores.
  AddAnchovyOutputData(response.get(),
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION,
                       "nan score", std::numeric_limits<float>::quiet_NaN());
  AddAnchovyOutputData(response.get(),
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION,
                       "inf score", std::numeric_limits<float>::infinity());

  // Entry 7: valid PRIMARY_CAPTION entry.
  AddAnchovyOutputData(response.get(),
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION,
                       "A golden retriever on grass", 0.88f);

  base::test::TestFuture<base::DictValue, MantaStatus> future;
  anchovy::AnchovyProtoHelper::HandleImageDescriptionResponse(
      future.GetCallback(), std::move(response), {MantaStatusCode::kOk, "ok"});

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get<1>().status_code, MantaStatusCode::kOk);
  const base::ListValue* results = future.Get<0>().FindList("results");
  ASSERT_NE(results, nullptr);
  ASSERT_EQ(results->size(), 1u);
  EXPECT_EQ(*(*results)[0].GetDict().FindString("type"),
            anchovy::kTypePrimaryCaption);
  EXPECT_EQ(*(*results)[0].GetDict().FindString("text"),
            "A golden retriever on grass");
}

// Verifies that non-OK MantaStatusCode values and empty/all-invalid responses
// return an empty DictValue.
TEST(AnchovyProtoHelperTest, HandleImageDescriptionResponseErrorAndEmpty) {
  // Non-OK status with null response returns empty dict and forwards status.
  base::test::TestFuture<base::DictValue, MantaStatus> error_future;
  anchovy::AnchovyProtoHelper::HandleImageDescriptionResponse(
      error_future.GetCallback(), nullptr,
      {MantaStatusCode::kBackendFailure, "backend error"});
  ASSERT_TRUE(error_future.Wait());
  EXPECT_TRUE(error_future.Get<0>().empty());
  EXPECT_EQ(error_future.Get<1>().status_code,
            MantaStatusCode::kBackendFailure);

  // OK status with empty output_data returns empty dict.
  base::test::TestFuture<base::DictValue, MantaStatus> empty_future;
  anchovy::AnchovyProtoHelper::HandleImageDescriptionResponse(
      empty_future.GetCallback(), std::make_unique<proto::Response>(),
      {MantaStatusCode::kOk, "ok", "en"});
  ASSERT_TRUE(empty_future.Wait());
  EXPECT_TRUE(empty_future.Get<0>().empty());
  EXPECT_EQ(empty_future.Get<1>().status_code, MantaStatusCode::kOk);
  EXPECT_EQ(empty_future.Get<1>().locale, "en");

  // OK status where all output_data entries are invalid also returns empty
  // dict.
  auto all_invalid_response = std::make_unique<proto::Response>();
  AddAnchovyOutputData(all_invalid_response.get(),
                       proto::AnchovyDescription_Type_UNKNOWN, "unknown", 0.5f);
  base::test::TestFuture<base::DictValue, MantaStatus> invalid_future;
  anchovy::AnchovyProtoHelper::HandleImageDescriptionResponse(
      invalid_future.GetCallback(), std::move(all_invalid_response),
      {MantaStatusCode::kOk, "ok"});
  ASSERT_TRUE(invalid_future.Wait());
  EXPECT_TRUE(invalid_future.Get<0>().empty());
  EXPECT_EQ(invalid_future.Get<1>().status_code, MantaStatusCode::kOk);
}

TEST_F(AnchovyProviderTest, GetImageDescriptionSuccess) {
  proto::Response response;
  AddAnchovyOutputData(&response,
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION,
                       "A cat sitting on a windowsill", 0.92f);

  base::HistogramTester histogram_tester;
  std::unique_ptr<FakeAnchovyProvider> anchovy_provider =
      CreateAnchovyProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/response.SerializeAsString(),
                          net::HTTP_OK, net::OK);

  const std::vector<uint8_t> kImageBytes = {1, 2, 3};
  anchovy::ImageDescriptionRequest request("https://example.com/cat.jpg", "en",
                                           kImageBytes);
  anchovy_provider->GetImageDescription(
      request, TRAFFIC_ANNOTATION_FOR_TESTS,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              base::DictValue dict, MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code, MantaStatusCode::kOk);
            const auto* results = dict.FindList("results");
            ASSERT_NE(results, nullptr);
            ASSERT_EQ(results->size(), 1u);
            EXPECT_EQ(*(*results)[0].GetDict().FindString("type"),
                      anchovy::kTypePrimaryCaption);
            EXPECT_EQ(*(*results)[0].GetDict().FindString("text"),
                      "A cat sitting on a windowsill");
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
  histogram_tester.ExpectTotalCount("Ash.MantaService.AnchovyProvider.TimeCost",
                                    1);
}

TEST_F(AnchovyProviderTest, GetImageDescriptionBackendFailure) {
  std::unique_ptr<FakeAnchovyProvider> anchovy_provider =
      CreateAnchovyProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_BAD_REQUEST, net::OK);

  const std::vector<uint8_t> kImageBytes = {1, 2, 3};
  anchovy::ImageDescriptionRequest request("https://example.com/cat.jpg", "en",
                                           kImageBytes);
  anchovy_provider->GetImageDescription(
      request, TRAFFIC_ANNOTATION_FOR_TESTS,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              base::DictValue dict, MantaStatus manta_status) {
            EXPECT_TRUE(dict.empty());
            EXPECT_EQ(manta_status.status_code,
                      MantaStatusCode::kBackendFailure);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

}  // namespace manta
