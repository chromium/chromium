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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_features.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
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
constexpr char kTestSuggestSignals[] = "encoded_image_signals";

// The fake server session id.
constexpr char kTestServerSessionId[] = "server_session_id";
constexpr char kTestServerSessionId2[] = "server_session_id2";

// The fake search session id.
constexpr char kTestSearchSessionId[] = "search_session_id";

// The locale to use.
constexpr char kLocale[] = "en-US";

// The fake page information.
constexpr char kTestPageUrl[] = "https://www.google.com";
constexpr char kTestPageTitle[] = "Page Title";

// The url parameter key for the video context.
constexpr char kVideoContextParamKey[] = "vidcip";

// The timestamp param.
constexpr char kStartTimeQueryParam[] = "qsubts";

// The visual search interaction log data param.
constexpr char kVisualSearchInteractionDataQueryParameterKey[] = "vsint";

// Query parameter for the request id.
inline constexpr char kRequestIdParameterKey[] = "vsrid";

// Query parameter for the visual input type.
inline constexpr char kVisualInputTypeParameterKey[] = "vit";

// Query parameter for the invocation source.
inline constexpr char kInvocationSourceParameterKey[] = "source";

// The encoded video context for the test page.
constexpr char kTestEncodedVideoContext[] =
    "ChkKF2h0dHBzOi8vd3d3Lmdvb2dsZS5jb20v";

// The session id query parameter key.
constexpr char kSessionIdQueryParameterKey[] = "gsessionid";

// The region.
constexpr char kRegion[] = "US";

// The time zone.
constexpr char kTimeZone[] = "America/Los_Angeles";

// The parameter key for gen204 request.
constexpr char kGen204IdentifierQueryParameter[] = "plla";

// Fake VariationsClient for testing. Without it, tests crash.
class FakeVariationsClient : public variations::VariationsClient {
 public:
  bool IsOffTheRecord() const override { return false; }

  variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const override {
    base::flat_map<variations::mojom::GoogleWebVisibility, std::string>
        headers = {
            {variations::mojom::GoogleWebVisibility::FIRST_PARTY, "123xyz"}};
    return variations::mojom::VariationsHeaders::New(headers);
  }
};

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

class FakeLensOverlayGen204Controller : public LensOverlayGen204Controller {
 public:
  FakeLensOverlayGen204Controller() = default;
  ~FakeLensOverlayGen204Controller() override = default;

 protected:
  void CheckMetricsConsentAndIssueGen204NetworkRequest(GURL url) override {
    // Noop.
  }
};

class LensOverlayQueryControllerMock : public LensOverlayQueryController {
 public:
  explicit LensOverlayQueryControllerMock(
      LensOverlayFullImageResponseCallback full_image_callback,
      LensOverlayUrlResponseCallback url_callback,
      LensOverlaySuggestInputsCallback interaction_data_callback,
      LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode,
      lens::LensOverlayGen204Controller* gen204_controller)
      : LensOverlayQueryController(full_image_callback,
                                   url_callback,
                                   interaction_data_callback,
                                   thumbnail_created_callback,
                                   variations_client,
                                   identity_manager,
                                   profile,
                                   invocation_source,
                                   use_dark_mode,
                                   gen204_controller) {
    fake_cluster_info_response_.set_server_session_id(kTestServerSessionId);
    fake_cluster_info_response_.set_search_session_id(kTestSearchSessionId);
  }
  ~LensOverlayQueryControllerMock() override = default;

  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response_;
  lens::LensOverlayObjectsResponse fake_objects_response_;
  lens::LensOverlayInteractionResponse fake_interaction_response_;
  GURL sent_fetch_url_;
  lens::LensOverlayClientLogs sent_client_logs_;
  lens::LensOverlayRequestId sent_request_id_;
  lens::LensOverlayRequestId sent_page_content_request_id_;
  lens::LensOverlayObjectsRequest sent_full_image_objects_request_;
  lens::LensOverlayObjectsRequest sent_page_content_objects_request_;
  lens::LensOverlayInteractionRequest sent_interaction_request_;
  int num_full_page_objects_gen204_pings_sent_ = 0;
  int num_full_page_translate_gen204_pings_sent_ = 0;

