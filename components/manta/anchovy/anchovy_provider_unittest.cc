// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/anchovy/anchovy_provider.h"

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
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
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace manta {

namespace {

constexpr char kMockEndpoint[] = "https://my-endpoint.com";

void AddAnchovyOutputData(
    proto::Response* response,
    proto::AnchovyDescription::Type type,
    const std::string& text,
    float score,
    std::string_view tag = "",
    std::string_view type_url =
        "type.googleapis.com/manta.proto.AnchovyDescription") {
  auto* output_data = response->add_output_data();
  output_data->set_score(score);
  if (!tag.empty()) {
    output_data->set_tag(tag);
  }
  proto::AnchovyDescription desc;
  desc.set_type(type);
  desc.set_text(text);
  output_data->mutable_custom()->set_type_url(type_url);
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

// Verifies compile-time rvalue temporary rejection, noexcept move traits, and
// runtime move construction/assignment for batch request structs.
TEST(AnchovyProtoHelperTest, BatchRequestAndEntryMoveOperations) {
  static_assert(
      std::is_constructible_v<anchovy::ImageDescriptionBatchEntry, std::string,
                              const std::vector<uint8_t>&>);
  static_assert(!std::is_constructible_v<anchovy::ImageDescriptionBatchEntry,
                                         std::string, std::vector<uint8_t>&&>);
  static_assert(
      !std::is_constructible_v<anchovy::ImageDescriptionBatchEntry, std::string,
                               const std::vector<uint8_t>&&>);
  static_assert(std::is_nothrow_move_constructible_v<
                anchovy::ImageDescriptionBatchEntry>);
  static_assert(
      std::is_nothrow_move_assignable_v<anchovy::ImageDescriptionBatchEntry>);
  static_assert(std::is_nothrow_move_constructible_v<
                anchovy::ImageDescriptionBatchRequest>);
  static_assert(
      std::is_nothrow_move_assignable_v<anchovy::ImageDescriptionBatchRequest>);

  const std::vector<uint8_t> kImg0 = {1, 2, 3};
  const std::vector<uint8_t> kImg1 = {4, 5};

  anchovy::ImageDescriptionBatchEntry entry0("0", kImg0);
  anchovy::ImageDescriptionBatchEntry entry1("1", kImg1);
  entry0 = std::move(entry1);
  EXPECT_EQ(entry0.tag, "1");
  EXPECT_EQ(*entry0.image_bytes, kImg1);

  anchovy::ImageDescriptionBatchRequest source("de");
  source.images.push_back(std::move(entry0));

  anchovy::ImageDescriptionBatchRequest move_constructed(std::move(source));
  EXPECT_EQ(move_constructed.lang_tag, "de");
  ASSERT_EQ(move_constructed.images.size(), 1u);
  EXPECT_EQ(move_constructed.images[0].tag, "1");
  EXPECT_EQ(*move_constructed.images[0].image_bytes, kImg1);

  anchovy::ImageDescriptionBatchRequest move_assigned("en");
  move_assigned = std::move(move_constructed);
  EXPECT_EQ(move_assigned.lang_tag, "de");
  ASSERT_EQ(move_assigned.images.size(), 1u);
  EXPECT_EQ(move_assigned.images[0].tag, "1");
  EXPECT_EQ(*move_assigned.images[0].image_bytes, kImg1);
}

// Verifies that ComposeRequest(ImageDescriptionBatchRequest) populates shared
// context options once and tags each image payload in order.
TEST(AnchovyProtoHelperTest, ComposeBatchRequest) {
  const std::vector<uint8_t> kImg0 = {1, 2, 3};
  const std::vector<uint8_t> kImg1 = {4, 5};
  const std::vector<uint8_t> kImg2 = {6, 7, 8, 9};

  anchovy::ImageDescriptionBatchRequest batch("fr");
  batch.images.emplace_back("0", kImg0);
  batch.images.emplace_back("1", kImg1);
  batch.images.emplace_back("2", kImg2);

  proto::Request request_proto =
      anchovy::AnchovyProtoHelper::ComposeRequest(batch);

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
  EXPECT_FALSE(context.has_version());
  EXPECT_FALSE(context.has_options());

  ASSERT_EQ(request_proto.input_data_size(), 3);
  EXPECT_EQ(request_proto.input_data(0).tag(), "0");
  EXPECT_EQ(request_proto.input_data(0).image().serialized_bytes(),
            std::string(kImg0.begin(), kImg0.end()));
  EXPECT_EQ(request_proto.input_data(1).tag(), "1");
  EXPECT_EQ(request_proto.input_data(1).image().serialized_bytes(),
            std::string(kImg1.begin(), kImg1.end()));
  EXPECT_EQ(request_proto.input_data(2).tag(), "2");
  EXPECT_EQ(request_proto.input_data(2).image().serialized_bytes(),
            std::string(kImg2.begin(), kImg2.end()));
}

// Verifies that ComposeRequest(ImageDescriptionBatchRequest) CHECK-crashes on
// an empty image list, empty tags, non-UTF-8 tags, duplicate tags, or empty
// image byte buffers.
TEST(AnchovyProtoHelperDeathTest, ComposeBatchRequestRejectsInvalidEntries) {
  const std::vector<uint8_t> kValidImg = {1, 2, 3};
  const std::vector<uint8_t> kEmptyImg = {};

  anchovy::ImageDescriptionBatchRequest empty_batch("en");
  EXPECT_CHECK_DEATH(anchovy::AnchovyProtoHelper::ComposeRequest(empty_batch));

  anchovy::ImageDescriptionBatchRequest empty_tag_batch("en");
  empty_tag_batch.images.emplace_back("", kValidImg);
  EXPECT_CHECK_DEATH(
      anchovy::AnchovyProtoHelper::ComposeRequest(empty_tag_batch));

  anchovy::ImageDescriptionBatchRequest invalid_utf8_batch("en");
  invalid_utf8_batch.images.emplace_back("\xC0\xAF", kValidImg);
  EXPECT_CHECK_DEATH(
      anchovy::AnchovyProtoHelper::ComposeRequest(invalid_utf8_batch));

  anchovy::ImageDescriptionBatchRequest duplicate_tag_batch("en");
  duplicate_tag_batch.images.emplace_back("0", kValidImg);
  duplicate_tag_batch.images.emplace_back("0", kValidImg);
  EXPECT_CHECK_DEATH(
      anchovy::AnchovyProtoHelper::ComposeRequest(duplicate_tag_batch));

  anchovy::ImageDescriptionBatchRequest empty_bytes_batch("en");
  empty_bytes_batch.images.emplace_back("0", kEmptyImg);
  EXPECT_CHECK_DEATH(
      anchovy::AnchovyProtoHelper::ComposeRequest(empty_bytes_batch));
}

// Verifies that HandleImageDescriptionsResponse groups multiple annotations per
// tag in order, preserves server-style custom type URLs, omits absent tags, and
// skips untagged, non-UTF-8, or malformed entries without dropping siblings.
TEST(AnchovyProtoHelperTest, HandleImageDescriptionsResponseGroupsByTag) {
  auto response = std::make_unique<proto::Response>();

  // Four valid annotations sharing tag "0".
  AddAnchovyOutputData(response.get(),
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION,
                       "A person riding a bicycle", 0.84f, "0");
  AddAnchovyOutputData(response.get(),
                       proto::AnchovyDescription_Type_SECONDARY_CAPTION,
                       "Golden Gate Park", 1.35f, "0");
  AddAnchovyOutputData(response.get(), proto::AnchovyDescription_Type_LABEL,
                       "bicycle", 0.96f, "0");
  AddAnchovyOutputData(response.get(), proto::AnchovyDescription_Type_OCR,
                       "STOP", 0.91f, "0");

  // Untagged and invalid entries interspersed with valid ones (tag "1" has only
  // invalid outputs, so "1" must remain absent from results_by_tag).
  AddAnchovyOutputData(response.get(),
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION,
                       "Untagged entry", 0.90f, /*tag=*/"");
  AddAnchovyOutputData(response.get(),
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION,
                       "Invalid UTF-8 tag", 0.90f, /*tag=*/"\xC0\xAF");
  auto* no_custom = response->add_output_data();
  no_custom->set_tag("1");
  no_custom->set_text("plain text");
  no_custom->set_score(0.9f);

  auto* malformed = response->add_output_data();
  malformed->set_tag("1");
  malformed->set_score(0.9f);
  malformed->mutable_custom()->set_value("\xFF\xFF\xFF\xFF");

  AddAnchovyOutputData(response.get(), proto::AnchovyDescription_Type_UNKNOWN,
                       "unknown", 0.8f, "1");
  AddAnchovyOutputData(response.get(), proto::AnchovyDescription_Type_UNUSED,
                       "unused", 0.8f, "1");
  AddAnchovyOutputData(response.get(),
                       static_cast<proto::AnchovyDescription::Type>(99),
                       "unrecognized", 0.8f, "1");
  AddAnchovyOutputData(response.get(),
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION, "", 0.8f,
                       "1");
  AddAnchovyOutputData(
      response.get(), proto::AnchovyDescription_Type_PRIMARY_CAPTION,
      "nan score", std::numeric_limits<float>::quiet_NaN(), "1");
  AddAnchovyOutputData(
      response.get(), proto::AnchovyDescription_Type_PRIMARY_CAPTION,
      "inf score", std::numeric_limits<float>::infinity(), "1");

  // Valid entry for dotted tag "img.2" using a server-defined internal type_url
  // distinct from manta.proto.AnchovyDescription, followed by an invalid entry
  // on already-populated tag "0" and a non-adjacent second entry for "img.2".
  AddAnchovyOutputData(
      response.get(), proto::AnchovyDescription_Type_PRIMARY_CAPTION,
      "A lighthouse on a cliff", 0.88f, "img.2",
      "type.googleapis.com/image_repository.api.ImageDescription");
  AddAnchovyOutputData(response.get(),
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION, "", 0.8f,
                       "0");
  AddAnchovyOutputData(
      response.get(), proto::AnchovyDescription_Type_OCR, "LIGHTHOUSE", 0.93f,
      "img.2", "type.googleapis.com/image_repository.api.ImageDescription");

  base::test::TestFuture<base::DictValue, MantaStatus> future;
  anchovy::AnchovyProtoHelper::HandleImageDescriptionsResponse(
      future.GetCallback(), std::move(response), {MantaStatusCode::kOk, "ok"});

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get<1>().status_code, MantaStatusCode::kOk);
  const base::DictValue* results_by_tag =
      future.Get<0>().FindDict(anchovy::kResultsByTagKey);
  ASSERT_NE(results_by_tag, nullptr);
  EXPECT_EQ(results_by_tag->size(), 2u);
  EXPECT_EQ(results_by_tag->FindList("1"), nullptr);
  EXPECT_EQ(results_by_tag->FindDict("img"), nullptr);

  const base::ListValue* tag0 = results_by_tag->FindList("0");
  ASSERT_NE(tag0, nullptr);
  ASSERT_EQ(tag0->size(), 4u);
  EXPECT_EQ(*(*tag0)[0].GetDict().FindString("type"),
            anchovy::kTypePrimaryCaption);
  EXPECT_EQ(*(*tag0)[0].GetDict().FindString("text"),
            "A person riding a bicycle");
  EXPECT_NEAR(*(*tag0)[0].GetDict().FindDouble("score"), 0.84, 1e-5);
  EXPECT_EQ(*(*tag0)[1].GetDict().FindString("type"),
            anchovy::kTypeSecondaryCaption);
  EXPECT_EQ(*(*tag0)[1].GetDict().FindString("text"), "Golden Gate Park");
  EXPECT_NEAR(*(*tag0)[1].GetDict().FindDouble("score"), 1.35, 1e-5);
  EXPECT_EQ(*(*tag0)[2].GetDict().FindString("type"), anchovy::kTypeLabel);
  EXPECT_EQ(*(*tag0)[3].GetDict().FindString("type"), anchovy::kTypeOcr);

  const base::ListValue* tag2 = results_by_tag->FindList("img.2");
  ASSERT_NE(tag2, nullptr);
  ASSERT_EQ(tag2->size(), 2u);
  EXPECT_EQ(*(*tag2)[0].GetDict().FindString("type"),
            anchovy::kTypePrimaryCaption);
  EXPECT_EQ(*(*tag2)[0].GetDict().FindString("text"),
            "A lighthouse on a cliff");
  EXPECT_NEAR(*(*tag2)[0].GetDict().FindDouble("score"), 0.88, 1e-5);
  EXPECT_EQ(*(*tag2)[1].GetDict().FindString("type"), anchovy::kTypeOcr);
  EXPECT_EQ(*(*tag2)[1].GetDict().FindString("text"), "LIGHTHOUSE");
  EXPECT_NEAR(*(*tag2)[1].GetDict().FindDouble("score"), 0.93, 1e-5);
}

// Verifies that HandleImageDescriptionsResponse returns an empty top-level
// DictValue on non-OK status, and returns an empty `kResultsByTagKey`
// dictionary on `kOk` when `output_data` is empty or contains only
// untagged/invalid entries.
TEST(AnchovyProtoHelperTest, HandleImageDescriptionsResponseErrorAndEmpty) {
  base::test::TestFuture<base::DictValue, MantaStatus> error_future;
  anchovy::AnchovyProtoHelper::HandleImageDescriptionsResponse(
      error_future.GetCallback(), nullptr,
      {MantaStatusCode::kBackendFailure, "backend error"});
  ASSERT_TRUE(error_future.Wait());
  EXPECT_TRUE(error_future.Get<0>().empty());
  EXPECT_EQ(error_future.Get<1>().status_code,
            MantaStatusCode::kBackendFailure);

  base::test::TestFuture<base::DictValue, MantaStatus> empty_future;
  anchovy::AnchovyProtoHelper::HandleImageDescriptionsResponse(
      empty_future.GetCallback(), std::make_unique<proto::Response>(),
      {MantaStatusCode::kOk, "ok", "en"});
  ASSERT_TRUE(empty_future.Wait());
  const base::DictValue* empty_results_by_tag =
      empty_future.Get<0>().FindDict(anchovy::kResultsByTagKey);
  ASSERT_NE(empty_results_by_tag, nullptr);
  EXPECT_TRUE(empty_results_by_tag->empty());
  EXPECT_EQ(empty_future.Get<1>().status_code, MantaStatusCode::kOk);
  EXPECT_EQ(empty_future.Get<1>().locale, "en");

  auto untagged_only = std::make_unique<proto::Response>();
  AddAnchovyOutputData(untagged_only.get(),
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION,
                       "Missing tag", 0.9f, /*tag=*/"");
  AddAnchovyOutputData(untagged_only.get(),
                       proto::AnchovyDescription_Type_UNKNOWN, "Unknown type",
                       0.9f, "0");
  base::test::TestFuture<base::DictValue, MantaStatus> invalid_future;
  anchovy::AnchovyProtoHelper::HandleImageDescriptionsResponse(
      invalid_future.GetCallback(), std::move(untagged_only),
      {MantaStatusCode::kOk, "ok"});
  ASSERT_TRUE(invalid_future.Wait());
  const base::DictValue* invalid_results_by_tag =
      invalid_future.Get<0>().FindDict(anchovy::kResultsByTagKey);
  ASSERT_NE(invalid_results_by_tag, nullptr);
  EXPECT_TRUE(invalid_results_by_tag->empty());
  EXPECT_EQ(invalid_future.Get<1>().status_code, MantaStatusCode::kOk);
}

TEST_F(AnchovyProviderTest, GetImageDescriptionsBatchSuccess) {
  proto::Response response;
  AddAnchovyOutputData(&response,
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION,
                       "A cat sitting on a windowsill", 0.92f, "0");
  AddAnchovyOutputData(&response,
                       proto::AnchovyDescription_Type_SECONDARY_CAPTION,
                       "Indoor", 0.80f, "0");
  AddAnchovyOutputData(&response, proto::AnchovyDescription_Type_LABEL, "dog",
                       0.85f, "1");

  base::HistogramTester histogram_tester;
  std::unique_ptr<FakeAnchovyProvider> anchovy_provider =
      CreateAnchovyProvider();

  int http_request_count = 0;
  std::string uploaded_body;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& req) {
        if (req.url != GURL{kMockEndpoint}) {
          return;
        }
        ++http_request_count;
        ASSERT_NE(req.request_body, nullptr);
        ASSERT_EQ(req.request_body->elements()->size(), 1u);
        uploaded_body = std::string(req.request_body->elements()
                                        ->at(0)
                                        .As<network::DataElementBytes>()
                                        .AsStringView());
      }));

  const std::string serialized_response = response.SerializeAsString();
  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/serialized_response, net::HTTP_OK,
                          net::OK);

  const std::vector<uint8_t> kCatBytes = {1, 2, 3};
  const std::vector<uint8_t> kDogBytes = {4, 5, 6, 7};
  anchovy::ImageDescriptionBatchRequest batch("en");
  batch.images.emplace_back("0", kCatBytes);
  batch.images.emplace_back("1", kDogBytes);

  base::test::TestFuture<base::DictValue, MantaStatus> future;
  anchovy_provider->GetImageDescriptions(batch, TRAFFIC_ANNOTATION_FOR_TESTS,
                                         future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_EQ(http_request_count, 1);
  proto::Request uploaded_request;
  ASSERT_TRUE(uploaded_request.ParseFromString(uploaded_body));
  EXPECT_EQ(uploaded_request.feature_name(),
            proto::FeatureName::ACCESSIBILITY_IMAGE_DESCRIPTION);
  ASSERT_EQ(uploaded_request.input_data_size(), 2);
  EXPECT_EQ(uploaded_request.input_data(0).tag(), "0");
  EXPECT_EQ(uploaded_request.input_data(0).image().serialized_bytes(),
            std::string(kCatBytes.begin(), kCatBytes.end()));
  EXPECT_EQ(uploaded_request.input_data(1).tag(), "1");
  EXPECT_EQ(uploaded_request.input_data(1).image().serialized_bytes(),
            std::string(kDogBytes.begin(), kDogBytes.end()));

  EXPECT_EQ(future.Get<1>().status_code, MantaStatusCode::kOk);
  const base::DictValue* results_by_tag =
      future.Get<0>().FindDict(anchovy::kResultsByTagKey);
  ASSERT_NE(results_by_tag, nullptr);
  ASSERT_NE(results_by_tag->FindList("0"), nullptr);
  EXPECT_EQ(results_by_tag->FindList("0")->size(), 2u);
  ASSERT_NE(results_by_tag->FindList("1"), nullptr);
  EXPECT_EQ(results_by_tag->FindList("1")->size(), 1u);

  histogram_tester.ExpectTotalCount("Ash.MantaService.AnchovyProvider.TimeCost",
                                    1);
  histogram_tester.ExpectUniqueSample(
      "Ash.MantaService.AnchovyProvider.RequestSizeBytes", uploaded_body.size(),
      1);
  histogram_tester.ExpectUniqueSample(
      "Ash.MantaService.AnchovyProvider.ResponseSizeBytes",
      serialized_response.size(), 1);
}

