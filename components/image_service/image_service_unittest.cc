// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_service/image_service.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/image_service/features.h"
#include "components/image_service/metrics_util.h"
#include "components/image_service/mojom/image_service.mojom-shared.h"
#include "components/image_service/mojom/image_service.mojom.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/test_new_optimization_guide_decider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/salient_image_metadata.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace optimization_guide {
namespace {

class ImageServiceTestOptGuide : public TestNewOptimizationGuideDecider {
 public:
  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<proto::OptimizationType>& optimization_types,
      proto::RequestContext request_context,
      OnDemandOptimizationGuideDecisionRepeatingCallback callback) override {
    requests_received_++;

    // For this test, we just want to store the parameters which were used in
    // the call, and the test will manually send a response to `callback`.
    on_demand_call_urls_ = urls;
    on_demand_call_optimization_types_ = optimization_types;
    on_demand_call_request_context_ = request_context;
    on_demand_call_callback_ = std::move(callback);
  }

  size_t requests_received_ = 0;

  std::vector<GURL> on_demand_call_urls_;
  base::flat_set<proto::OptimizationType> on_demand_call_optimization_types_;
  proto::RequestContext on_demand_call_request_context_;
  OnDemandOptimizationGuideDecisionRepeatingCallback on_demand_call_callback_;
};

}  // namespace
}  // namespace optimization_guide

namespace image_service {

class ImageServiceTest : public testing::Test {
 public:
  ImageServiceTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {kImageService, kImageServiceSuggestPoweredImages,
         kImageServiceOptimizationGuideSalientImages},
        {});

    test_opt_guide_ =
        std::make_unique<optimization_guide::ImageServiceTestOptGuide>();
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    image_service_ = std::make_unique<ImageService>(
        nullptr, test_opt_guide_.get(), test_sync_service_.get());
  }

  bool GetConsentToFetchImageAwaitResult(mojom::ClientId client_id) {
    bool out_consent = false;
    base::RunLoop loop;
    image_service_->GetConsentToFetchImage(
        client_id, base::BindLambdaForTesting([&](bool result) {
          out_consent = result;
          loop.Quit();
        }));
    loop.Run();
    return out_consent;
  }

  ImageServiceTest(const ImageServiceTest&) = delete;
  ImageServiceTest& operator=(const ImageServiceTest&) = delete;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<optimization_guide::ImageServiceTestOptGuide> test_opt_guide_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<ImageService> image_service_;

  base::HistogramTester histogram_tester_;
};

// Helper method that stores `image_url` into `out_image_url`.
void StoreImageUrlResponse(GURL* out_image_url, const GURL& image_url) {
  DCHECK(out_image_url);
  *out_image_url = image_url;
}

void AppendResponse(std::vector<GURL>* responses, const GURL& image_url) {
  DCHECK(responses);
  responses->push_back(image_url);
}

TEST_F(ImageServiceTest, DoesNotRegisterForNavigationRelatedMetadata) {
  ASSERT_EQ(test_opt_guide_->registered_optimization_types().size(), 0U);
}

TEST_F(ImageServiceTest, GetConsentToFetchImage) {
  test_sync_service_->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  test_sync_service_->FireStateChanged();

  EXPECT_FALSE(GetConsentToFetchImageAwaitResult(mojom::ClientId::Journeys));
  EXPECT_FALSE(
      GetConsentToFetchImageAwaitResult(mojom::ClientId::JourneysSidePanel));
  EXPECT_FALSE(GetConsentToFetchImageAwaitResult(mojom::ClientId::NtpRealbox));
  EXPECT_FALSE(GetConsentToFetchImageAwaitResult(mojom::ClientId::NtpQuests));
  EXPECT_FALSE(GetConsentToFetchImageAwaitResult(mojom::ClientId::Bookmarks));

  test_sync_service_->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet(
          syncer::UserSelectableType::kHistory));
  test_sync_service_->FireStateChanged();

  EXPECT_TRUE(GetConsentToFetchImageAwaitResult(mojom::ClientId::Journeys));
  EXPECT_TRUE(
      GetConsentToFetchImageAwaitResult(mojom::ClientId::JourneysSidePanel));
  EXPECT_FALSE(GetConsentToFetchImageAwaitResult(mojom::ClientId::NtpRealbox));
  EXPECT_TRUE(GetConsentToFetchImageAwaitResult(mojom::ClientId::NtpQuests));
  EXPECT_FALSE(GetConsentToFetchImageAwaitResult(mojom::ClientId::Bookmarks));
}

