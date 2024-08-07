// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/image_service_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_optimization_guide_decider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/salient_image_metadata.pb.h"
#include "components/page_image_service/features.h"
#include "components/page_image_service/metrics_util.h"
#include "components/page_image_service/mojom/page_image_service.mojom-shared.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace optimization_guide {
namespace {

class ImageServiceTestOptGuide : public TestOptimizationGuideDecider {
 public:
  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<proto::OptimizationType>& optimization_types,
      proto::RequestContext request_context,
      OnDemandOptimizationGuideDecisionRepeatingCallback callback,
      std::optional<proto::RequestContextMetadata> request_context_metadata =
          std::nullopt) override {
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

namespace page_image_service {

class ImageServiceImplTest : public testing::Test {
 public:
  ImageServiceImplTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {kImageService, kImageServiceSuggestPoweredImages,
         kImageServiceOptimizationGuideSalientImages},
        {});

    remote_suggestions_service_ = std::make_unique<RemoteSuggestionsService>(
        /*document_suggestions_service=*/nullptr,
        test_url_loader_factory_.GetSafeWeakWrapper());
    test_opt_guide_ =
        std::make_unique<optimization_guide::ImageServiceTestOptGuide>();
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    image_service_ = std::make_unique<ImageServiceImpl>(
        search_engines_test_environment_.template_url_service(),
        remote_suggestions_service_.get(), test_opt_guide_.get(),
        test_sync_service_.get(), std::make_unique<TestSchemeClassifier>());
  }

  PageImageServiceConsentStatus GetConsentStatusToFetchImageAwaitResult(
      mojom::ClientId client_id) {
    PageImageServiceConsentStatus out_status;
    base::RunLoop loop;
    image_service_->GetConsentToFetchImage(
        client_id,
        base::BindLambdaForTesting([&](PageImageServiceConsentStatus status) {
          out_status = status;
          loop.Quit();
        }));
    loop.Run();
    return out_status;
  }

  ImageServiceImplTest(const ImageServiceImplTest&) = delete;
  ImageServiceImplTest& operator=(const ImageServiceImplTest&) = delete;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  network::TestURLLoaderFactory test_url_loader_factory_;

  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<RemoteSuggestionsService> remote_suggestions_service_;
  std::unique_ptr<optimization_guide::ImageServiceTestOptGuide> test_opt_guide_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<ImageServiceImpl> image_service_;

  base::HistogramTester histogram_tester_;
};

// Helper method that stores `image_url` into `out_image_url`.
void StoreImageUrlResponse(GURL* out_image_url, const GURL& image_url) {
  DCHECK(out_image_url);
  *out_image_url = image_url;
}

// Stores an image response and exits out of `loop` if it is defined.
void QuitLoopAndStoreImageUrlResponse(base::RunLoop* loop,
                                      GURL* out_image_url,
                                      const GURL& image_url) {
  DCHECK(out_image_url);
  *out_image_url = image_url;
  loop->Quit();
}

void AppendResponse(std::vector<GURL>* responses, const GURL& image_url) {
  DCHECK(responses);
  responses->push_back(image_url);
}

TEST_F(ImageServiceImplTest, DoesNotRegisterForNavigationRelatedMetadata) {
  ASSERT_EQ(test_opt_guide_->registered_optimization_types().size(), 0U);
}

TEST_F(ImageServiceImplTest, GetConsentToFetchImage) {
  test_sync_service_->SetDownloadStatusFor(
      {syncer::DataType::BOOKMARKS,
       syncer::DataType::HISTORY_DELETE_DIRECTIVES},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  test_sync_service_->FireStateChanged();

  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::Journeys),
            PageImageServiceConsentStatus::kTimedOut);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(
                mojom::ClientId::JourneysSidePanel),
            PageImageServiceConsentStatus::kTimedOut);
  EXPECT_EQ(
      GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::NtpRealbox),
      PageImageServiceConsentStatus::kFailure);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::NtpQuests),
            PageImageServiceConsentStatus::kTimedOut);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::Bookmarks),
            PageImageServiceConsentStatus::kTimedOut);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(
                mojom::ClientId::NtpTabResumption),
            PageImageServiceConsentStatus::kTimedOut);

  test_sync_service_->SetDownloadStatusFor(
      {syncer::DataType::HISTORY_DELETE_DIRECTIVES},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service_->FireStateChanged();

  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::Journeys),
            PageImageServiceConsentStatus::kSuccess);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(
                mojom::ClientId::JourneysSidePanel),
            PageImageServiceConsentStatus::kSuccess);
  // NTP Realbox still false as it does not have an approved privacy model yet.
  EXPECT_EQ(
      GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::NtpRealbox),
      PageImageServiceConsentStatus::kFailure);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::NtpQuests),
            PageImageServiceConsentStatus::kSuccess);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(mojom::ClientId::Bookmarks),
            PageImageServiceConsentStatus::kTimedOut);
  EXPECT_EQ(GetConsentStatusToFetchImageAwaitResult(
                mojom::ClientId::NtpTabResumption),
            PageImageServiceConsentStatus::kSuccess);
}