TEST_F(AnchovyProviderTest,
       GetImageDescriptionsOversizedBatchReturnsInvalidInput) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<FakeAnchovyProvider> anchovy_provider =
      CreateAnchovyProvider();

  int http_request_count = 0;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& req) {
        if (req.url == GURL{kMockEndpoint}) {
          ++http_request_count;
        }
      }));

  // Batch exceeding `kMaxBatchSize` (`100`) images fails fast with
  // `kInvalidInput` without issuing an HTTP request.
  const std::vector<uint8_t> kValidBytes = {1, 2};
  anchovy::ImageDescriptionBatchRequest oversized_batch("en");
  oversized_batch.images.reserve(anchovy::kMaxBatchSize + 1);
  for (size_t i = 0; i < anchovy::kMaxBatchSize + 1; ++i) {
    oversized_batch.images.emplace_back(base::NumberToString(i), kValidBytes);
  }
  base::test::TestFuture<base::DictValue, MantaStatus> oversized_future;
  anchovy_provider->GetImageDescriptions(oversized_batch,
                                         TRAFFIC_ANNOTATION_FOR_TESTS,
                                         oversized_future.GetCallback());
  ASSERT_TRUE(oversized_future.Wait());
  EXPECT_TRUE(oversized_future.Get<0>().empty());
  EXPECT_EQ(oversized_future.Get<1>().status_code,
            MantaStatusCode::kInvalidInput);

  EXPECT_EQ(http_request_count, 0);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  histogram_tester.ExpectTotalCount("Ash.MantaService.AnchovyProvider.TimeCost",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Ash.MantaService.AnchovyProvider.StatusCode", 0);
  histogram_tester.ExpectTotalCount(
      "Ash.MantaService.AnchovyProvider.RequestSizeBytes", 0);
  histogram_tester.ExpectTotalCount(
      "Ash.MantaService.AnchovyProvider.ResponseSizeBytes", 0);

  // Boundary check: exactly `kMaxBatchSize` (`100`) valid images is accepted
  // and issues a single HTTP request.
  SetEndpointMockResponse(
      GURL{kMockEndpoint},
      /*response_data=*/proto::Response().SerializeAsString(), net::HTTP_OK,
      net::OK);
  anchovy::ImageDescriptionBatchRequest max_batch("en");
  max_batch.images.reserve(anchovy::kMaxBatchSize);
  for (size_t i = 0; i < anchovy::kMaxBatchSize; ++i) {
    max_batch.images.emplace_back(base::NumberToString(i), kValidBytes);
  }
  base::test::TestFuture<base::DictValue, MantaStatus> max_future;
  anchovy_provider->GetImageDescriptions(
      max_batch, TRAFFIC_ANNOTATION_FOR_TESTS, max_future.GetCallback());
  ASSERT_TRUE(max_future.Wait());
  EXPECT_EQ(max_future.Get<1>().status_code, MantaStatusCode::kOk);
  EXPECT_EQ(http_request_count, 1);
}