TEST_F(ImageServiceTest, SyncInitialization) {
  // Put Sync into the initializing state.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  test_sync_service_->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet(
          syncer::UserSelectableType::kHistory));
  test_sync_service_->FireStateChanged();

  mojom::Options options;
  options.suggest_images = false;
  options.optimization_guide_images = true;

  std::vector<GURL> responses;
  image_service_->FetchImageFor(mojom::ClientId::Journeys,
                                GURL("https://page-url.com"), options,
                                base::BindOnce(&AppendResponse, &responses));
  EXPECT_EQ(test_opt_guide_->requests_received_, 0U)
      << "Expect no immediate requests, because the consent should be "
         "throttling it.";
  EXPECT_TRUE(responses.empty());

  task_environment.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(test_opt_guide_->requests_received_, 0U)
      << "After 10 seconds, the throttle should have killed the request, never "
         "passing it to the backend.";
  ASSERT_EQ(responses.size(), 1U);
  EXPECT_EQ(responses[0], GURL());

  // Now send another request.
  image_service_->FetchImageFor(mojom::ClientId::Journeys,
                                GURL("https://page-url.com"), options,
                                base::BindOnce(&AppendResponse, &responses));
  task_environment.FastForwardBy(base::Seconds(3));
  EXPECT_EQ(test_opt_guide_->requests_received_, 0U) << "Still throttled.";

  // Now set the test sync service to active.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();
  EXPECT_EQ(test_opt_guide_->requests_received_, 1U)
      << "The test backend should immediately get the request after Sync "
         "activates, and the consent throttle unthrottles.";

  // This test only covers sync unthrottling, so we don't care about fulfilling
  // the actual request. That's covered by
  // OptimizationGuideSalientImagesEndToEnd.
}

TEST_F(ImageServiceTest, OptimizationGuideSalientImagesEndToEnd) {
  mojom::Options options;
  options.suggest_images = false;
  options.optimization_guide_images = true;

  GURL image_url_response;
  image_service_->FetchImageFor(
      mojom::ClientId::Journeys, GURL("https://page-url.com"), options,
      base::BindOnce(&StoreImageUrlResponse, &image_url_response));

  // Verify that the OptimizationGuide backend got one appropriate call.
  EXPECT_THAT(test_opt_guide_->on_demand_call_urls_,
              ElementsAre(GURL("https://page-url.com")));
  EXPECT_THAT(test_opt_guide_->on_demand_call_optimization_types_,
              ElementsAre(optimization_guide::proto::SALIENT_IMAGE));
  EXPECT_EQ(test_opt_guide_->on_demand_call_request_context_,
            optimization_guide::proto::CONTEXT_JOURNEYS);

  // Test histograms with literal names to validate client-sliced names.
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "PageImageService.Backend",
                PageImageServiceBackend::kOptimizationGuide),
            1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "PageImageService.Backend.Journeys",
                PageImageServiceBackend::kOptimizationGuide),
            1);

  // Verify the decision can be parsed and sent back to the original caller.
  optimization_guide::OptimizationGuideDecisionWithMetadata decision;
  {
    decision.decision = optimization_guide::OptimizationGuideDecision::kTrue;

    optimization_guide::proto::SalientImageMetadata salient_image_metadata;
    auto* thumbnail = salient_image_metadata.add_thumbnails();
    thumbnail->set_image_url("https://image-url.com/foo.png");

    optimization_guide::proto::Any any;
    any.set_type_url(salient_image_metadata.GetTypeName());
    salient_image_metadata.SerializeToString(any.mutable_value());
    decision.metadata.set_any_metadata(any);
  }

  test_opt_guide_->on_demand_call_callback_.Run(
      GURL("https://page-url.com"),
      {{optimization_guide::proto::SALIENT_IMAGE, decision}});
  EXPECT_EQ(image_url_response, GURL("https://image-url.com/foo.png"));

  // Test histograms with literal names to validate client-sliced names.
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "PageImageService.Backend.OptimizationGuide.Result",
                PageImageServiceOptimizationGuideResult::kSuccess),
            1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "PageImageService.Backend.OptimizationGuide.Result.Journeys",
                PageImageServiceOptimizationGuideResult::kSuccess),
            1);
}

}  // namespace image_service
