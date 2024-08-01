// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lens_overlay_query_controller.h"

#include "base/base64url.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_features.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_visual_search_interaction_data.pb.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "url/gurl.h"

namespace lens {

// The fake multimodal query text.
constexpr char kTestQueryText[] = "query_text";

// The fake suggest signals.
constexpr char kTestSuggestSignals[] = "suggest_signals";

// The fake server session id.
constexpr char kTestServerSessionId[] = "server_session_id";

// The fake api key to use for fetching requests.
constexpr char kTestApiKey[] = "test_api_key";

// The locale to use.
constexpr char kLocale[] = "en-US";

// The fake page information.
constexpr char kTestPageUrl[] = "https://www.google.com";
constexpr char kTestPageTitle[] = "Page Title";

// The url parameter key for the search context.
constexpr char kSearchContextParamKey[] = "mactx";

// The timestamp param.
constexpr char kStartTimeQueryParam[] = "qsubts";

// The visual search interaction log data param.
constexpr char kVisualSearchInteractionDataQueryParameterKey[] = "vsint";

// Query parameter for the request id.
inline constexpr char kRequestIdParameterKey[] = "vsrid";

// The encoded search context for the test page and title.
constexpr char kTestEncodedSearchContext[] =
    "ChdodHRwczovL3d3dy5nb29nbGUuY29tLxIKUGFnZSBUaXRsZQ";

// The region.
constexpr char kRegion[] = "US";

// The time zone.
constexpr char kTimeZone[] = "America/Los_Angeles";

// The parameter key for gen204 request.
constexpr char kGen204IdentifierQueryParameter[] = "plla";

class FakeEndpointFetcher : public EndpointFetcher {
 public:
  explicit FakeEndpointFetcher(EndpointResponse response)
      : EndpointFetcher(
            net::DefineNetworkTrafficAnnotation("lens_overlay_mock_fetcher",
                                                R"()")),
        response_(response) {}

  void PerformRequest(EndpointFetcherCallback endpoint_fetcher_callback,
                      const char* key) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(endpoint_fetcher_callback),
                       std::make_unique<EndpointResponse>(response_)));
  }

 private:
  EndpointResponse response_;
};

class LensOverlayQueryControllerMock : public LensOverlayQueryController {
 public:
  explicit LensOverlayQueryControllerMock(
      LensOverlayFullImageResponseCallback full_image_callback,
      LensOverlayUrlResponseCallback url_callback,
      LensOverlayInteractionResponseCallback interaction_data_callback,
      LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode)
      : LensOverlayQueryController(full_image_callback,
                                   url_callback,
                                   interaction_data_callback,
                                   thumbnail_created_callback,
                                   variations_client,
                                   identity_manager,
                                   profile,
                                   invocation_source,
                                   use_dark_mode) {}
  ~LensOverlayQueryControllerMock() override = default;

  lens::LensOverlayObjectsResponse fake_objects_response_;
  lens::LensOverlayInteractionResponse fake_interaction_response_;
  lens::LensOverlayClientLogs sent_client_logs_;
  lens::LensOverlayRequestId sent_request_id_;
  lens::LensOverlayObjectsRequest sent_objects_request_;
  lens::LensOverlayInteractionRequest sent_interaction_request_;
  int num_gen204_pings_sent_ = 0;

 protected:
  void CreateAndFetchEndpointFetcher(
      lens::LensOverlayServerRequest request_data,
      base::OnceCallback<void(std::unique_ptr<EndpointFetcher>)>
          fetcher_created_callback,
      EndpointFetcherCallback endpoint_fetcher_callback) override {
    lens::LensOverlayServerResponse fake_server_response;
    if (request_data.has_objects_request()) {
      sent_objects_request_.CopyFrom(request_data.objects_request());
      fake_server_response.mutable_objects_response()->CopyFrom(
          fake_objects_response_);
      sent_request_id_.CopyFrom(
          request_data.objects_request().request_context().request_id());
    } else if (request_data.has_interaction_request()) {
      sent_interaction_request_.CopyFrom(request_data.interaction_request());
      fake_server_response.mutable_interaction_response()->CopyFrom(
          fake_interaction_response_);
      sent_request_id_.CopyFrom(
          request_data.interaction_request().request_context().request_id());
    } else {
      NOTREACHED_IN_MIGRATION();
    }
    sent_client_logs_.CopyFrom(request_data.client_logs());

    EndpointResponse fake_endpoint_response;
    fake_endpoint_response.response = fake_server_response.SerializeAsString();
    fake_endpoint_response.http_status_code =
        google_apis::ApiErrorCode::HTTP_SUCCESS;
    std::unique_ptr<FakeEndpointFetcher> endpoint_fetcher =
        std::make_unique<FakeEndpointFetcher>(fake_endpoint_response);
    EndpointFetcher* fetcher = endpoint_fetcher.get();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(fetcher_created_callback),
                                  std::move(endpoint_fetcher)));
    fetcher->PerformRequest(std::move(endpoint_fetcher_callback), kTestApiKey);
  }

  void SendLatencyGen204IfEnabled(int64_t latency_ms) override {
    num_gen204_pings_sent_++;
  }
};