TEST_F(AnchovyProviderTest, GetImageDescriptionsBackendFailure) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<FakeAnchovyProvider> anchovy_provider =
      CreateAnchovyProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_BAD_REQUEST, net::OK);

  const std::vector<uint8_t> kImg0 = {1, 2, 3};
  const std::vector<uint8_t> kImg1 = {4, 5, 6};
  anchovy::ImageDescriptionBatchRequest batch("en");
  batch.images.emplace_back("0", kImg0);
  batch.images.emplace_back("1", kImg1);

  base::test::TestFuture<base::DictValue, MantaStatus> future;
  anchovy_provider->GetImageDescriptions(batch, TRAFFIC_ANNOTATION_FOR_TESTS,
                                         future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get<0>().empty());
  EXPECT_EQ(future.Get<1>().status_code, MantaStatusCode::kBackendFailure);
  histogram_tester.ExpectUniqueSample(
      "Ash.MantaService.AnchovyProvider.StatusCode",
      MantaStatusCode::kBackendFailure, 1);
  histogram_tester.ExpectTotalCount(
      "Ash.MantaService.AnchovyProvider.RequestSizeBytes", 1);
  // HTTP error responses must not record synthetic error strings as server
  // response sizes.
  histogram_tester.ExpectTotalCount(
      "Ash.MantaService.AnchovyProvider.ResponseSizeBytes", 0);
}