TEST_F(ImageServiceImplTest, SyncInitialization) {
  // Put Sync into the initializing state.
  test_sync_service_->SetDownloadStatusFor(
      {syncer::DataType::BOOKMARKS,
       syncer::DataType::HISTORY_DELETE_DIRECTIVES},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
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
  test_sync_service_->SetDownloadStatusFor(
      {syncer::DataType::BOOKMARKS,
       syncer::DataType::HISTORY_DELETE_DIRECTIVES},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service_->FireStateChanged();
  task_environment.FastForwardBy(kOptimizationGuideBatchingTimeout);
  EXPECT_EQ(test_opt_guide_->requests_received_, 1U)
      << "The test backend should immediately get the request after Sync "
         "activates, and the consent throttle unthrottles, and after the "
         "short aggregation timeout expires.";

  // This test only covers sync unthrottling, so we don't care about fulfilling
  // the actual request. That's covered by
  // OptimizationGuideSalientImagesEndToEnd.
}

TEST_F(ImageServiceImplTest, SuggestBackendEndToEnd) {
  mojom::Options options;
  options.suggest_images = true;
  options.optimization_guide_images = true;

  base::RunLoop loop;

  GURL response;
  image_service_->FetchImageFor(
      mojom::ClientId::Journeys,
      GURL("https://www.google.com/search?q=santa+monica"), options,
      base::BindOnce(&QuitLoopAndStoreImageUrlResponse, &loop, &response));

  // Test histograms with literal names to validate client-sliced names.
  // This also validates that the correct backend was selected.
  EXPECT_EQ(histogram_tester_.GetBucketCount("PageImageService.Backend",
                                             PageImageServiceBackend::kSuggest),
            1);
  EXPECT_EQ(
      histogram_tester_.GetBucketCount("PageImageService.Backend.Journeys",
                                       PageImageServiceBackend::kSuggest),
      1);

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  GURL request_url = test_url_loader_factory_.GetPendingRequest(0)->request.url;
  EXPECT_EQ(request_url.host(), "www.google.com");

  test_url_loader_factory_.AddResponse(request_url.spec(), R"([
  "santa monica",
  [
    "santa monica"
  ],
  [
    ""
  ],
  [],
  {
    "google:clientdata": {
      "bpc": false,
      "tlw": false
    },
    "google:suggestdetail": [
      {
        "google:entityinfo": "CggvbS8wNl9raBISQ2l0eSBpbiBDYWxpZm9ybmlhMnRodHRwczovL2VuY3J5cHRlZC10Ym4wLmdzdGF0aWMuY29tL2ltYWdlcz9xPXRibjpBTmQ5R2NTd3ZOaHc3cktRV2dqRG9vUC1zY1ptRHlTSlNJWWpCT1gwVkVDRDU1czM4dHA0eEZORWcwTTdQdUEmcz0xMDoMU2FudGEgTW9uaWNhSgcjNDI0MjQyUjVnc19zc3A9ZUp6ajR0RFAxVGN3aThfT01HRDA0aWxPekN0SlZNak56OHRNVGdRQVdDY0hxd3AMcBo="
      }
    ],
    "google:suggestrelevance": [
      1300
    ],
    "google:suggestsubtypes": [
      [
        131,
        433,
        512
      ]
    ],
    "google:suggesttype": [
      "ENTITY"
    ],
    "google:verbatimrelevance": 1300
  }
])");

  // Successfully fetching the image quits this loop.
  loop.Run();
  // This expected value matches the hardcoded proto above.
  EXPECT_EQ(response, GURL("https://encrypted-tbn0.gstatic.com/"
                           "images?q=tbn:ANd9GcSwvNhw7rKQWgjDooP-"
                           "scZmDySJSIYjBOX0VECD55s38tp4xFNEg0M7PuA&s=10"));

  // Test histograms with literal names to validate client-sliced names.
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "PageImageService.Backend.Suggest.Result",
                PageImageServiceResult::kSuccess),
            1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "PageImageService.Backend.Suggest.Result.Journeys",
                PageImageServiceResult::kSuccess),
            1);
}