 protected:
  std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      lens::LensOverlayServerRequest* request,
      const GURL& fetch_url,
      const std::string& http_method,
      const std::vector<std::string>& request_headers,
      const std::vector<std::string>& cors_exempt_headers) override {
    lens::LensOverlayServerResponse fake_server_response;
    std::string fake_server_response_string;
    if (!request) {
      // Cluster info request.
      fake_server_response_string =
          fake_cluster_info_response_.SerializeAsString();
    } else if (request->has_objects_request() &&
               request->objects_request().has_payload()) {
      // Page content upload request.
      sent_page_content_objects_request_.CopyFrom(request->objects_request());
      // The server doesn't send a response to this request, so no need to set
      // the response string to something meaningful.
      fake_server_response_string = "";
      sent_page_content_request_id_.CopyFrom(
          request->objects_request().request_context().request_id());
    } else if (request->has_objects_request()) {
      // Full image request.
      sent_full_image_objects_request_.CopyFrom(request->objects_request());
      fake_server_response.mutable_objects_response()->CopyFrom(
          fake_objects_response_);
      fake_server_response_string = fake_server_response.SerializeAsString();
      sent_request_id_.CopyFrom(
          request->objects_request().request_context().request_id());
    } else if (request->has_interaction_request()) {
      // Interaction request.
      sent_interaction_request_.CopyFrom(request->interaction_request());
      fake_server_response.mutable_interaction_response()->CopyFrom(
          fake_interaction_response_);
      fake_server_response_string = fake_server_response.SerializeAsString();
      sent_request_id_.CopyFrom(
          request->interaction_request().request_context().request_id());
    } else {
      NOTREACHED_IN_MIGRATION();
    }
    if (request) {
      sent_client_logs_.CopyFrom(request->client_logs());
    }
    sent_fetch_url_ = fetch_url;

    // Create the fake endpoint fetcher to return the fake response.
    EndpointResponse fake_endpoint_response;
    fake_endpoint_response.response = fake_server_response_string;
    fake_endpoint_response.http_status_code =
        google_apis::ApiErrorCode::HTTP_SUCCESS;

    return std::make_unique<FakeEndpointFetcher>(fake_endpoint_response);
  }

  void SendLatencyGen204IfEnabled(int64_t latency_ms,
                                  bool is_translate_query) override {
    if (is_translate_query) {
      num_full_page_translate_gen204_pings_sent_++;
    } else {
      num_full_page_objects_gen204_pings_sent_++;
    }
  }
};

class LensOverlayQueryControllerTest : public testing::Test {
 public:
  LensOverlayQueryControllerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    fake_variations_client_ = std::make_unique<FakeVariationsClient>();
  }

  lens::LensOverlayGen204Controller* GetGen204Controller() {
    return gen204_controller_.get();
  }

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
  std::unique_ptr<lens::FakeLensOverlayGen204Controller> gen204_controller_;
  std::unique_ptr<FakeVariationsClient> fake_variations_client_;

  TestingProfile* profile() { return profile_.get(); }

 private:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    profile_ = profile_builder.Build();
    g_browser_process->SetApplicationLocale(kLocale);
    gen204_controller_ =
        std::make_unique<lens::FakeLensOverlayGen204Controller>();
    icu::TimeZone::adoptDefault(
        icu::TimeZone::createTimeZone(icu::UnicodeString(kTimeZone)));
    UErrorCode error_code = U_ZERO_ERROR;
    icu::Locale::setDefault(icu::Locale(kLocale), error_code);
    ASSERT_TRUE(U_SUCCESS(error_code));

    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlayContextualSearchbox,
        {{"use-video-context-for-text-only-requests", "true"},
         {"use-optimized-request-flow", "true"},
         {"use-pdf-vit-param", "true"},
         {"use-webpage-vit-param", "true"}});
  }
};