TEST_F(AnchovyProviderTest, GetImageDescriptionsMalformedResponse) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<FakeAnchovyProvider> anchovy_provider =
      CreateAnchovyProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/"\xFF\xFF\xFF\xFF", net::HTTP_OK,
                          net::OK);

  const std::vector<uint8_t> kImg0 = {1, 2, 3};
  anchovy::ImageDescriptionBatchRequest batch("en");
  batch.images.emplace_back("0", kImg0);

  base::test::TestFuture<base::DictValue, MantaStatus> future;
  anchovy_provider->GetImageDescriptions(batch, TRAFFIC_ANNOTATION_FOR_TESTS,
                                         future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get<0>().empty());
  EXPECT_EQ(future.Get<1>().status_code, MantaStatusCode::kMalformedResponse);
  histogram_tester.ExpectUniqueSample(
      "Ash.MantaService.AnchovyProvider.StatusCode",
      MantaStatusCode::kMalformedResponse, 1);
  histogram_tester.ExpectUniqueSample(
      "Ash.MantaService.AnchovyProvider.ResponseSizeBytes", 4, 1);
}

// Exercises BaseProvider::RequestInternal directly in API-key mode (matching
// production AnchovyProvider construction) to verify that serialized request
// payload size includes ClientInfo metadata added during serialization.
TEST_F(AnchovyProviderTest, GetImageDescriptionsApiKeyModeLogsPayloadSizes) {
  base::HistogramTester histogram_tester;
  const ProviderParams provider_params = {/*use_api_key=*/true, "160.0.0.0",
                                          version_info::Channel::STABLE,
                                          "en-US"};
  AnchovyProvider anchovy_provider(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      /*identity_manager=*/nullptr, provider_params);

  proto::Response response;
  AddAnchovyOutputData(&response,
                       proto::AnchovyDescription_Type_PRIMARY_CAPTION,
                       "A lighthouse by the sea", 0.91f, "0");
  const std::string serialized_response = response.SerializeAsString();
  std::string uploaded_body;
  // API-key mode appends `?key=...` to the endpoint URL, so register the mock
  // response for the exact intercepted request URL on arrival.
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& req) {
        SetEndpointMockResponse(req.url, serialized_response, net::HTTP_OK,
                                net::OK);
        EXPECT_NE(req.request_body, nullptr);
        if (req.request_body && req.request_body->elements()->size() == 1u) {
          uploaded_body = std::string(req.request_body->elements()
                                          ->at(0)
                                          .As<network::DataElementBytes>()
                                          .AsStringView());
        } else if (req.request_body) {
          EXPECT_EQ(req.request_body->elements()->size(), 1u);
        }
      }));

  const std::vector<uint8_t> kImgBytes = {10, 20, 30, 40};
  anchovy::ImageDescriptionBatchRequest batch("en");
  batch.images.emplace_back("0", kImgBytes);

  base::test::TestFuture<base::DictValue, MantaStatus> future;
  anchovy_provider.GetImageDescriptions(batch, TRAFFIC_ANNOTATION_FOR_TESTS,
                                        future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get<1>().status_code, MantaStatusCode::kOk);

  proto::Request uploaded_request;
  ASSERT_TRUE(uploaded_request.ParseFromString(uploaded_body));
  EXPECT_TRUE(uploaded_request.has_client_info());
  histogram_tester.ExpectUniqueSample(
      "Ash.MantaService.AnchovyProvider.RequestSizeBytes", uploaded_body.size(),
      1);
  histogram_tester.ExpectUniqueSample(
      "Ash.MantaService.AnchovyProvider.ResponseSizeBytes",
      serialized_response.size(), 1);
}

}  // namespace manta