// This also tests batching, because it's an integral part of how Optimization
// Guide backend works.
TEST_F(ImageServiceImplTest, OptimizationGuideSalientImagesEndToEnd) {
  mojom::Options options;
  options.suggest_images = false;
  options.optimization_guide_images = true;

  GURL response_1;
  GURL response_2;
  GURL response_3;
  image_service_->FetchImageFor(
      mojom::ClientId::Journeys, GURL("https://1.com"), options,
      base::BindOnce(&StoreImageUrlResponse, &response_1));
  image_service_->FetchImageFor(
      mojom::ClientId::Journeys, GURL("https://2.com"), options,
      base::BindOnce(&StoreImageUrlResponse, &response_2));
  image_service_->FetchImageFor(
      mojom::ClientId::Journeys, GURL("https://1.com"), options,
      base::BindOnce(&StoreImageUrlResponse, &response_3));
  image_service_->FetchImageFor(
      mojom::ClientId::Journeys, GURL("https://httpimage.com"), options,
      base::BindOnce(&StoreImageUrlResponse, &response_3));
  task_environment.FastForwardBy(kOptimizationGuideBatchingTimeout);

  // Verify that the OptimizationGuide backend got one appropriate call.
  ASSERT_EQ(test_opt_guide_->requests_received_, 1U);
  EXPECT_THAT(
      test_opt_guide_->on_demand_call_urls_,
      ElementsAre(GURL("https://1.com"), GURL("https://2.com"),
                  GURL("https://1.com"), GURL("https://httpimage.com")));
  EXPECT_THAT(test_opt_guide_->on_demand_call_optimization_types_,
              ElementsAre(optimization_guide::proto::SALIENT_IMAGE));
  EXPECT_EQ(test_opt_guide_->on_demand_call_request_context_,
            optimization_guide::proto::CONTEXT_JOURNEYS);

  // Test histograms with literal names to validate client-sliced names.
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "PageImageService.Backend",
                PageImageServiceBackend::kOptimizationGuide),
            4);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                "PageImageService.Backend.Journeys",
                PageImageServiceBackend::kOptimizationGuide),
            4);

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

  // Verify the decision can be parsed and sent back to the original caller.
  optimization_guide::OptimizationGuideDecisionWithMetadata http_decision;
  {
    http_decision.decision =
        optimization_guide::OptimizationGuideDecision::kTrue;

    optimization_guide::proto::SalientImageMetadata salient_image_metadata;
    auto* thumbnail = salient_image_metadata.add_thumbnails();
    thumbnail->set_image_url("http://image-url.com/foo.png");

    optimization_guide::proto::Any any;
    any.set_type_url(salient_image_metadata.GetTypeName());
    salient_image_metadata.SerializeToString(any.mutable_value());
    http_decision.metadata.set_any_metadata(any);
  }

  // Verify that the repeating callback can be called twice with the two
  // different URLs, the "https://1.com" one being deduplicated.
  test_opt_guide_->on_demand_call_callback_.Run(
      GURL("https://2.com"),
      {{optimization_guide::proto::SALIENT_IMAGE, decision}});
  EXPECT_EQ(response_1, GURL());
  EXPECT_EQ(response_2, GURL("https://image-url.com/foo.png"));
  EXPECT_EQ(response_3, GURL());
  test_opt_guide_->on_demand_call_callback_.Run(
      GURL("https://1.com"),
      {{optimization_guide::proto::SALIENT_IMAGE, decision}});
  EXPECT_EQ(response_1, GURL("https://image-url.com/foo.png"));
  EXPECT_EQ(response_2, GURL("https://image-url.com/foo.png"));
  EXPECT_EQ(response_3, GURL("https://image-url.com/foo.png"));
  test_opt_guide_->on_demand_call_callback_.Run(
      GURL("https://httpimage.com"),
      {{optimization_guide::proto::SALIENT_IMAGE, http_decision}});

  // Test histograms with literal names to validate client-sliced names.
  histogram_tester_.ExpectBucketCount(
      "PageImageService.Backend.OptimizationGuide.Result",
      PageImageServiceResult::kSuccess, 2);
  histogram_tester_.ExpectBucketCount(
      "PageImageService.Backend.OptimizationGuide.Result.Journeys",
      PageImageServiceResult::kSuccess, 2);
  histogram_tester_.ExpectBucketCount(
      "PageImageService.Backend.OptimizationGuide.Result",
      PageImageServiceResult::kResponseMalformed, 1);
  histogram_tester_.ExpectBucketCount(
      "PageImageService.Backend.OptimizationGuide.Result.Journeys",
      PageImageServiceResult::kResponseMalformed, 1);
}