TEST_F(LensOverlayQueryControllerTest, FetchInitialQuery_ReturnsResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(), base::NullCallback(),
      base::NullCallback(), base::NullCallback(), fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_content_bytes=*/{}, lens::PageContentMimeType::kNone, 0);

  task_environment_.RunUntilIdle();
  query_controller.EndQuery();
  ASSERT_TRUE(full_image_response_future.IsReady());

  // Check initial fetch objects request is correct.
  auto sent_object_request = query_controller.sent_full_image_objects_request_;
  ASSERT_EQ(sent_object_request.request_context().request_id().sequence_id(),
            1);
  ASSERT_EQ(sent_object_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(sent_object_request.image_data().image_metadata().height(), 100);
  ASSERT_EQ(sent_object_request.request_context()
                .client_context()
                .locale_context()
                .language(),
            kLocale);
  ASSERT_EQ(sent_object_request.request_context()
                .client_context()
                .locale_context()
                .region(),
            kRegion);
  ASSERT_EQ(sent_object_request.request_context()
                .client_context()
                .locale_context()
                .time_zone(),
            kTimeZone);
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 0);
  ASSERT_EQ(query_controller.sent_client_logs_.lens_overlay_entry_point(),
            lens::LensOverlayClientLogs::APP_MENU);
  ASSERT_TRUE(query_controller.sent_client_logs_.has_paella_id());
}

// Tests that the query controller attaches the server session id from the
// cluster info response to the full image request.
TEST_F(LensOverlayQueryControllerTest,
       FetchInitialQuery_UsesClusterInfoResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(), base::NullCallback(),
      base::NullCallback(), base::NullCallback(), fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_content_bytes=*/{}, lens::PageContentMimeType::kNone, 0);

  task_environment_.RunUntilIdle();
  query_controller.EndQuery();
  ASSERT_TRUE(full_image_response_future.IsReady());

  // Check the server session id is attached to the fetch url.
  std::string session_id_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(query_controller.sent_fetch_url_,
                                         kSessionIdQueryParameterKey,
                                         &session_id_value));
  ASSERT_EQ(session_id_value, kTestServerSessionId);
}