class LensOverlayQueryControllerTest : public testing::Test {
 public:
  LensOverlayQueryControllerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  const SkBitmap CreateNonEmptyBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(SK_ColorGREEN);
    return bitmap;
  }

  std::string GetExpectedJpegBytesForBitmap(const SkBitmap& bitmap) {
    std::vector<unsigned char> data;
    gfx::JPEGCodec::Encode(
        bitmap, lens::features::GetLensOverlayImageCompressionQuality(), &data);
    return std::string(data.begin(), data.end());
  }

  lens::LensOverlaySelectionType GetSelectionTypeFromUrl(
      std::string url_string) {
    GURL url = GURL(url_string);
    std::string vsint_param;
    EXPECT_TRUE(net::GetValueForKeyInQuery(
        url, kVisualSearchInteractionDataQueryParameterKey, &vsint_param));
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        vsint_param, base::Base64UrlDecodePolicy::DISALLOW_PADDING,
        &serialized_proto));
    lens::LensOverlayVisualSearchInteractionData proto;
    EXPECT_TRUE(proto.ParseFromString(serialized_proto));
    return proto.log_data().user_selection_data().selection_type();
  }

  std::string GetAnalyticsIdFromUrl(std::string url_string) {
    GURL url = GURL(url_string);
    std::string vsrid_param;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(url, kRequestIdParameterKey, &vsrid_param));
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        vsrid_param, base::Base64UrlDecodePolicy::DISALLOW_PADDING,
        &serialized_proto));
    lens::LensOverlayRequestId proto;
    EXPECT_TRUE(proto.ParseFromString(serialized_proto));
    return proto.analytics_id();
  }

  void CheckGen204IdsMatch(
      const lens::LensOverlayClientLogs& client_logs,
      const lens::proto::LensOverlayUrlResponse& url_response) {
    std::string url_gen204_id;
    bool has_gen204_id = net::GetValueForKeyInQuery(
        GURL(url_response.url()), kGen204IdentifierQueryParameter,
        &url_gen204_id);
    ASSERT_TRUE(has_gen204_id);
    ASSERT_TRUE(client_logs.has_paella_id());
    ASSERT_EQ(base::NumberToString(client_logs.paella_id()).c_str(),
              url_gen204_id);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  TestingProfile* profile() { return profile_.get(); }

 private:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
    g_browser_process->SetApplicationLocale(kLocale);
    icu::TimeZone::adoptDefault(
        icu::TimeZone::createTimeZone(icu::UnicodeString(kTimeZone)));
    UErrorCode error_code = U_ZERO_ERROR;
    icu::Locale::setDefault(icu::Locale(kLocale), error_code);
    ASSERT_TRUE(U_SUCCESS(error_code));
  }
};