TEST_F(ImageServiceImplTest, OptimizationGuideBatchingRespectsMaxUrls) {
  mojom::Options options;
  options.suggest_images = false;
  options.optimization_guide_images = true;

  std::vector<GURL> responses;

  size_t max_batch = optimization_guide::features::
      MaxUrlsForOptimizationGuideServiceHintsFetch();
  // Fetch one LESS than the max batch size, to verify no requests are sent.
  for (size_t i = 0; i < max_batch - 1; ++i) {
    image_service_->FetchImageFor(
        mojom::ClientId::Journeys,
        GURL("https://" + base::NumberToString(i) + ".com"), options,
        base::BindOnce(&AppendResponse, &responses));
    EXPECT_EQ(test_opt_guide_->requests_received_, 0U) << "i = " << i;
  }

  image_service_->FetchImageFor(mojom::ClientId::Journeys,
                                GURL("https://last.com"), options,
                                base::BindOnce(&AppendResponse, &responses));
  EXPECT_EQ(test_opt_guide_->requests_received_, 1U);
  ASSERT_EQ(test_opt_guide_->on_demand_call_urls_.size(), max_batch);
  EXPECT_EQ(test_opt_guide_->on_demand_call_urls_[0], GURL("https://0.com/"));
  EXPECT_EQ(test_opt_guide_->on_demand_call_urls_[max_batch - 1],
            GURL("https://last.com"));

  image_service_->FetchImageFor(mojom::ClientId::Journeys,
                                GURL("https://one_more.com"), options,
                                base::BindOnce(&AppendResponse, &responses));
  EXPECT_EQ(test_opt_guide_->requests_received_, 1U)
      << "Expect that making more request restarts the queue.";
}

class DisabledOptGuideImageServiceImplTest : public ImageServiceImplTest {
 public:
  DisabledOptGuideImageServiceImplTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kImageService, kImageServiceSuggestPoweredImages},
        /*disabled_features=*/{kImageServiceOptimizationGuideSalientImages});

    remote_suggestions_service_ = std::make_unique<RemoteSuggestionsService>(
        /*document_suggestions_service=*/nullptr,
        test_url_loader_factory_.GetSafeWeakWrapper());
    test_opt_guide_ =
        std::make_unique<optimization_guide::ImageServiceTestOptGuide>();
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    image_service_ = std::make_unique<ImageServiceImpl>(
        search_engines_test_environment_.template_url_service(),
        remote_suggestions_service_.get(), test_opt_guide_.get(),
        test_sync_service_.get(), std::make_unique<TestSchemeClassifier>());
  }
};

TEST_F(DisabledOptGuideImageServiceImplTest, DoesNotFetch) {
  mojom::Options options;
  options.suggest_images = false;
  options.optimization_guide_images = true;

  GURL image_url_response;
  image_service_->FetchImageFor(
      mojom::ClientId::Journeys, GURL("https://page-url.com"), options,
      base::BindOnce(&StoreImageUrlResponse, &image_url_response));

  // Verify that the OptimizationGuide backend did not get called.
  EXPECT_EQ(test_opt_guide_->requests_received_, 0U);
  EXPECT_EQ(image_url_response, GURL());
}

}  // namespace page_image_service