// Tests that the query controller attaches the server session id from the
// cluster info response to the interaction request, even if the full image
// request included a different server session id.
TEST_F(LensOverlayQueryControllerTest,
       FetchInteraction_UsesClusterInfoResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlaySuggestInputs>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId2);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_content_bytes=*/{}, lens::PageContentMimeType::kNone, 0);
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

  // Check the server session id is attached to the fetch url.
  std::string session_id_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(query_controller.sent_fetch_url_,
                                         kSessionIdQueryParameterKey,
                                         &session_id_value));
  ASSERT_EQ(session_id_value, kTestServerSessionId);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchRegionSearchInteraction_ReturnsResponses) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlaySuggestInputs>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_content_bytes=*/{}, lens::PageContentMimeType::kNone, 0);
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

  std::string unused_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &unused_start_time);

  ASSERT_TRUE(full_image_response_future.IsReady());

  // Check the initial fetch objects request.
  auto sent_object_request = query_controller.sent_full_image_objects_request_;
  ASSERT_EQ(sent_object_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(sent_object_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(GetSelectionTypeFromUrl(url_response_future.Get().url()),
            lens::REGION_SEARCH);
  ASSERT_EQ(interaction_data_response_future.Get().encoded_image_signals(),
            kTestSuggestSignals);
  ASSERT_EQ(sent_object_request.request_context().request_id().sequence_id(),
            1);

  // Verify the interaction request.
  auto sent_interaction_request = query_controller.sent_interaction_request_;
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .selection_metadata()
                .region()
                .region()
                .center_x(),
            30);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .selection_metadata()
                .region()
                .region()
                .center_y(),
            40);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_x(),
      30);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_y(),
      40);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_query_metadata());
  ASSERT_TRUE(has_start_time);
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 1);
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
  base::test::TestFuture<lens::proto::LensOverlaySuggestInputs>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap viewport_bitmap = CreateNonEmptyBitmap(1000, 1000);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      viewport_bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_content_bytes=*/{}, lens::PageContentMimeType::kNone, 0);
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

  std::string unused_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &unused_start_time);

  ASSERT_TRUE(full_image_response_future.IsReady());

  // Check initial fetch objects request is correct.
  auto sent_object_request = query_controller.sent_full_image_objects_request_;
  ASSERT_EQ(sent_object_request.image_data().image_metadata().width(), 1000);
  ASSERT_EQ(sent_object_request.image_data().image_metadata().height(), 1000);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(GetSelectionTypeFromUrl(url_response_future.Get().url()),
            lens::REGION_SEARCH);
  ASSERT_EQ(interaction_data_response_future.Get().encoded_image_signals(),
            kTestSuggestSignals);
  ASSERT_EQ(sent_object_request.request_context().request_id().sequence_id(),
            1);

  // Verify the interaction request.
  auto sent_interaction_request = query_controller.sent_interaction_request_;
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .selection_metadata()
                .region()
                .region()
                .center_x(),
            50);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .selection_metadata()
                .region()
                .region()
                .center_y(),
            50);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_x(),
      50);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_y(),
      50);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_y(),
      50);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_y(),
      50);
  ASSERT_EQ(GetExpectedJpegBytesForBitmap(region_bitmap),
            sent_interaction_request.image_crop().image().image_content());
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_query_metadata());
  ASSERT_TRUE(has_start_time);
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 1);
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
  base::test::TestFuture<lens::proto::LensOverlaySuggestInputs>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_content_bytes=*/{}, lens::PageContentMimeType::kNone, 0);
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

  std::string unused_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &unused_start_time);

  ASSERT_TRUE(full_image_response_future.IsReady());

  // Check initial fetch objects request is correct.
  auto sent_object_request = query_controller.sent_full_image_objects_request_;
  ASSERT_EQ(sent_object_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(sent_object_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(GetSelectionTypeFromUrl(url_response_future.Get().url()),
            lens::MULTIMODAL_SEARCH);
  ASSERT_EQ(interaction_data_response_future.Get().encoded_image_signals(),
            kTestSuggestSignals);
  ASSERT_EQ(sent_object_request.request_context().request_id().sequence_id(),
            1);

  // Verify the interaction request.
  auto sent_interaction_request = query_controller.sent_interaction_request_;
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .selection_metadata()
                .region()
                .region()
                .center_x(),
            30);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .selection_metadata()
                .region()
                .region()
                .center_y(),
            40);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_x(),
      30);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_y(),
      40);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_TRUE(has_start_time);
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 1);
  CheckGen204IdsMatch(query_controller.sent_client_logs_,
                      url_response_future.Get());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchTextOnlyInteraction_ReturnsResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlaySuggestInputs>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_content_bytes=*/{}, lens::PageContentMimeType::kNone, 0);
  task_environment_.RunUntilIdle();

  query_controller.SendTextOnlyQuery("", TextOnlyQueryType::kLensTextSelection,
                                     additional_search_query_params);
  task_environment_.RunUntilIdle();
  query_controller.EndQuery();

  std::string actual_encoded_video_context;
  net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                             kVideoContextParamKey,
                             &actual_encoded_video_context);

  std::string unused_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &unused_start_time);

  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_TRUE(url_response_future.IsReady());
  ASSERT_FALSE(interaction_data_response_future.IsReady());
  ASSERT_EQ(GetSelectionTypeFromUrl(url_response_future.Get().url()),
            lens::SELECT_TEXT_HIGHLIGHT);
  ASSERT_EQ(actual_encoded_video_context, kTestEncodedVideoContext);
  ASSERT_TRUE(has_start_time);
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 0);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchTextOnlyInteractionWithPdfBytes_ReturnsResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlaySuggestInputs>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  std::vector<uint8_t> fake_content_bytes({1, 2, 3, 4});
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), fake_content_bytes,
      lens::PageContentMimeType::kPdf, 0);
  task_environment_.RunUntilIdle();
  query_controller.SendTextOnlyQuery(kTestQueryText,
                                     TextOnlyQueryType::kLensTextSelection,
                                     additional_search_query_params);
  task_environment_.RunUntilIdle();
  query_controller.EndQuery();

  ASSERT_TRUE(full_image_response_future.IsReady());

  // Verify the content bytes were not included with the image bytes request.
  auto full_image_request = query_controller.sent_full_image_objects_request_;
  ASSERT_EQ(full_image_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(full_image_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(full_image_request.payload().content_data().empty());

  // Verify the content bytes were included in a followup request.
  auto page_content_request =
      query_controller.sent_page_content_objects_request_;
  ASSERT_FALSE(page_content_request.payload().content_data().empty());
  ASSERT_EQ(page_content_request.payload().content_type(), "application/pdf");

  // The full image and page content requests should have the same request id.
  ASSERT_EQ(full_image_request.request_context().request_id().sequence_id(),
            page_content_request.request_context().request_id().sequence_id());

  // Check interaction request is correct.
  auto sent_interaction_request = query_controller.sent_interaction_request_;
  ASSERT_TRUE(interaction_data_response_future.IsReady());
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 3);
  ASSERT_EQ(
      sent_interaction_request.interaction_request_metadata().type(),
      lens::LensOverlayInteractionRequestMetadata::CONTEXTUAL_SEARCH_QUERY);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_selection_metadata());

  // Check search URL is correct.
  ASSERT_TRUE(url_response_future.IsReady());
  std::string unused_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &unused_start_time);
  std::string visual_input_type;
  bool has_visual_input_type = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kVisualInputTypeParameterKey,
      &visual_input_type);
  std::string invocation_source;
  bool has_invocation_source = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kInvocationSourceParameterKey,
      &invocation_source);
  ASSERT_EQ(GetSelectionTypeFromUrl(url_response_future.Get().url()),
            lens::UNKNOWN_SELECTION_TYPE);
  ASSERT_TRUE(has_start_time);
  ASSERT_TRUE(has_visual_input_type);
  ASSERT_EQ(visual_input_type, "pdf");
  ASSERT_TRUE(has_invocation_source);
  ASSERT_EQ(invocation_source, "chrome.cr.menu");
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 1);
  ASSERT_TRUE(url_response_future.Get().has_url());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchTextOnlyInteractionWithHtml_ReturnsResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlaySuggestInputs>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  std::vector<uint8_t> fake_content_bytes({1, 2, 3, 4});
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), fake_content_bytes,
      lens::PageContentMimeType::kHtml, 0);
  task_environment_.RunUntilIdle();
  query_controller.SendTextOnlyQuery(kTestQueryText,
                                     TextOnlyQueryType::kLensTextSelection,
                                     additional_search_query_params);
  task_environment_.RunUntilIdle();
  query_controller.EndQuery();

  ASSERT_TRUE(full_image_response_future.IsReady());

  // Verify the content bytes were not included with the image bytes request.
  auto full_image_request = query_controller.sent_full_image_objects_request_;
  ASSERT_EQ(full_image_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(full_image_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(full_image_request.payload().content_data().empty());

  // Verify the content bytes were included in a followup request.
  auto page_content_request =
      query_controller.sent_page_content_objects_request_;
  ASSERT_FALSE(page_content_request.payload().content_data().empty());
  ASSERT_EQ(page_content_request.payload().content_type(), "text/html");

  // The full image and page content requests should have the same request id.
  ASSERT_EQ(full_image_request.request_context().request_id().sequence_id(),
            page_content_request.request_context().request_id().sequence_id());

  // Check interaction request is correct.
  auto sent_interaction_request = query_controller.sent_interaction_request_;
  ASSERT_TRUE(interaction_data_response_future.IsReady());
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 3);
  ASSERT_EQ(
      sent_interaction_request.interaction_request_metadata().type(),
      lens::LensOverlayInteractionRequestMetadata::CONTEXTUAL_SEARCH_QUERY);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_selection_metadata());

  // Check search URL is correct.
  ASSERT_TRUE(url_response_future.IsReady());
  std::string unused_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &unused_start_time);
  std::string visual_input_type;
  bool has_visual_input_type = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kVisualInputTypeParameterKey,
      &visual_input_type);
  std::string invocation_source;
  bool has_invocation_source = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kInvocationSourceParameterKey,
      &invocation_source);
  ASSERT_EQ(GetSelectionTypeFromUrl(url_response_future.Get().url()),
            lens::UNKNOWN_SELECTION_TYPE);
  ASSERT_TRUE(has_start_time);
  ASSERT_TRUE(has_visual_input_type);
  ASSERT_EQ(visual_input_type, "wp");
  ASSERT_TRUE(has_invocation_source);
  ASSERT_EQ(invocation_source, "chrome.cr.menu");
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 1);
  ASSERT_TRUE(url_response_future.Get().has_url());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchTextOnlyInteractionWithHtmlInnerText_ReturnsResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlaySuggestInputs>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  std::vector<uint8_t> fake_content_bytes({1, 2, 3, 4});
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), fake_content_bytes,
      lens::PageContentMimeType::kPlainText, 0);
  task_environment_.RunUntilIdle();
  query_controller.SendTextOnlyQuery(kTestQueryText,
                                     TextOnlyQueryType::kLensTextSelection,
                                     additional_search_query_params);
  task_environment_.RunUntilIdle();
  query_controller.EndQuery();

  ASSERT_TRUE(full_image_response_future.IsReady());

  // Verify the content bytes were not included with the image bytes request.
  auto full_image_request = query_controller.sent_full_image_objects_request_;
  ASSERT_EQ(full_image_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(full_image_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(full_image_request.payload().content_data().empty());

  // Verify the content bytes were included in a followup request.
  auto page_content_request =
      query_controller.sent_page_content_objects_request_;
  ASSERT_FALSE(page_content_request.payload().content_data().empty());
  ASSERT_EQ(page_content_request.payload().content_type(), "text/plain");

  // The full image and page content requests should have the same request id.
  ASSERT_EQ(full_image_request.request_context().request_id().sequence_id(),
            page_content_request.request_context().request_id().sequence_id());

  // Check interaction request is correct.
  auto sent_interaction_request = query_controller.sent_interaction_request_;
  ASSERT_TRUE(interaction_data_response_future.IsReady());
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 3);
  ASSERT_EQ(
      sent_interaction_request.interaction_request_metadata().type(),
      lens::LensOverlayInteractionRequestMetadata::CONTEXTUAL_SEARCH_QUERY);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_selection_metadata());

  // Check search URL is correct.
  ASSERT_TRUE(url_response_future.IsReady());
  std::string unused_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &unused_start_time);
  std::string visual_input_type;
  bool has_visual_input_type = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kVisualInputTypeParameterKey,
      &visual_input_type);
  std::string invocation_source;
  bool has_invocation_source = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kInvocationSourceParameterKey,
      &invocation_source);
  ASSERT_EQ(GetSelectionTypeFromUrl(url_response_future.Get().url()),
            lens::UNKNOWN_SELECTION_TYPE);
  ASSERT_TRUE(has_start_time);
  ASSERT_TRUE(has_visual_input_type);
  ASSERT_EQ(visual_input_type, "wp");
  ASSERT_TRUE(has_invocation_source);
  ASSERT_EQ(invocation_source, "chrome.cr.menu");
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 1);
  ASSERT_TRUE(url_response_future.Get().has_url());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchInteraction_StartsNewQueryFlowAfterTimeout) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlaySuggestInputs>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
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
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_content_bytes=*/{}, lens::PageContentMimeType::kNone,
      /**/ 0);
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
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 2);
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
  base::test::TestFuture<lens::proto::LensOverlaySuggestInputs>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
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
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_content_bytes=*/{}, lens::PageContentMimeType::kNone, 0);
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