TEST_F(LensOverlayQueryControllerTest, FetchInitialQuery_ReturnsResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(), base::NullCallback(),
      base::NullCallback(), base::NullCallback(),
      profile()->GetVariationsClient(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), 0);

  task_environment_.RunUntilIdle();
  query_controller.EndQuery();
  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_EQ(query_controller.sent_objects_request_.request_context()
                .request_id()
                .sequence_id(),
            1);
  ASSERT_EQ(query_controller.sent_objects_request_.image_data()
                .image_metadata()
                .width(),
            100);
  ASSERT_EQ(query_controller.sent_objects_request_.image_data()
                .image_metadata()
                .height(),
            100);
  ASSERT_EQ(query_controller.sent_objects_request_.request_context()
                .client_context()
                .locale_context()
                .language(),
            kLocale);
  ASSERT_EQ(query_controller.sent_objects_request_.request_context()
                .client_context()
                .locale_context()
                .region(),
            kRegion);
  ASSERT_EQ(query_controller.sent_objects_request_.request_context()
                .client_context()
                .locale_context()
                .time_zone(),
            kTimeZone);
  ASSERT_EQ(query_controller.num_gen204_pings_sent_, 0);
  ASSERT_EQ(query_controller.sent_client_logs_.lens_overlay_entry_point(),
            lens::LensOverlayClientLogs::APP_MENU);
  ASSERT_TRUE(query_controller.sent_client_logs_.has_paella_id());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchRegionSearchInteraction_ReturnsResponses) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlayInteractionResponse>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      profile()->GetVariationsClient(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false);
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), 0);
  task_environment_.RunUntilIdle();

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(std::move(region), lens::REGION_SEARCH,
                                    additional_search_query_params,
                                    std::nullopt);
  task_environment_.RunUntilIdle();
  query_controller.EndQuery();

  std::string actual_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &actual_start_time);

  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_EQ(query_controller.sent_objects_request_.image_data()
                .image_metadata()
                .width(),
            100);
  ASSERT_EQ(query_controller.sent_objects_request_.image_data()
                .image_metadata()
                .height(),
            100);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(GetSelectionTypeFromUrl(url_response_future.Get().url()),
            lens::REGION_SEARCH);
  ASSERT_EQ(interaction_data_response_future.Get().suggest_signals(),
            kTestSuggestSignals);
  ASSERT_EQ(query_controller.sent_objects_request_.request_context()
                .request_id()
                .sequence_id(),
            1);
  ASSERT_EQ(query_controller.sent_interaction_request_.request_context()
                .request_id()
                .sequence_id(),
            2);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .type(),
      lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .selection_metadata()
          .region()
          .region()
          .center_x(),
      30);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .selection_metadata()
          .region()
          .region()
          .center_y(),
      40);
  ASSERT_EQ(query_controller.sent_interaction_request_.image_crop()
                .zoomed_crop()
                .crop()
                .center_x(),
            30);
  ASSERT_EQ(query_controller.sent_interaction_request_.image_crop()
                .zoomed_crop()
                .crop()
                .center_y(),
            40);
  ASSERT_FALSE(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .has_query_metadata());
  ASSERT_TRUE(has_start_time);
  ASSERT_EQ(query_controller.num_gen204_pings_sent_, 1);
  CheckGen204IdsMatch(query_controller.sent_client_logs_,
                      url_response_future.Get());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchRegionSearchInteractionWithBytes_ReturnsResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlayInteractionResponse>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      profile()->GetVariationsClient(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false);
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap viewport_bitmap = CreateNonEmptyBitmap(1000, 1000);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      viewport_bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), 0);
  task_environment_.RunUntilIdle();

  SkBitmap region_bitmap = CreateNonEmptyBitmap(100, 100);
  region_bitmap.setAlphaType(kOpaque_SkAlphaType);
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(50, 50, 100, 100);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(
      std::move(region), lens::REGION_SEARCH, additional_search_query_params,
      std::make_optional<SkBitmap>(region_bitmap));
  task_environment_.RunUntilIdle();
  query_controller.EndQuery();

  std::string actual_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &actual_start_time);

  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_EQ(query_controller.sent_objects_request_.image_data()
                .image_metadata()
                .width(),
            1000);
  ASSERT_EQ(query_controller.sent_objects_request_.image_data()
                .image_metadata()
                .height(),
            1000);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(GetSelectionTypeFromUrl(url_response_future.Get().url()),
            lens::REGION_SEARCH);
  ASSERT_EQ(interaction_data_response_future.Get().suggest_signals(),
            kTestSuggestSignals);
  ASSERT_EQ(query_controller.sent_objects_request_.request_context()
                .request_id()
                .sequence_id(),
            1);
  ASSERT_EQ(query_controller.sent_interaction_request_.request_context()
                .request_id()
                .sequence_id(),
            2);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .type(),
      lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .selection_metadata()
          .region()
          .region()
          .center_x(),
      50);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .selection_metadata()
          .region()
          .region()
          .center_y(),
      50);
  ASSERT_EQ(query_controller.sent_interaction_request_.image_crop()
                .zoomed_crop()
                .crop()
                .center_x(),
            50);
  ASSERT_EQ(query_controller.sent_interaction_request_.image_crop()
                .zoomed_crop()
                .crop()
                .center_y(),
            50);
  ASSERT_EQ(query_controller.sent_interaction_request_.image_crop()
                .zoomed_crop()
                .crop()
                .center_y(),
            50);
  ASSERT_EQ(query_controller.sent_interaction_request_.image_crop()
                .zoomed_crop()
                .crop()
                .center_y(),
            50);
  ASSERT_EQ(GetExpectedJpegBytesForBitmap(region_bitmap),
            query_controller.sent_interaction_request_.image_crop()
                .image()
                .image_content());
  ASSERT_FALSE(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .has_query_metadata());
  ASSERT_TRUE(has_start_time);
  ASSERT_EQ(query_controller.num_gen204_pings_sent_, 1);
  CheckGen204IdsMatch(query_controller.sent_client_logs_,
                      url_response_future.Get());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchMultimodalSearchInteraction_ReturnsResponses) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlayInteractionResponse>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      profile()->GetVariationsClient(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false);
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), 0);
  task_environment_.RunUntilIdle();

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendMultimodalRequest(
      std::move(region), kTestQueryText, lens::MULTIMODAL_SEARCH,
      additional_search_query_params, std::nullopt);
  task_environment_.RunUntilIdle();
  query_controller.EndQuery();

  std::string actual_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &actual_start_time);

  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_EQ(query_controller.sent_objects_request_.image_data()
                .image_metadata()
                .width(),
            100);
  ASSERT_EQ(query_controller.sent_objects_request_.image_data()
                .image_metadata()
                .height(),
            100);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(GetSelectionTypeFromUrl(url_response_future.Get().url()),
            lens::MULTIMODAL_SEARCH);
  ASSERT_EQ(interaction_data_response_future.Get().suggest_signals(),
            kTestSuggestSignals);
  ASSERT_EQ(query_controller.sent_objects_request_.request_context()
                .request_id()
                .sequence_id(),
            1);
  ASSERT_EQ(query_controller.sent_interaction_request_.request_context()
                .request_id()
                .sequence_id(),
            2);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .type(),
      lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .selection_metadata()
          .region()
          .region()
          .center_x(),
      30);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .selection_metadata()
          .region()
          .region()
          .center_y(),
      40);
  ASSERT_EQ(query_controller.sent_interaction_request_.image_crop()
                .zoomed_crop()
                .crop()
                .center_x(),
            30);
  ASSERT_EQ(query_controller.sent_interaction_request_.image_crop()
                .zoomed_crop()
                .crop()
                .center_y(),
            40);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .query_metadata()
          .text_query()
          .query(),
      kTestQueryText);
  ASSERT_TRUE(has_start_time);
  ASSERT_EQ(query_controller.num_gen204_pings_sent_, 1);
  CheckGen204IdsMatch(query_controller.sent_client_logs_,
                      url_response_future.Get());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchTextOnlyInteraction_ReturnsResponse) {
  feature_list_.InitAndEnableFeatureWithParameters(
      lens::features::kLensOverlay,
      {{"use-search-context-for-text-only-requests", "true"}});
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlayInteractionResponse>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      profile()->GetVariationsClient(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), 0);
  task_environment_.RunUntilIdle();

  query_controller.SendTextOnlyQuery("", TextOnlyQueryType::kLensTextSelection,
                                     additional_search_query_params);
  task_environment_.RunUntilIdle();
  query_controller.EndQuery();

  std::string actual_encoded_search_context;
  net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                             kSearchContextParamKey,
                             &actual_encoded_search_context);

  std::string actual_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &actual_start_time);

  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_TRUE(url_response_future.IsReady());
  ASSERT_FALSE(interaction_data_response_future.IsReady());
  ASSERT_EQ(GetSelectionTypeFromUrl(url_response_future.Get().url()),
            lens::SELECT_TEXT_HIGHLIGHT);
  ASSERT_EQ(actual_encoded_search_context, kTestEncodedSearchContext);
  ASSERT_TRUE(has_start_time);
  ASSERT_EQ(query_controller.num_gen204_pings_sent_, 0);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchInteraction_StartsNewQueryFlowAfterTimeout) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlayInteractionResponse>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      profile()->GetVariationsClient(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false);
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;

  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), 0);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(full_image_response_future.IsReady());
  full_image_response_future.Clear();

  task_environment_.FastForwardBy(base::TimeDelta(base::Minutes(60)));
  query_controller.SendRegionSearch(std::move(region), lens::REGION_SEARCH,
                                    additional_search_query_params,
                                    std::nullopt);
  task_environment_.RunUntilIdle();
  query_controller.EndQuery();

  // The full image response having another value, after it was already
  // cleared, indicates that the query controller successfully started a
  // new query flow due to the timeout occurring.
  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_TRUE(url_response_future.IsReady());
  ASSERT_TRUE(interaction_data_response_future.IsReady());
  ASSERT_EQ(query_controller.num_gen204_pings_sent_, 2);
  CheckGen204IdsMatch(query_controller.sent_client_logs_,
                      url_response_future.Get());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchInteraction_UsesSameAnalyticsIdForLensRequestAndUrl) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlayInteractionResponse>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      profile()->GetVariationsClient(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false);
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;

  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), 0);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(full_image_response_future.IsReady());
  std::string first_analytics_id =
      query_controller.sent_request_id_.analytics_id();
  query_controller.SendRegionSearch(std::move(region), lens::REGION_SEARCH,
                                    additional_search_query_params,
                                    std::nullopt);
  task_environment_.RunUntilIdle();
  query_controller.EndQuery();

  ASSERT_TRUE(url_response_future.IsReady());
  ASSERT_TRUE(interaction_data_response_future.IsReady());
  std::string second_analytics_id =
      query_controller.sent_request_id_.analytics_id();

  ASSERT_NE(second_analytics_id, first_analytics_id);
  ASSERT_EQ(GetAnalyticsIdFromUrl(url_response_future.Get().url()),
            second_analytics_id);
}

}  // namespace lens