TEST_F(LensOverlayQueryControllerTest,
       SendFullPageTranslateQuery_UpdatesRequestIdCorrectly) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlaySuggestInputs>
      interaction_data_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
  query_controller.fake_objects_response_.mutable_cluster_info()
      ->set_server_session_id(kTestServerSessionId);
  query_controller.fake_interaction_response_.set_encoded_response(
      kTestSuggestSignals);
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, std::make_optional<GURL>(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_content_bytes=*/{}, lens::PageContentMimeType::kNone, 0);
  ASSERT_TRUE(full_image_response_future.Wait());

  // Check initial fetch objects request id is correct.
  ASSERT_TRUE(full_image_response_future.IsReady());
  auto initial_sent_object_request =
      query_controller.sent_full_image_objects_request_;
  ASSERT_EQ(initial_sent_object_request.request_context()
                .request_id()
                .image_sequence_id(),
            1);
  ASSERT_EQ(
      initial_sent_object_request.request_context().request_id().sequence_id(),
      1);
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 1);

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendMultimodalRequest(
      std::move(region), kTestQueryText, lens::MULTIMODAL_SEARCH,
      additional_search_query_params, std::nullopt);
  ASSERT_TRUE(interaction_data_response_future.Wait());

  // Verify the interaction request id sequence was incremented.
  auto initial_sent_interaction_request =
      query_controller.sent_interaction_request_;
  ASSERT_EQ(initial_sent_interaction_request.request_context()
                .request_id()
                .sequence_id(),
            2);
  std::string interaction_analytics_id =
      GetAnalyticsIdFromUrl(url_response_future.Get().url());
  ASSERT_NE(interaction_analytics_id,
            initial_sent_object_request.request_context()
                .request_id()
                .analytics_id());

  // Now issue a fullpage translate request.
  full_image_response_future.Clear();
  query_controller.SendFullPageTranslateQuery("en", "de");
  ASSERT_TRUE(full_image_response_future.Wait());

  // Check that the image sequence id and sequence id were incremented by
  // the fullpage translate request, and a new analytics id was generated.
  ASSERT_TRUE(full_image_response_future.IsReady());
  auto second_sent_object_request =
      query_controller.sent_full_image_objects_request_;
  ASSERT_EQ(second_sent_object_request.request_context()
                .request_id()
                .image_sequence_id(),
            2);
  ASSERT_NE(
      second_sent_object_request.request_context().request_id().analytics_id(),
      interaction_analytics_id);
  // Interactions increment the sequence twice (once for Lens requests and once
  // in the search url) so the sequence id should now be 4.
  ASSERT_EQ(
      second_sent_object_request.request_context().request_id().sequence_id(),
      4);
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 1);
  ASSERT_EQ(query_controller.num_full_page_translate_gen204_pings_sent_, 1);

  // Now change the languages.
  full_image_response_future.Clear();
  query_controller.SendFullPageTranslateQuery("en", "es");
  ASSERT_TRUE(full_image_response_future.Wait());

  // Check that the image sequence id and sequence id were incremented by
  // the fullpage translate request, and a new analytics id was generated.
  ASSERT_TRUE(full_image_response_future.IsReady());
  auto third_sent_object_request =
      query_controller.sent_full_image_objects_request_;
  ASSERT_EQ(third_sent_object_request.request_context()
                .request_id()
                .image_sequence_id(),
            3);
  ASSERT_NE(
      third_sent_object_request.request_context().request_id().analytics_id(),
      second_sent_object_request.request_context().request_id().analytics_id());
  ASSERT_EQ(
      third_sent_object_request.request_context().request_id().sequence_id(),
      5);
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 1);
  ASSERT_EQ(query_controller.num_full_page_translate_gen204_pings_sent_, 2);

  // Now disable translate mode.
  full_image_response_future.Clear();
  query_controller.SendEndTranslateModeQuery();
  ASSERT_TRUE(full_image_response_future.Wait());

  // Check that the image sequence id and sequence id were incremented by
  // the end translate mode request.
  ASSERT_TRUE(full_image_response_future.IsReady());
  auto fourth_sent_object_request =
      query_controller.sent_full_image_objects_request_;
  ASSERT_EQ(fourth_sent_object_request.request_context()
                .request_id()
                .image_sequence_id(),
            4);
  ASSERT_EQ(
      fourth_sent_object_request.request_context().request_id().sequence_id(),
      6);
  ASSERT_NE(
      fourth_sent_object_request.request_context().request_id().analytics_id(),
      third_sent_object_request.request_context().request_id().analytics_id());
  ASSERT_EQ(query_controller.num_full_page_objects_gen204_pings_sent_, 2);
  ASSERT_EQ(query_controller.num_full_page_translate_gen204_pings_sent_, 2);

  query_controller.EndQuery();
}

}  // namespace lens
