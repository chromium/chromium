// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"

#include <string>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
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
#include "chrome/browser/ui/lens/test_lens_overlay_query_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/base32/base32.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/lens_server_proto/lens_overlay_client_context.pb.h"
#include "third_party/lens_server_proto/lens_overlay_document.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_text.pb.h"
#include "third_party/lens_server_proto/lens_overlay_visual_search_interaction_data.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/zstd/src/lib/zstd.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "url/gurl.h"

namespace lens {

using LatencyType = LensOverlayGen204Controller::LatencyType;
using base::test::EqualsProto;

// The fake multimodal query text.
constexpr char kTestQueryText[] = "query_text";

// The fake suggest signals.
constexpr char kTestSuggestSignals[] = "encoded_image_signals";

// The fake server session id.
constexpr char kTestServerSessionId[] = "server_session_id";
constexpr char kTestServerSessionId2[] = "server_session_id2";

// The fake routing info.
constexpr char kTestServerAddress[] = "test_server_address";

// The fake search session id.
constexpr char kTestSearchSessionId[] = "search_session_id";

// The locale to use.
constexpr char kLocale[] = "en-US";

// Fake username for OAuth.
constexpr char kFakePrimaryUsername[] = "test-primary@example.com";

// Fake OAuth token.
constexpr char kFakeOAuthToken[] = "fake-oauth-token";

// The fake page information.
constexpr char kTestPageUrl[] = "https://www.google.com/";
constexpr char kTestPageTitle[] = "Page Title";

// The url parameter key for the video context.
constexpr char kVideoContextParamKey[] = "vidcip";

// Query parameter for the query submission time.
inline constexpr char kQuerySubmissionTimeQueryParameter[] = "qsubts";

// Query parameter for the client upload processing duration.
inline constexpr char kClientUploadDurationQueryParameter[] = "cud";

// The visual search interaction log data param.
constexpr char kVisualSearchInteractionDataQueryParameterKey[] = "vsint";

// Query parameter for the request id.
inline constexpr char kRequestIdParameterKey[] = "vsrid";

// Query parameter for the visual input type.
inline constexpr char kVisualInputTypeParameterKey[] = "vit";

// Query parameter for the invocation source.
inline constexpr char kInvocationSourceParameterKey[] = "source";

// The session id query parameter key.
constexpr char kSessionIdQueryParameterKey[] = "gsessionid";

// The region.
constexpr char kRegion[] = "US";

// The time zone.
constexpr char kTimeZone[] = "America/Los_Angeles";

// The parameter key for gen204 request.
constexpr char kGen204IdentifierQueryParameter[] = "plla";

// The test time.
constexpr base::Time kTestTime = base::Time::FromSecondsSinceUnixEpoch(1000);

// kFakeContentBytes and kNewFakeContentBytes needs to outlive the query
// controller, so initialize it here.
const std::vector<uint8_t> kFakeContentBytes({1, 2, 3, 4});
const std::vector<uint8_t> kFakeContentBytes2({2, 3, 4, 5, 6});
const std::vector<uint8_t> kNewFakeContentBytes({5, 6, 7, 8});
const std::vector<uint8_t> kFakeSmallContentBytes({1, 2});

// The PageContent needs to outlive the query controller, so initialize it here.
const std::vector<lens::PageContent> kFakePdfPageContents = {
    lens::PageContent(kFakeContentBytes, lens::MimeType::kPdf)};
const std::vector<lens::PageContent> kFakeSmallPdfPageContents = {
    lens::PageContent(kFakeSmallContentBytes, lens::MimeType::kPdf)};
const std::vector<lens::PageContent> kFakeTextPageContents = {
    lens::PageContent(kFakeContentBytes, lens::MimeType::kPlainText)};
const std::vector<lens::PageContent> kFakeApcPageContents = {
    lens::PageContent(kFakeContentBytes,
                      lens::MimeType::kAnnotatedPageContent)};
const std::vector<lens::PageContent> kFakeHtmlPageContentsWithMultipleContents =
    {lens::PageContent(kFakeContentBytes, lens::MimeType::kHtml),
     lens::PageContent(kFakeContentBytes2, lens::MimeType::kPlainText),
     lens::PageContent(kNewFakeContentBytes,
                       lens::MimeType::kAnnotatedPageContent)};
const std::vector<lens::PageContent> kNewFakeTextPageContents = {
    lens::PageContent(kFakeContentBytes, lens::MimeType::kPlainText)};

const std::vector<std::u16string> kShortPartialContent({u"page 1", u"page 2",
                                                        u"page 3"});
const std::vector<std::u16string> kLongPartialContent(
    {u"this is a page with over 100 characters to ensure that the average "
     "characters per page is above the heuristic."});

const base::test::FeatureRefAndParams
    kDefaultLensOverlayContextualSearchboxParams =
        base::test::FeatureRefAndParams(
            lens::features::kLensOverlayContextualSearchbox,
            {{"send-lens-inputs-for-contextual-suggest", "true"},
             {"use-updated-content-fields", "false"},
             {"page-content-request-id-fix", "false"},
             {"send-lens-inputs-for-lens-suggest", "true"},
             {"send-lens-visual-interaction-data-for-lens-suggest", "true"},
             {"characters-per-page-heuristic", "50"}});

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

class FakeLensOverlayGen204Controller : public LensOverlayGen204Controller {
 public:
  FakeLensOverlayGen204Controller() = default;
  ~FakeLensOverlayGen204Controller() override = default;

 protected:
  void CheckMetricsConsentAndIssueGen204NetworkRequest(GURL url) override {
    // Noop.
  }
};

class LensOverlayQueryControllerTest : public testing::Test {
 public:
  // Constructs a LensOverlayQueryControllerTest with |traits| being forwarded
  // to its TaskEnvironment. MainThreadType always defaults to UI and must not
  // be specified.
  template <typename... TaskEnvironmentTraits>
  NOINLINE explicit LensOverlayQueryControllerTest(
      TaskEnvironmentTraits&&... traits)
      : LensOverlayQueryControllerTest(
            std::make_unique<content::BrowserTaskEnvironment>(
                content::BrowserTaskEnvironment::MainThreadType::UI,
                std::forward<TaskEnvironmentTraits>(traits)...)) {}

  explicit LensOverlayQueryControllerTest(
      std::unique_ptr<content::BrowserTaskEnvironment> task_environment)
      : task_environment_(std::move(task_environment)) {
    fake_variations_client_ = std::make_unique<FakeVariationsClient>();
  }

  void WaitForSuggestInputsWithEncodedImageSignals(
      LensOverlayQueryController* query_controller) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return query_controller->GetLensSuggestInputs()
          .has_encoded_image_signals();
    }));
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
    std::optional<std::vector<uint8_t>> data = gfx::JPEGCodec::Encode(
        bitmap, lens::features::GetLensOverlayImageCompressionQuality());
    return std::string(base::as_string_view(data.value()));
  }

  lens::LensOverlayVisualSearchInteractionData GetVsintFromUrl(
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
    return proto;
  }

  std::string GetEncodedRequestIdFromUrl(std::string url_string) {
    GURL url = GURL(url_string);
    std::string vsrid_param;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(url, kRequestIdParameterKey, &vsrid_param));
    return vsrid_param;
  }

  lens::LensOverlayRequestId DecodeRequestIdFromVsrid(std::string vsrid_param) {
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        vsrid_param, base::Base64UrlDecodePolicy::DISALLOW_PADDING,
        &serialized_proto));
    lens::LensOverlayRequestId proto;
    EXPECT_TRUE(proto.ParseFromString(serialized_proto));
    return proto;
  }

  lens::LensOverlayRequestId GetRequestIdFromUrl(std::string url_string) {
    GURL url = GURL(url_string);
    std::string vsrid_param;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(url, kRequestIdParameterKey, &vsrid_param));
    return DecodeRequestIdFromVsrid(vsrid_param);
  }

  std::string GetAnalyticsIdFromUrl(std::string url_string) {
    return GetRequestIdFromUrl(url_string).analytics_id();
  }

  lens::LensOverlayRoutingInfo GetRoutingInfoFromUrl(std::string url_string) {
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
    return proto.routing_info();
  }

  void CheckGen204IdsMatch(
      const lens::LensOverlayClientLogs& client_logs,
      const lens::proto::LensOverlayUrlResponse& url_response) {
    std::string url_gen204_id;
    bool has_gen204_id = net::GetValueForKeyInQuery(
        GURL(url_response.url()), kGen204IdentifierQueryParameter,
        &url_gen204_id);
    ASSERT_TRUE(has_gen204_id);
    ASSERT_NE(client_logs.paella_id(), 0u);
    ASSERT_EQ(base::NumberToString(client_logs.paella_id()).c_str(),
              url_gen204_id);
  }

  void CheckVsintMatchesInteractionRequest(
      const lens::LensOverlayVisualSearchInteractionData& vsint,
      const lens::LensOverlayInteractionRequest& interaction_request) {
    ASSERT_EQ(vsint.interaction_type(),
              interaction_request.interaction_request_metadata().type());
    if (interaction_request.has_interaction_request_metadata() &&
        interaction_request.interaction_request_metadata()
            .has_selection_metadata() &&
        interaction_request.interaction_request_metadata()
            .selection_metadata()
            .has_object()) {
      ASSERT_EQ(vsint.object_id(),
                interaction_request.interaction_request_metadata()
                    .selection_metadata()
                    .object()
                    .object_id());
    } else {
      // Proto3 primitives don't have a has_foo method.
      ASSERT_EQ(vsint.object_id(), "");
    }
    auto interaction_type =
        interaction_request.interaction_request_metadata().type();
    if (interaction_request.has_image_crop()) {
      EXPECT_THAT(vsint.zoomed_crop(),
                  EqualsProto(interaction_request.image_crop().zoomed_crop()));
    } else if (interaction_type ==
                   lens::LensOverlayInteractionRequestMetadata::PDF_QUERY ||
               interaction_type ==
                   lens::LensOverlayInteractionRequestMetadata::WEBPAGE_QUERY) {
      ASSERT_TRUE(vsint.has_zoomed_crop());
      const auto& crop = vsint.zoomed_crop().crop();
      EXPECT_EQ(crop.center_x(), 0.5f);
      EXPECT_EQ(crop.center_y(), 0.5f);
      EXPECT_EQ(crop.width(), 1);
      EXPECT_EQ(crop.height(), 1);
      EXPECT_EQ(crop.coordinate_type(), lens::CoordinateType::NORMALIZED);
      EXPECT_EQ(vsint.zoomed_crop().zoom(), 1);
    } else {
      ASSERT_FALSE(vsint.has_zoomed_crop());
    }
  }

  void CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      const lens::LensOverlayServerClusterInfoRequest& cluster_info_request) {
    ASSERT_TRUE(cluster_info_request.enable_search_session_id());
    ASSERT_EQ(cluster_info_request.surface(), lens::SURFACE_CHROMIUM);
    ASSERT_EQ(cluster_info_request.platform(), lens::PLATFORM_WEB);
    ASSERT_EQ(cluster_info_request.rendering_context().rendering_environment(),
              lens::RENDERING_ENV_LENS_OVERLAY);
  }

  void CheckClusterInfoRequestMatchesUpdatedClientContextRequest(
      const lens::LensOverlayServerClusterInfoRequest& cluster_info_request) {
    ASSERT_TRUE(cluster_info_request.enable_search_session_id());
    ASSERT_EQ(cluster_info_request.surface(), lens::SURFACE_LENS_OVERLAY);
    ASSERT_EQ(cluster_info_request.platform(), lens::PLATFORM_LENS_OVERLAY);
    ASSERT_EQ(cluster_info_request.rendering_context().rendering_environment(),
              lens::RENDERING_ENV_UNSPECIFIED);
  }

  std::string GetEncodedRequestId(lens::LensOverlayRequestId request_id) {
    std::string serialized_proto;
    EXPECT_TRUE(request_id.SerializeToString(&serialized_proto));
    std::string encoded_proto;
    base::Base64UrlEncode(serialized_proto,
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &encoded_proto);
    return encoded_proto;
  }

  void InitFeaturesWithClusterInfoOptimization() {
    feature_list_.Reset();
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlayLatencyOptimizations,
          {{"enable-cluster-info-optimization", "true"}}},
         kDefaultLensOverlayContextualSearchboxParams},
        {});
  }

  void InitFeaturesWithClusterInfoOptimizationAndUpdatedClientContext() {
    feature_list_.Reset();
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlayLatencyOptimizations,
          {{"enable-cluster-info-optimization", "true"}}},
         {lens::features::kLensOverlayUpdatedClientContext, {}}},
        {});
  }

  void InitFeaturesWithPdfCompression() {
    feature_list_.Reset();
    base::FieldTrialParams params =
        kDefaultLensOverlayContextualSearchboxParams.params;
    params.insert({"ztsd-compress-pdf-bytes", "true"});
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlayContextualSearchbox, params);
  }

  void InitFeaturesWithNewContentPayload() {
    feature_list_.Reset();
    base::FieldTrialParams params =
        kDefaultLensOverlayContextualSearchboxParams.params;
    params["use-updated-content-fields"] = "true";
    params.insert({"use-inner-text-with-inner-html", "true"});
    params.insert({"use-apc-with-inner-html", "true"});
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlayContextualSearchbox, params);
  }

  void InitFeaturesWithUploadChunking() {
    feature_list_.Reset();
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlayUploadChunking,
          {{"chunk-size-bytes", "3"}}},
         kDefaultLensOverlayContextualSearchboxParams},
        {});
  }

  void InitFeaturesWithUploadChunkingAndNewContentPayload() {
    feature_list_.Reset();
    base::FieldTrialParams params =
        kDefaultLensOverlayContextualSearchboxParams.params;
    params["use-updated-content-fields"] = "true";
    params.insert({"use-inner-text-with-inner-html", "true"});
    params.insert({"use-apc-with-inner-html", "true"});
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlayUploadChunking,
          {{"chunk-size-bytes", "3"}}},
         {lens::features::kLensOverlayContextualSearchbox, params}},
        {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<lens::FakeLensOverlayGen204Controller> gen204_controller_;
  std::unique_ptr<FakeVariationsClient> fake_variations_client_;
  base::WeakPtrFactory<LensOverlayQueryControllerTest> weak_factory_{this};

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
    PrefService* prefs = profile_->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
    prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, true);

    feature_list_.InitWithFeaturesAndParameters(
        {kDefaultLensOverlayContextualSearchboxParams}, {});
    testing::Test::SetUp();
  }
};

TEST_F(LensOverlayQueryControllerTest, FetchInitialQuery_ReturnsResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(), base::NullCallback(),
      base::NullCallback(), base::NullCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());

  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());
  query_controller.EndQuery();

  // Check initial fetch objects request is correct.
  auto sent_object_request = query_controller.sent_full_image_objects_request();
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
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialFullPageObjectsRequestSent),
            1);
  ASSERT_EQ(
      query_controller.latency_gen_204_counter(
          LatencyType::kInvocationToInitialFullPageObjectsResponseReceived),
      1);
  ASSERT_EQ(query_controller.sent_client_logs().lens_overlay_entry_point(),
            lens::LensOverlayClientLogs::APP_MENU);
  ASSERT_NE(query_controller.sent_client_logs().paella_id(), 0u);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchInitialQuery_UpdatedClientContext_ReturnsResponse) {
  InitFeaturesWithClusterInfoOptimizationAndUpdatedClientContext();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(), base::NullCallback(),
      base::NullCallback(), base::NullCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());

  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesUpdatedClientContextRequest(
      *query_controller.last_cluster_info_request());
  query_controller.EndQuery();

  // Check initial fetch objects request is correct.
  auto sent_object_request = query_controller.sent_full_image_objects_request();
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
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialFullPageObjectsRequestSent),
            1);
  ASSERT_EQ(
      query_controller.latency_gen_204_counter(
          LatencyType::kInvocationToInitialFullPageObjectsResponseReceived),
      1);
  ASSERT_EQ(query_controller.sent_client_logs().lens_overlay_entry_point(),
            lens::LensOverlayClientLogs::APP_MENU);
  ASSERT_NE(query_controller.sent_client_logs().paella_id(), 0u);
}

// Tests that the query controller attaches the server session id from the
// cluster info response to the full image request.
TEST_F(LensOverlayQueryControllerTest,
       FetchInitialQuery_UsesClusterInfoResponse) {
  feature_list_.Reset();
  feature_list_.InitAndEnableFeatureWithParameters(
      lens::features::kLensOverlayLatencyOptimizations,
      {{"enable-cluster-info-optimization", "true"}});

  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(), base::NullCallback(),
      base::NullCallback(), base::NullCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());

  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());
  query_controller.EndQuery();

  // Check the server session id is attached to the fetch url.
  std::string session_id_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(query_controller.sent_fetch_url(),
                                         kSessionIdQueryParameterKey,
                                         &session_id_value));
  ASSERT_EQ(session_id_value, kTestServerSessionId);

  const auto& latest_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_FALSE(latest_suggest_inputs.has_encoded_image_signals());
  ASSERT_FALSE(
      latest_suggest_inputs.has_encoded_visual_search_interaction_log_data());
  ASSERT_EQ(latest_suggest_inputs.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(GetEncodedRequestId(query_controller.sent_full_image_request_id()),
            latest_suggest_inputs.encoded_request_id());
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialClusterInfoRequestSent),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialFullPageObjectsRequestSent),
            1);
}

TEST_F(LensOverlayQueryControllerTest,
       ClusterInfoExpires_RefetchesClusterInfo) {
  feature_list_.Reset();
  feature_list_.InitAndEnableFeatureWithParameters(
      lens::features::kLensOverlayLatencyOptimizations,
      {{"enable-cluster-info-optimization", "true"}});

  // Prep the Primary account.
  signin::IdentityTestEnvironment identity_test_env;
  const AccountInfo primary_account_info =
      identity_test_env.MakePrimaryAccountAvailable(
          kFakePrimaryUsername, signin::ConsentLevel::kSignin);
  EXPECT_TRUE(identity_test_env.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(), identity_test_env.identity_manager(),
      profile(), lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId2);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());

  // Wait for the access token request for the cluster info to be sent.
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kFakeOAuthToken, base::Time::Max());
  // Wait for the access token request for the full image request to be sent.
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kFakeOAuthToken, base::Time::Max());

  // Wait for the cluster info request to be sent.
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());
  EXPECT_EQ(1, query_controller.num_cluster_info_fetch_requests_sent());

  // Reset the cluster info state.
  query_controller.ResetRequestClusterInfoStateForTesting();
  full_image_response_future.Clear();

  // Send interaction to trigger new query flow.
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(
      kTestTime, std::move(region), lens::REGION_SEARCH,
      additional_search_query_params, std::nullopt);

  // Wait for the access token request for the interaction request to be sent.
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kFakeOAuthToken, base::Time::Max());
  // Wait for the access token request for the cluster info request to be sent.
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kFakeOAuthToken, base::Time::Max());

  // Wait for the cluster info request to be sent.
  ASSERT_TRUE(url_response_future.Wait());
  EXPECT_EQ(2, query_controller.num_cluster_info_fetch_requests_sent());
  query_controller.EndQuery();
}

TEST_F(LensOverlayQueryControllerTest,
       FetchInitialQuery_SuggestInputsHaveFlagValues) {
  feature_list_.Reset();
  feature_list_.InitWithFeaturesAndParameters(
      {{lens::features::kLensOverlayLatencyOptimizations,
        {{"enable-early-interaction-optimization", "true"},
         {"enable-cluster-info-optimization", "true"}}},
       {lens::features::kLensOverlayContextualSearchbox, {}},
       {lens::features::kLensAimSuggestions,
        {{"aim-suggestions-type", "None"}}},
       {lens::features::kLensOverlaySuggestionsMigration,
        {{"send-lens-visual-interaction-data-for-lens-suggest", "false"}}}},
      {});

  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(), base::NullCallback(),
      base::NullCallback(), base::NullCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());

  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());
  query_controller.EndQuery();

  const auto& latest_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_TRUE(
      latest_suggest_inputs.send_gsession_vsrid_for_contextual_suggest());
  ASSERT_FALSE(
      latest_suggest_inputs.send_gsession_vsrid_vit_for_lens_suggest());
  ASSERT_FALSE(latest_suggest_inputs.send_vsint_for_lens_suggest());
}

// Tests that the query controller attaches the server session id from the
// cluster info response to the interaction request, even if the full image
// request included a different server session id.
TEST_F(LensOverlayQueryControllerTest,
       FetchInteraction_UsesClusterInfoResponse) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId2);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());

  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(
      kTestTime, std::move(region), lens::REGION_SEARCH,
      additional_search_query_params, std::nullopt);

  ASSERT_TRUE(url_response_future.Wait());
  query_controller.EndQuery();

  // Check the server session id is attached to the fetch url.
  std::string session_id_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(query_controller.sent_fetch_url(),
                                         kSessionIdQueryParameterKey,
                                         &session_id_value));
  ASSERT_EQ(session_id_value, kTestServerSessionId);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchRegionSearchInteraction_ReturnsResponses) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(25, 50, 30, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(
      kTestTime, std::move(region), lens::REGION_SEARCH,
      additional_search_query_params, std::nullopt);

  // Wait for async flows to complete.
  ASSERT_TRUE(url_response_future.Wait());
  WaitForSuggestInputsWithEncodedImageSignals(&query_controller);
  query_controller.EndQuery();

  std::string unused_client_upload_duration;
  bool has_client_upload_duration = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kClientUploadDurationQueryParameter, &unused_client_upload_duration);
  std::string unused_query_submission_time;
  bool has_query_submission_time = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kQuerySubmissionTimeQueryParameter,
      &unused_query_submission_time);

  ASSERT_TRUE(full_image_response_future.IsReady());

  // Check the initial fetch objects request.
  auto sent_object_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(sent_object_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(sent_object_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::REGION_SEARCH);

  const auto& latest_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_EQ(latest_suggest_inputs.encoded_image_signals(), kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs.search_session_id(), kTestSearchSessionId);
  ASSERT_TRUE(
      latest_suggest_inputs.has_encoded_visual_search_interaction_log_data());
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs.encoded_request_id());
  ASSERT_EQ(sent_object_request.request_context().request_id().sequence_id(),
            1);

  // Verify the interaction request.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().media_type(),
      lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .selection_metadata()
                .region()
                .region()
                .center_x(),
            25);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .selection_metadata()
                .region()
                .region()
                .center_y(),
            50);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_x(),
      0.25);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_y(),
      0.50);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_query_metadata());
  ASSERT_TRUE(has_client_upload_duration);
  ASSERT_TRUE(has_query_submission_time);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialFullPageObjectsRequestSent),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInteractionRequestFetchLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialInteractionRequestSent),
            1);
  CheckGen204IdsMatch(query_controller.sent_client_logs(),
                      url_response_future.Get());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchRegionSearchInteraction_ReturnsResponsesOptimizedClusterInfoFlow) {
  feature_list_.Reset();
  feature_list_.InitAndEnableFeatureWithParameters(
      lens::features::kLensOverlayLatencyOptimizations,
      {{"enable-early-interaction-optimization", "true"},
       {"enable-cluster-info-optimization", "true"}});
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.set_disable_next_objects_response(true);
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());

  ASSERT_EQ(query_controller.num_cluster_info_fetch_requests_sent(), 1);
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(
      kTestTime, std::move(region), lens::REGION_SEARCH,
      additional_search_query_params, std::nullopt);
  ASSERT_TRUE(url_response_future.Wait());
  query_controller.EndQuery();

  // Despite the full image response not being ready, the search url should
  // already start loading because the cluster info is available.
  ASSERT_FALSE(full_image_response_future.IsReady());
  ASSERT_TRUE(url_response_future.IsReady());

  // Check the search session id is attached to the fetch url.
  std::string session_id_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                         kSessionIdQueryParameterKey,
                                         &session_id_value));
  ASSERT_EQ(session_id_value, kTestSearchSessionId);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchRegionSearchInteractionWithBytes_ReturnsResponse) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap viewport_bitmap = CreateNonEmptyBitmap(1000, 1000);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      viewport_bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  SkBitmap region_bitmap = CreateNonEmptyBitmap(100, 100);
  region_bitmap.setAlphaType(kOpaque_SkAlphaType);
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(125, 125, 100, 100);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(
      kTestTime, std::move(region), lens::REGION_SEARCH,
      additional_search_query_params,
      std::make_optional<SkBitmap>(region_bitmap));

  ASSERT_TRUE(url_response_future.Wait());
  WaitForSuggestInputsWithEncodedImageSignals(&query_controller);
  query_controller.EndQuery();

  std::string unused_client_upload_duration;
  bool has_client_upload_duration = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kClientUploadDurationQueryParameter, &unused_client_upload_duration);
  std::string unused_query_submission_time;
  bool has_query_submission_time = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kQuerySubmissionTimeQueryParameter,
      &unused_query_submission_time);
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  ASSERT_TRUE(has_vsint);

  ASSERT_TRUE(full_image_response_future.IsReady());

  // Check initial fetch objects request is correct.
  auto sent_object_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(sent_object_request.image_data().image_metadata().width(), 1000);
  ASSERT_EQ(sent_object_request.image_data().image_metadata().height(), 1000);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::REGION_SEARCH);
  const auto& lens_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_EQ(lens_suggest_inputs.encoded_image_signals(), kTestSuggestSignals);
  ASSERT_EQ(lens_suggest_inputs.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(lens_suggest_inputs.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            lens_suggest_inputs.encoded_request_id());
  ASSERT_EQ(sent_object_request.request_context().request_id().sequence_id(),
            1);

  // Verify the interaction request.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().media_type(),
      lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .selection_metadata()
                .region()
                .region()
                .center_x(),
            125);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .selection_metadata()
                .region()
                .region()
                .center_y(),
            125);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_x(),
      0.125);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_y(),
      0.125);
  ASSERT_EQ(GetExpectedJpegBytesForBitmap(region_bitmap),
            sent_interaction_request.image_crop().image().image_content());
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_query_metadata());
  ASSERT_TRUE(has_client_upload_duration);
  ASSERT_TRUE(has_query_submission_time);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  CheckGen204IdsMatch(query_controller.sent_client_logs(),
                      url_response_future.Get());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchMultimodalSearchInteraction_ReturnsResponses) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(25, 50, 30, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendMultimodalRequest(
      kTestTime, std::move(region), kTestQueryText, lens::MULTIMODAL_SEARCH,
      additional_search_query_params, std::nullopt);
  ASSERT_TRUE(url_response_future.Wait());
  WaitForSuggestInputsWithEncodedImageSignals(&query_controller);
  query_controller.EndQuery();

  std::string unused_client_upload_duration;
  bool has_client_upload_duration = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kClientUploadDurationQueryParameter, &unused_client_upload_duration);
  std::string unused_query_submission_time;
  bool has_query_submission_time = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kQuerySubmissionTimeQueryParameter,
      &unused_query_submission_time);
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  ASSERT_TRUE(has_vsint);

  ASSERT_TRUE(full_image_response_future.IsReady());

  // Check initial fetch objects request is correct.
  auto sent_object_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(sent_object_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(sent_object_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::MULTIMODAL_SEARCH);

  const auto latest_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_EQ(latest_suggest_inputs.encoded_image_signals(), kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs.encoded_request_id());
  ASSERT_EQ(sent_object_request.request_context().request_id().sequence_id(),
            1);

  // Verify the interaction request.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().media_type(),
      lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .selection_metadata()
                .region()
                .region()
                .center_x(),
            25);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .selection_metadata()
                .region()
                .region()
                .center_y(),
            50);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_x(),
      0.25);
  ASSERT_EQ(
      sent_interaction_request.image_crop().zoomed_crop().crop().center_y(),
      0.50);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_TRUE(has_client_upload_duration);
  ASSERT_TRUE(has_query_submission_time);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  CheckGen204IdsMatch(query_controller.sent_client_logs(),
                      url_response_future.Get());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchTextOnlyInteraction_ReturnsResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  query_controller.SendTextOnlyQuery(
      kTestTime, "", lens::LensOverlaySelectionType::SELECT_TEXT_HIGHLIGHT,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());
  query_controller.EndQuery();

  std::string actual_encoded_video_context;
  net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                             kVideoContextParamKey,
                             &actual_encoded_video_context);

  std::string unused_client_upload_duration;
  bool has_client_upload_duration = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kClientUploadDurationQueryParameter, &unused_client_upload_duration);
  std::string unused_query_submission_time;
  bool has_query_submission_time = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kQuerySubmissionTimeQueryParameter,
      &unused_query_submission_time);

  auto vsint = GetVsintFromUrl(url_response_future.Get().url());
  ASSERT_EQ(vsint.object_id(), "");
  ASSERT_FALSE(vsint.has_zoomed_crop());
  ASSERT_EQ(vsint.interaction_type(),
            lens::LensOverlayInteractionRequestMetadata::TEXT_SELECTION);

  const auto latest_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_TRUE(url_response_future.IsReady());
  ASSERT_FALSE(latest_suggest_inputs.has_encoded_image_signals());
  ASSERT_EQ(vsint.log_data().user_selection_data().selection_type(),
            lens::SELECT_TEXT_HIGHLIGHT);
  ASSERT_FALSE(latest_suggest_inputs.has_contextual_visual_input_type());
  ASSERT_TRUE(has_client_upload_duration);
  ASSERT_TRUE(has_query_submission_time);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchTextOnlyInteractionWithPdfBytes_ReturnsResponse) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakePdfPageContents,
      lens::MimeType::kPdf, /*pdf_current_page=*/std::nullopt, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());
  WaitForSuggestInputsWithEncodedImageSignals(&query_controller);
  query_controller.EndQuery();

  ASSERT_TRUE(full_image_response_future.IsReady());

  // Verify the content bytes were not included with the image bytes request.
  auto full_image_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(full_image_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(full_image_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(full_image_request.payload().content_data().empty());

  // Verify the content bytes were included in a followup request.
  auto page_content_request =
      query_controller.sent_page_content_objects_request();
  ASSERT_TRUE(page_content_request.payload().content_data().empty());
  ASSERT_TRUE(page_content_request.payload().has_content());
  ASSERT_EQ(page_content_request.payload().content().content_data().size(), 1);
  ASSERT_FALSE(
      page_content_request.payload().content().content_data(0).data().empty());
  ASSERT_EQ(
      page_content_request.payload().content().content_data(0).content_type(),
      lens::ContentData::CONTENT_TYPE_PDF);

  // Verify the page url was included in the request.
  ASSERT_EQ(page_content_request.payload().content().webpage_url(),
            kTestPageUrl);

  // The full image and page content requests should have the same request id.
  ASSERT_EQ(full_image_request.request_context().request_id().sequence_id(),
            page_content_request.request_context().request_id().sequence_id());

  // Check interaction request is correct.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().media_type(),
      lens::LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::PDF_QUERY);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_selection_metadata());

  // Check search URL is correct.
  ASSERT_TRUE(url_response_future.IsReady());
  std::string unused_client_upload_duration;
  bool has_client_upload_duration = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kClientUploadDurationQueryParameter, &unused_client_upload_duration);
  std::string unused_query_submission_time;
  bool has_query_submission_time = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kQuerySubmissionTimeQueryParameter,
      &unused_query_submission_time);
  std::string visual_input_type;
  bool has_visual_input_type = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kVisualInputTypeParameterKey,
      &visual_input_type);
  std::string invocation_source;
  bool has_invocation_source = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kInvocationSourceParameterKey,
      &invocation_source);
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  ASSERT_TRUE(has_vsint);
  const auto latest_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::MULTIMODAL_SEARCH);
  ASSERT_TRUE(has_client_upload_duration);
  ASSERT_TRUE(has_query_submission_time);
  ASSERT_TRUE(has_visual_input_type);
  ASSERT_EQ(visual_input_type, "pdf");
  ASSERT_TRUE(has_invocation_source);
  ASSERT_EQ(invocation_source, "chrome.cr.menu");
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kPageContentUploadLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(latest_suggest_inputs.encoded_image_signals(), kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(latest_suggest_inputs.contextual_visual_input_type(), "pdf");
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs.encoded_request_id());
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialPageContentRequestSent),
            1);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchTextOnlyInteractionWithApc_ReturnsResponse) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeApcPageContents,
      lens::MimeType::kAnnotatedPageContent, /*pdf_current_page=*/std::nullopt,
      0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());
  WaitForSuggestInputsWithEncodedImageSignals(&query_controller);
  query_controller.EndQuery();

  ASSERT_TRUE(full_image_response_future.IsReady());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  // Verify the content bytes were not included with the image bytes request.
  auto full_image_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(full_image_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(full_image_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(full_image_request.payload().content_data().empty());

  // Verify the content bytes were included in a followup request.
  auto page_content_request =
      query_controller.sent_page_content_objects_request();
  ASSERT_TRUE(page_content_request.payload().content_data().empty());
  ASSERT_TRUE(page_content_request.payload().has_content());
  ASSERT_EQ(page_content_request.payload().content().content_data().size(), 1);
  ASSERT_FALSE(
      page_content_request.payload().content().content_data(0).data().empty());
  ASSERT_EQ(
      page_content_request.payload().content().content_data(0).content_type(),
      lens::ContentData::CONTENT_TYPE_ANNOTATED_PAGE_CONTENT);

  // Verify the page url was included in the request.
  ASSERT_EQ(page_content_request.payload().content().webpage_url(),
            kTestPageUrl);

  // The full image and page content requests should have the same request id.
  ASSERT_EQ(full_image_request.request_context().request_id().sequence_id(),
            page_content_request.request_context().request_id().sequence_id());

  // Check interaction request is correct.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().media_type(),
      lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::WEBPAGE_QUERY);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_selection_metadata());

  // Check search URL is correct.
  ASSERT_TRUE(url_response_future.IsReady());
  std::string unused_client_upload_duration;
  bool has_client_upload_duration = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kClientUploadDurationQueryParameter, &unused_client_upload_duration);
  std::string unused_query_submission_time;
  bool has_query_submission_time = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kQuerySubmissionTimeQueryParameter,
      &unused_query_submission_time);
  std::string visual_input_type;
  bool has_visual_input_type = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kVisualInputTypeParameterKey,
      &visual_input_type);
  std::string invocation_source;
  bool has_invocation_source = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kInvocationSourceParameterKey,
      &invocation_source);
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  const auto latest_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_TRUE(has_vsint);
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::MULTIMODAL_SEARCH);
  ASSERT_TRUE(has_client_upload_duration);
  ASSERT_TRUE(has_query_submission_time);
  ASSERT_TRUE(has_visual_input_type);
  ASSERT_EQ(visual_input_type, "wp");
  ASSERT_TRUE(has_invocation_source);
  ASSERT_EQ(invocation_source, "chrome.cr.menu");
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kPageContentUploadLatency),
            1);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(latest_suggest_inputs.encoded_image_signals(), kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(latest_suggest_inputs.contextual_visual_input_type(), "wp");
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs.encoded_request_id());
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialPageContentRequestSent),
            1);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchTextOnlyInteractionWithHtmlInnerText_ReturnsResponse) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeTextPageContents,
      lens::MimeType::kPlainText, /*pdf_current_page=*/std::nullopt, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());
  WaitForSuggestInputsWithEncodedImageSignals(&query_controller);
  query_controller.EndQuery();

  ASSERT_TRUE(full_image_response_future.IsReady());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  // Verify the content bytes were not included with the image bytes request.
  auto full_image_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(full_image_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(full_image_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(full_image_request.payload().content_data().empty());

  // Verify the content bytes were included in a followup request.
  auto page_content_request =
      query_controller.sent_page_content_objects_request();
  ASSERT_TRUE(page_content_request.payload().content_data().empty());
  ASSERT_TRUE(page_content_request.payload().has_content());
  ASSERT_EQ(page_content_request.payload().content().content_data().size(), 1);
  ASSERT_FALSE(
      page_content_request.payload().content().content_data(0).data().empty());
  ASSERT_EQ(
      page_content_request.payload().content().content_data(0).content_type(),
      lens::ContentData::CONTENT_TYPE_INNER_TEXT);

  // Verify the page url was included in the request.
  ASSERT_EQ(page_content_request.payload().content().webpage_url(),
            kTestPageUrl);

  // The full image and page content requests should have the same request id.
  ASSERT_EQ(full_image_request.request_context().request_id().sequence_id(),
            page_content_request.request_context().request_id().sequence_id());

  // Check interaction request is correct.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::WEBPAGE_QUERY);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_selection_metadata());

  // Check search URL is correct.
  ASSERT_TRUE(url_response_future.IsReady());
  std::string unused_client_upload_duration;
  bool has_client_upload_duration = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kClientUploadDurationQueryParameter, &unused_client_upload_duration);
  std::string unused_query_submission_time;
  bool has_query_submission_time = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kQuerySubmissionTimeQueryParameter,
      &unused_query_submission_time);
  std::string visual_input_type;
  bool has_visual_input_type = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kVisualInputTypeParameterKey,
      &visual_input_type);
  std::string invocation_source;
  bool has_invocation_source = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kInvocationSourceParameterKey,
      &invocation_source);
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  const auto latest_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_TRUE(has_vsint);
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::MULTIMODAL_SEARCH);
  ASSERT_TRUE(has_client_upload_duration);
  ASSERT_TRUE(has_query_submission_time);
  ASSERT_TRUE(has_visual_input_type);
  ASSERT_EQ(visual_input_type, "wp");
  ASSERT_TRUE(has_invocation_source);
  ASSERT_EQ(invocation_source, "chrome.cr.menu");
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kPageContentUploadLatency),
            1);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(latest_suggest_inputs.encoded_image_signals(), kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(latest_suggest_inputs.contextual_visual_input_type(), "wp");
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs.encoded_request_id());
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialPageContentRequestSent),
            1);
}

TEST_F(LensOverlayQueryControllerTest,
       SendContextualTextQuery_WithNoPartialUpload_HoldsRequest) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  // Disable the upload progress callback so the test can control when it
  // completes.
  query_controller.set_disable_page_upload_response_callback(true);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeTextPageContents,
      lens::MimeType::kPlainText, /*pdf_current_page=*/std::nullopt, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  // Send an empty partial page content request.
  query_controller.SendPartialPageContentRequest({});

  // Send the contextual text query.
  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);

  // Verify the query controller did not issue the search request yet.
  EXPECT_FALSE(url_response_future.IsReady());

  // Simulate the upload progress callback completing the upload. Verify the
  // query controller issued the search request after the upload completed.
  query_controller.RunUploadProgressCallback();
  ASSERT_TRUE(url_response_future.Wait());

  query_controller.EndQuery();
}

TEST_F(LensOverlayQueryControllerTest,
       SendContextualTextQuery_WithScannedPdfPartialUpload_HoldsRequest) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  // Disable the upload progress callback so the test can control when it
  // completes.
  query_controller.set_disable_page_upload_response_callback(true);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeTextPageContents,
      lens::MimeType::kPlainText, /*pdf_current_page=*/std::nullopt, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  // Send a partial page content request that does not meet the per page
  // character limit to be considered as a non-scanned pdf. This emulates the
  // case where the users PDF is scanned and therefore the search query should
  // be held until the page content upload is finished.
  query_controller.SendPartialPageContentRequest(kShortPartialContent);

  // Send the contextual text query.
  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);

  // Verify the query controller did not issue the search request yet.
  EXPECT_FALSE(url_response_future.IsReady());

  // Simulate the upload progress callback completing the upload. Verify the
  // query controller issued the search request after the upload completed.
  query_controller.RunUploadProgressCallback();
  ASSERT_TRUE(url_response_future.Wait());

  query_controller.EndQuery();
}

TEST_F(
    LensOverlayQueryControllerTest,
    SendPartialPageContentRequest_WithInsubstantialPdfPartialUpload_DoesntSendRequest) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeTextPageContents,
      lens::MimeType::kPlainText, /*pdf_current_page=*/std::nullopt, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  // Send a partial page content request that does not meet the per page
  // character limit to be considered as a non-scanned pdf. This emulates the
  // case where the users PDF is scanned and therefore the partial page content
  // request should not be sent.
  query_controller.SendPartialPageContentRequest(kShortPartialContent);

  // If there is a partial page content request start time, there is a partial
  // page content request in progress and the test should fail.
  EXPECT_TRUE(
      query_controller.partial_page_contents_request_start_time_for_testing()
          .is_null());

  query_controller.EndQuery();
}

TEST_F(LensOverlayQueryControllerTest,
       SendContextualTextQuery_WithFullTextPdfPartialUpload_SendsRequest) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  // Disable the upload progress callback so the test can control when it
  // completes.
  query_controller.set_disable_page_upload_response_callback(true);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeTextPageContents,
      lens::MimeType::kPlainText, /*pdf_current_page=*/std::nullopt, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  // Send a partial page content request that does meet the per page
  // character limit to be considered as a non-scanned pdf.
  query_controller.SendPartialPageContentRequest(kLongPartialContent);

  // Send the contextual text query.
  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);

  // Verify the query controller did issue the search request.
  ASSERT_TRUE(url_response_future.Wait());

  query_controller.EndQuery();
}

TEST_F(LensOverlayQueryControllerTest,
       SendUpdatedPageContent_IncrementsSequence) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());

  // Immediately send a page content update request.
  query_controller.SendUpdatedPageContent(
      kFakeTextPageContents, lens::MimeType::kPlainText, GURL(kTestPageUrl),
      kTestPageTitle, std::nullopt, SkBitmap());

  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_page_content_update_requests_sent() == 1;
  }));

  // The full image and page content requests should have the same request id.
  ASSERT_EQ(query_controller.sent_full_image_request_id().sequence_id(), 1);
  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            1);

  // Send a new page content update request.
  query_controller.SendUpdatedPageContent(
      kNewFakeTextPageContents, lens::MimeType::kPlainText, GURL(kTestPageUrl),
      kTestPageTitle, std::nullopt, SkBitmap());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_page_content_update_requests_sent() == 2;
  }));

  // The new page content request should have a different sequence ID.
  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            1);
  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            2);

  // Send an additional page content update request with a partial page content
  // request.
  query_controller.SendUpdatedPageContent(
      kNewFakeTextPageContents, lens::MimeType::kPlainText, GURL(kTestPageUrl),
      kTestPageTitle, std::nullopt, SkBitmap());
  query_controller.SendPartialPageContentRequest(kLongPartialContent);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_page_content_update_requests_sent() == 3;
  }));

  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            1);
  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            3);
  ASSERT_EQ(query_controller.sent_partial_page_content_objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            1);
  ASSERT_EQ(query_controller.sent_partial_page_content_objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            4);

  // Send a contextual search query.
  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return query_controller.num_interaction_requests_sent() == 1; }));

  ASSERT_EQ(query_controller.sent_interaction_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            1);
  ASSERT_EQ(query_controller.sent_interaction_request()
                .request_context()
                .request_id()
                .sequence_id(),
            5);
}

TEST_F(LensOverlayQueryControllerTest, FullCsbRequestFlow) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      lens::features::kLensOverlayContextualSearchbox);

  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());

  // Immediately send a page content update request.
  query_controller.SendUpdatedPageContent(
      kFakeTextPageContents, lens::MimeType::kPlainText, GURL(kTestPageUrl),
      kTestPageTitle, std::nullopt, SkBitmap());

  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_page_content_update_requests_sent() == 1;
  }));

  // The full image and page content requests should have the same request id.
  auto last_page_content_request_id =
      query_controller.sent_page_content_objects_request()
          .request_context()
          .request_id();
  ASSERT_EQ(query_controller.sent_full_image_request_id().sequence_id(), 1);
  ASSERT_EQ(query_controller.sent_full_image_request_id().long_context_id(), 1);
  ASSERT_EQ(last_page_content_request_id.sequence_id(), 1);
  ASSERT_EQ(last_page_content_request_id.long_context_id(), 1);

  // Send a query.
  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());

  // The interaction request should have incremented the sequence id.
  ASSERT_EQ(query_controller.sent_interaction_request()
                .request_context()
                .request_id()
                .sequence_id(),
            2);

  // The URL should also have incremented the sequence id.
  ASSERT_EQ(GetRequestIdFromUrl(url_response_future.Take().url()).sequence_id(),
            3);

  // Send a new page content update request.
  query_controller.SendUpdatedPageContent(
      kNewFakeTextPageContents, lens::MimeType::kPlainText, GURL(kTestPageUrl),
      kTestPageTitle, std::nullopt, SkBitmap());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_page_content_update_requests_sent() == 2;
  }));

  // The new page content request should have incremented the sequence id and
  // long context id.
  last_page_content_request_id =
      query_controller.sent_page_content_objects_request()
          .request_context()
          .request_id();
  ASSERT_EQ(last_page_content_request_id.image_sequence_id(), 1);
  ASSERT_EQ(last_page_content_request_id.long_context_id(), 2);
  ASSERT_EQ(last_page_content_request_id.sequence_id(), 4);

  // Send another contextual search query.
  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());

  // The interaction request should have incremented the sequence id.
  ASSERT_EQ(query_controller.sent_interaction_request()
                .request_context()
                .request_id()
                .sequence_id(),
            5);

  // The URL should also have incremented the sequence id.
  ASSERT_EQ(GetRequestIdFromUrl(url_response_future.Take().url()).sequence_id(),
            6);

  // Send a new screenshot.
  full_image_response_future.Clear();
  SkBitmap bitmap2 = CreateNonEmptyBitmap(200, 100);
  query_controller.SendUpdatedPageContent(std::nullopt, std::nullopt,
                                          std::nullopt, std::nullopt,
                                          std::nullopt, bitmap2);
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  // Verify the request had a new image sequence id and sequence id.
  ASSERT_EQ(query_controller.sent_full_image_request_id().image_sequence_id(),
            2);
  ASSERT_EQ(query_controller.sent_full_image_request_id().long_context_id(), 2);
  ASSERT_EQ(query_controller.sent_full_image_request_id().sequence_id(), 7);

  // Send another contextual search query.
  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());

  // The interaction request should have incremented the sequence id.
  ASSERT_EQ(query_controller.sent_interaction_request()
                .request_context()
                .request_id()
                .sequence_id(),
            8);

  // The URL should also have incremented the sequence id.
  ASSERT_EQ(GetRequestIdFromUrl(url_response_future.Take().url()).sequence_id(),
            9);
}

TEST_F(LensOverlayQueryControllerTest,
       SendUpdatedPageContentWithScreenshot_IncrementsSequence) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());

  // Immediately send a page content update request.
  query_controller.SendUpdatedPageContent(
      kFakeTextPageContents, lens::MimeType::kPlainText, GURL(kTestPageUrl),
      kTestPageTitle, std::nullopt, SkBitmap());

  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_page_content_update_requests_sent() == 1;
  }));

  // The full image and page content requests should have the same request id.
  ASSERT_EQ(query_controller.sent_full_image_request_id().image_sequence_id(),
            1);
  ASSERT_EQ(query_controller.sent_full_image_request_id().sequence_id(), 1);
  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            1);
  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            1);

  // Send a new screenshot.
  full_image_response_future.Clear();
  SkBitmap bitmap2 = CreateNonEmptyBitmap(200, 100);
  query_controller.SendUpdatedPageContent(std::nullopt, std::nullopt,
                                          std::nullopt, std::nullopt,
                                          std::nullopt, bitmap2);
  ASSERT_TRUE(full_image_response_future.Wait());

  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return query_controller.num_full_image_requests_sent() == 2; }));

  // Screenshot should increment image sequence id and sequence id.
  ASSERT_EQ(query_controller.sent_full_image_request_id().image_sequence_id(),
            2);
  ASSERT_EQ(query_controller.sent_full_image_request_id().sequence_id(), 2);

  // Send an additional page content update request.
  query_controller.SendUpdatedPageContent(
      kFakeTextPageContents, lens::MimeType::kPlainText, GURL(kTestPageUrl),
      kTestPageTitle, std::nullopt, SkBitmap());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_page_content_update_requests_sent() == 2;
  }));

  // Page content update should only increment image sequence id.
  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            2);
  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            3);

  // Send screenshot and additional page content update request together.
  full_image_response_future.Clear();
  SkBitmap bitmap3 = CreateNonEmptyBitmap(300, 100);
  query_controller.SendUpdatedPageContent(
      kFakeTextPageContents, lens::MimeType::kPlainText, GURL(kTestPageUrl),
      kTestPageTitle, std::nullopt, bitmap3);
  ASSERT_TRUE(full_image_response_future.Wait());

  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_page_content_update_requests_sent() == 3;
  }));

  // Screenshot should increment image sequence id and sequence id.
  ASSERT_EQ(query_controller.sent_full_image_request_id().image_sequence_id(),
            3);
  ASSERT_EQ(query_controller.sent_full_image_request_id().sequence_id(), 4);
  // Page content update should only increment image sequence id.
  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            3);
  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            5);

  // Send a contextual search query.
  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return query_controller.num_interaction_requests_sent() == 1; }));

  ASSERT_EQ(query_controller.sent_interaction_request()
                .request_context()
                .request_id()
                .image_sequence_id(),
            3);
  ASSERT_EQ(query_controller.sent_interaction_request()
                .request_context()
                .request_id()
                .sequence_id(),
            6);
  query_controller.EndQuery();
}

TEST_F(LensOverlayQueryControllerTest,
       SendUpdatedPageContentWithScreenshot_SendsPageNumber) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/123, 0, base::TimeTicks::Now());

  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());
  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return query_controller.num_full_image_requests_sent() == 1; }));

  // Image request should have page number.
  auto full_image_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(full_image_request.viewport_request_context().pdf_page_number(),
            123);

  // Send a new screenshot.
  full_image_response_future.Clear();
  SkBitmap bitmap2 = CreateNonEmptyBitmap(200, 100);
  query_controller.SendUpdatedPageContent(
      std::nullopt, std::nullopt, std::nullopt, std::nullopt, 234, bitmap2);
  ASSERT_TRUE(full_image_response_future.Wait());
  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return query_controller.num_full_image_requests_sent() == 2; }));

  // Image request should have page number.
  full_image_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(full_image_request.viewport_request_context().pdf_page_number(),
            234);

  query_controller.EndQuery();
}

TEST_F(LensOverlayQueryControllerTest,
       PageContentRequest_MultiplePageContents_SendProperRequest) {
  InitFeaturesWithNewContentPayload();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      kFakeHtmlPageContentsWithMultipleContents, lens::MimeType::kHtml,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_page_content_update_requests_sent() == 1;
  }));
  query_controller.EndQuery();

  // Verify that the payload has 3 sets of data
  auto page_content_request =
      query_controller.sent_page_content_objects_request();
  ASSERT_TRUE(page_content_request.payload().content_data().empty());
  ASSERT_TRUE(page_content_request.payload().has_content());
  EXPECT_EQ(3, page_content_request.payload().content().content_data().size());

  // Verify the payload has a URL and title.
  const auto sent_content = page_content_request.payload().content();
  EXPECT_EQ(sent_content.webpage_url(), kTestPageUrl);
  EXPECT_EQ(sent_content.webpage_title(), kTestPageTitle);

  // Verify the first is the first bytes with the correct content type
  EXPECT_EQ(sent_content.content_data(0).data().size(),
            kFakeContentBytes.size());
  EXPECT_EQ(sent_content.content_data(0).content_type(),
            lens::ContentData::CONTENT_TYPE_INNER_HTML);

  // Verify the second is the second bytes with the correct content type
  EXPECT_EQ(sent_content.content_data(1).data().size(),
            kFakeContentBytes2.size());
  EXPECT_EQ(sent_content.content_data(1).content_type(),
            lens::ContentData::CONTENT_TYPE_INNER_TEXT);

  // Verify the third is the third bytes with the correct content type
  EXPECT_EQ(sent_content.content_data(2).data().size(),
            kNewFakeContentBytes.size());
  EXPECT_EQ(sent_content.content_data(2).content_type(),
            lens::ContentData::CONTENT_TYPE_ANNOTATED_PAGE_CONTENT);
}

TEST_F(LensOverlayQueryControllerTest,
       SendPartialPageContentRequest_SendsRequest) {
  InitFeaturesWithClusterInfoOptimization();
  TestLensOverlayQueryController query_controller(
      /**full_image_callback=*/base::DoNothing(),
      /**url_callback=*/base::DoNothing(),
      /**interaction_callback=*/base::NullCallback(),
      /**thumbnail_created_callback=*/base::DoNothing(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the server responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakePdfPageContents,
      lens::MimeType::kPdf, /*pdf_current_page=*/std::nullopt, 0,
      base::TimeTicks::Now());

  const std::vector<std::u16string> kFakeSubstantialPartialContent(
      {u"this is a page with enough content to make it substantial",
       u"this is a second page substantial enought content"});
  query_controller.SendPartialPageContentRequest(
      kFakeSubstantialPartialContent);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_partial_page_content_requests_sent() == 1;
  }));

  // Check the request is correct.
  auto sent_request =
      query_controller.sent_partial_page_content_objects_request();
  ASSERT_EQ(1, sent_request.request_context().request_id().sequence_id());
  ASSERT_EQ(lens::RequestType::REQUEST_TYPE_EARLY_PARTIAL_PDF,
            sent_request.payload().request_type());
  ASSERT_TRUE(sent_request.payload().has_content());
  EXPECT_EQ(sent_request.payload().content().content_data().size(), 1);

  auto partial_pdf_data = sent_request.payload().content().content_data(0);
  EXPECT_EQ(partial_pdf_data.content_type(),
            lens::ContentData::CONTENT_TYPE_EARLY_PARTIAL_PDF);
  lens::LensOverlayDocument partial_pdf_document;
  partial_pdf_document.ParseFromString(partial_pdf_data.data());

  ASSERT_EQ(2, partial_pdf_document.pages_size());
  ASSERT_EQ(1, partial_pdf_document.pages(0).page_number());
  ASSERT_EQ("this is a page with enough content to make it substantial",
            partial_pdf_document.pages(0).text_segments(0));
  ASSERT_EQ(2, partial_pdf_document.pages(1).page_number());
  ASSERT_EQ("this is a second page substantial enought content",
            partial_pdf_document.pages(1).text_segments(0));

  // Send a new page content request to incremement the sequence id.
  query_controller.SendUpdatedPageContent(
      kFakePdfPageContents, lens::MimeType::kPdf, GURL(kTestPageUrl),
      kTestPageTitle, 123, SkBitmap());

  // Send a new request.
  const std::vector<std::u16string> kNewFakeSubstantialPartialContent(
      {u"this is a new page1 with substantial enough content to be sent"});
  query_controller.SendPartialPageContentRequest(
      kNewFakeSubstantialPartialContent);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_partial_page_content_requests_sent() == 2;
  }));

  // Check the request is correct.
  sent_request = query_controller.sent_partial_page_content_objects_request();
  ASSERT_EQ(3, sent_request.request_context().request_id().sequence_id());
  ASSERT_EQ(lens::RequestType::REQUEST_TYPE_EARLY_PARTIAL_PDF,
            sent_request.payload().request_type());

  partial_pdf_data = sent_request.payload().content().content_data(0);
  EXPECT_EQ(partial_pdf_data.content_type(),
            lens::ContentData::CONTENT_TYPE_EARLY_PARTIAL_PDF);
  partial_pdf_document.ParseFromString(partial_pdf_data.data());

  ASSERT_EQ(1, partial_pdf_document.pages_size());
  ASSERT_EQ(1, partial_pdf_document.pages(0).page_number());
  ASSERT_EQ("this is a new page1 with substantial enough content to be sent",
            partial_pdf_document.pages(0).text_segments(0));

  // Send an empty request.
  const std::vector<std::u16string> kEmptyPartialContent({});
  query_controller.ResetPageContentData();
  query_controller.SendPartialPageContentRequest(kEmptyPartialContent);

  // Check that no request is not sent.
  query_controller.EndQuery();
  ASSERT_EQ(query_controller.num_partial_page_content_requests_sent(), 2);
}

TEST_F(LensOverlayQueryControllerTest,
       SendPartialPageContentRequest_UpdatedContentFields_SendsRequest) {
  feature_list_.Reset();
  base::FieldTrialParams params =
      kDefaultLensOverlayContextualSearchboxParams.params;
  params["use-updated-content-fields"] = "true";
  feature_list_.InitWithFeaturesAndParameters(
      {{lens::features::kLensOverlayLatencyOptimizations,
        {{"enable-cluster-info-optimization", "true"}}},
       {lens::features::kLensOverlayContextualSearchbox, params}},
      {});
  TestLensOverlayQueryController query_controller(
      /**full_image_callback=*/base::DoNothing(),
      //   full_image_response_future.GetRepeatingCallback(),
      /**url_callback=*/base::DoNothing(),
      /**interaction_callback=*/base::NullCallback(),
      /**thumbnail_created_callback=*/base::DoNothing(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the server responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakePdfPageContents,
      lens::MimeType::kPdf, /*pdf_current_page=*/std::nullopt, 0,
      base::TimeTicks::Now());

  const std::vector<std::u16string> kFakeSubstantialPartialContent(
      {u"this is a page with enough content to make it substantial",
       u"this is a second page substantial enought content"});
  query_controller.SendPartialPageContentRequest(
      kFakeSubstantialPartialContent);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_partial_page_content_requests_sent() == 1;
  }));

  // Check the request is correct.
  auto sent_request =
      query_controller.sent_partial_page_content_objects_request();
  ASSERT_EQ(1, sent_request.request_context().request_id().sequence_id());
  ASSERT_EQ(lens::RequestType::REQUEST_TYPE_EARLY_PARTIAL_PDF,
            sent_request.payload().request_type());
  ASSERT_TRUE(sent_request.payload().content_data().empty());
  ASSERT_TRUE(sent_request.payload().has_content());
  EXPECT_EQ(1, sent_request.payload().content().content_data().size());
  auto sent_content = sent_request.payload().content();
  EXPECT_EQ(sent_content.content_data(0).content_type(),
            lens::ContentData::CONTENT_TYPE_EARLY_PARTIAL_PDF);
  lens::LensOverlayDocument partial_pdf_document;
  EXPECT_TRUE(partial_pdf_document.ParseFromString(
      sent_content.content_data(0).data()));
  ASSERT_EQ(2, partial_pdf_document.pages_size());
  ASSERT_EQ(1, partial_pdf_document.pages(0).page_number());
  ASSERT_EQ("this is a page with enough content to make it substantial",
            partial_pdf_document.pages(0).text_segments(0));
  ASSERT_EQ(2, partial_pdf_document.pages(1).page_number());
  ASSERT_EQ("this is a second page substantial enought content",
            partial_pdf_document.pages(1).text_segments(0));

  // Send a new page content request to increment the sequence id.
  query_controller.SendUpdatedPageContent(
      kFakePdfPageContents, lens::MimeType::kPdf, GURL(kTestPageUrl),
      kTestPageTitle, std::nullopt, SkBitmap());

  // Send a new request.
  const std::vector<std::u16string> kNewFakeSubstantialPartialContent(
      {u"this is a new page1 with substantial enough content to be sent"});
  query_controller.SendPartialPageContentRequest(
      kNewFakeSubstantialPartialContent);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_partial_page_content_requests_sent() == 2;
  }));

  // Check the request is correct.
  sent_request = query_controller.sent_partial_page_content_objects_request();
  ASSERT_EQ(3, sent_request.request_context().request_id().sequence_id());
  ASSERT_EQ(lens::RequestType::REQUEST_TYPE_EARLY_PARTIAL_PDF,
            sent_request.payload().request_type());
  ASSERT_TRUE(sent_request.payload().content_data().empty());
  ASSERT_TRUE(sent_request.payload().has_content());
  EXPECT_EQ(1, sent_request.payload().content().content_data().size());
  sent_content = sent_request.payload().content();
  EXPECT_EQ(sent_content.content_data(0).content_type(),
            lens::ContentData::CONTENT_TYPE_EARLY_PARTIAL_PDF);
  EXPECT_TRUE(partial_pdf_document.ParseFromString(
      sent_content.content_data(0).data()));
  ASSERT_EQ(1, partial_pdf_document.pages_size());
  ASSERT_EQ(1, partial_pdf_document.pages(0).page_number());
  ASSERT_EQ("this is a new page1 with substantial enough content to be sent",
            partial_pdf_document.pages(0).text_segments(0));

  // Send an empty request.
  const std::vector<std::u16string> kEmptyPartialContent({});
  query_controller.ResetPageContentData();
  query_controller.SendPartialPageContentRequest(kEmptyPartialContent);

  // Check that no request is sent.
  query_controller.EndQuery();
  ASSERT_EQ(query_controller.num_partial_page_content_requests_sent(), 2);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchInteraction_UsesSameAnalyticsIdForLensRequestAndUrl) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;

  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  ASSERT_TRUE(full_image_response_future.IsReady());
  std::string first_analytics_id =
      query_controller.sent_full_image_request_id().analytics_id();
  query_controller.SendRegionSearch(
      kTestTime, std::move(region), lens::REGION_SEARCH,
      additional_search_query_params, std::nullopt);
  ASSERT_TRUE(url_response_future.Wait());
  WaitForSuggestInputsWithEncodedImageSignals(&query_controller);
  query_controller.EndQuery();

  ASSERT_TRUE(url_response_future.IsReady());
  ASSERT_TRUE(
      query_controller.GetLensSuggestInputs().has_encoded_image_signals());
  std::string second_analytics_id =
      query_controller.sent_interaction_request_id().analytics_id();

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
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  // Check initial fetch objects request id is correct.
  ASSERT_TRUE(full_image_response_future.IsReady());
  auto initial_sent_object_request =
      query_controller.sent_full_image_objects_request();
  ASSERT_EQ(initial_sent_object_request.request_context()
                .request_id()
                .image_sequence_id(),
            1);
  ASSERT_EQ(
      initial_sent_object_request.request_context().request_id().sequence_id(),
      1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendMultimodalRequest(
      kTestTime, std::move(region), kTestQueryText, lens::MULTIMODAL_SEARCH,
      additional_search_query_params, std::nullopt);

  // Verify the interaction request id sequence was incremented.
  ASSERT_TRUE(url_response_future.Wait());
  auto initial_sent_interaction_request =
      query_controller.sent_interaction_request();
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
      query_controller.sent_full_image_objects_request();
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
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageTranslateRequestFetchLatency),
            1);

  // Now change the languages.
  full_image_response_future.Clear();
  query_controller.SendFullPageTranslateQuery("en", "es");
  ASSERT_TRUE(full_image_response_future.Wait());

  // Check that the image sequence id and sequence id were incremented by
  // the fullpage translate request, and a new analytics id was generated.
  ASSERT_TRUE(full_image_response_future.IsReady());
  auto third_sent_object_request =
      query_controller.sent_full_image_objects_request();
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
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageTranslateRequestFetchLatency),
            2);

  // Now disable translate mode.
  full_image_response_future.Clear();
  query_controller.SendEndTranslateModeQuery();
  ASSERT_TRUE(full_image_response_future.Wait());

  // Check that the image sequence id and sequence id were incremented by
  // the end translate mode request.
  ASSERT_TRUE(full_image_response_future.IsReady());
  auto fourth_sent_object_request =
      query_controller.sent_full_image_objects_request();
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
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            2);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageTranslateRequestFetchLatency),
            2);

  query_controller.EndQuery();
}

TEST_F(LensOverlayQueryControllerTest, GetVsridForNewTab) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());
  ASSERT_TRUE(full_image_response_future.IsReady());

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendMultimodalRequest(
      kTestTime, std::move(region), kTestQueryText, lens::MULTIMODAL_SEARCH,
      additional_search_query_params, std::nullopt);

  ASSERT_TRUE(url_response_future.Wait());

  // Check that GetVsridForNewTab produces a vsrid that changes the analytics
  // id but keeps the other fields the same.
  lens::LensOverlayRequestId request_id =
      GetRequestIdFromUrl(url_response_future.Get().url());
  lens::LensOverlayRequestId new_tab_request_id =
      DecodeRequestIdFromVsrid(query_controller.GetVsridForNewTab());
  ASSERT_EQ(request_id.uuid(), new_tab_request_id.uuid());
  ASSERT_EQ(request_id.sequence_id(), new_tab_request_id.sequence_id());
  ASSERT_EQ(request_id.image_sequence_id(),
            new_tab_request_id.image_sequence_id());
  ASSERT_NE(request_id.analytics_id(), new_tab_request_id.analytics_id());

  // Check that sending a new task completion event still has the original
  // analytics id.
  query_controller
      .LensOverlayQueryController::SendTaskCompletionGen204IfEnabled(
          lens::mojom::UserAction::kCopyText);
  EXPECT_TRUE(
      query_controller.last_task_completion_gen204_analytics_id().has_value());
  std::string encoded_analytics_id =
      base32::Base32Encode(base::as_byte_span(request_id.analytics_id()),
                           base32::Base32EncodePolicy::OMIT_PADDING);
  EXPECT_EQ(query_controller.last_task_completion_gen204_analytics_id().value(),
            encoded_analytics_id);

  query_controller.EndQuery();
}

TEST_F(LensOverlayQueryControllerTest,
       RoutingInfo_FromFullImageResponse_IncludedInRequestId) {
  // Disable the cluster info handshake flow.
  feature_list_.Reset();
  feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{},
      /*disabled_features=*/{lens::features::kLensOverlayLatencyOptimizations,
                             lens::features::kLensOverlayContextualSearchbox});

  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  fake_objects_response.mutable_cluster_info()
      ->mutable_routing_info()
      ->set_server_address(kTestServerAddress);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  ASSERT_FALSE(query_controller.last_cluster_info_request().has_value());

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendMultimodalRequest(
      kTestTime, std::move(region), kTestQueryText, lens::MULTIMODAL_SEARCH,
      additional_search_query_params, std::nullopt);
  ASSERT_TRUE(url_response_future.Wait());
  query_controller.EndQuery();

  // Verify the routing info is included in the request id.
  ASSERT_TRUE(url_response_future.IsReady());
  auto initial_sent_interaction_request =
      query_controller.sent_interaction_request();
  ASSERT_EQ(kTestServerAddress,
            initial_sent_interaction_request.request_context()
                .request_id()
                .routing_info()
                .server_address());
  lens::LensOverlayRoutingInfo url_routing_info =
      GetRoutingInfoFromUrl(url_response_future.Get().url());
  ASSERT_EQ(kTestServerAddress, url_routing_info.server_address());
}

// Tests that the query controller attaches the server session id from the
// cluster info response to the full image request.
TEST_F(LensOverlayQueryControllerTest,
       RoutingInfo_FromClusterInfoReseponse_IncludedInRequestId) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  fake_cluster_info_response.mutable_routing_info()->set_server_address(
      kTestServerAddress);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      "garbage");
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendMultimodalRequest(
      kTestTime, std::move(region), kTestQueryText, lens::MULTIMODAL_SEARCH,
      additional_search_query_params, std::nullopt);
  ASSERT_TRUE(url_response_future.Wait());
  query_controller.EndQuery();

  // Verify the routing info is included in the request id.
  ASSERT_TRUE(url_response_future.IsReady());
  auto initial_sent_interaction_request =
      query_controller.sent_interaction_request();
  ASSERT_EQ(kTestServerAddress,
            initial_sent_interaction_request.request_context()
                .request_id()
                .routing_info()
                .server_address());
  lens::LensOverlayRoutingInfo url_routing_info =
      GetRoutingInfoFromUrl(url_response_future.Get().url());
  ASSERT_EQ(kTestServerAddress, url_routing_info.server_address());
}

// Tests that the query controller does not need the full image response to
// include the cluster info if it was already included in the cluster info
// response.
TEST_F(LensOverlayQueryControllerTest,
       FulllImageResponseHandler_SupportsNoClusterInfoInObjectsResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  fake_cluster_info_response.mutable_routing_info()->set_server_address(
      kTestServerAddress);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);

  // This fake response does not include cluster info.
  lens::LensOverlayObjectsResponse fake_objects_response;
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendMultimodalRequest(
      kTestTime, std::move(region), kTestQueryText, lens::MULTIMODAL_SEARCH,
      additional_search_query_params, std::nullopt);
  ASSERT_TRUE(url_response_future.Wait());
  query_controller.EndQuery();

  // Verify the routing info is included in the request id.
  ASSERT_TRUE(url_response_future.IsReady());
  auto initial_sent_interaction_request =
      query_controller.sent_interaction_request();
  ASSERT_EQ(kTestServerAddress,
            initial_sent_interaction_request.request_context()
                .request_id()
                .routing_info()
                .server_address());
  lens::LensOverlayRoutingInfo url_routing_info =
      GetRoutingInfoFromUrl(url_response_future.Get().url());
  ASSERT_EQ(kTestServerAddress, url_routing_info.server_address());
}

TEST_F(LensOverlayQueryControllerTest,
       PdfCompressionEnabled_WebpageBytes_DoesNotCompressBytes) {
  InitFeaturesWithPdfCompression();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeApcPageContents,
      lens::MimeType::kHtml, /*pdf_current_page=*/std::nullopt, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_page_content_update_requests_sent() == 1;
  }));

  // Verify the page content request is as expected.
  auto page_content_request =
      query_controller.sent_page_content_objects_request();
  ASSERT_FALSE(page_content_request.payload().content().content_data().empty());
  ASSERT_EQ(page_content_request.payload().content().webpage_url(),
            kTestPageUrl);

  // Verify the bytes were included but not compressed.
  auto webpage_content_data =
      page_content_request.payload().content().content_data(0);
  ASSERT_EQ(webpage_content_data.content_type(),
            lens::ContentData::CONTENT_TYPE_ANNOTATED_PAGE_CONTENT);
  ASSERT_EQ(webpage_content_data.data().size(), kFakeContentBytes.size());
  ASSERT_EQ(webpage_content_data.compression_type(),
            lens::CompressionType::UNCOMPRESSED);
}

TEST_F(LensOverlayQueryControllerTest,
       CompressionEnabled_PdfBytes_CompressesBytes) {
  InitFeaturesWithPdfCompression();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakePdfPageContents,
      lens::MimeType::kPdf, /*pdf_current_page=*/std::nullopt, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_page_content_update_requests_sent() == 1;
  }));

  // Verify the page content request is as expected.
  auto page_content_request =
      query_controller.sent_page_content_objects_request();
  ASSERT_EQ(page_content_request.payload().content().content_data().size(), 1);
  EXPECT_EQ(page_content_request.payload().content().webpage_url(),
            kTestPageUrl);

  auto content_data = page_content_request.payload().content().content_data(0);
  ASSERT_FALSE(content_data.data().empty());
  EXPECT_EQ(content_data.content_type(), lens::ContentData::CONTENT_TYPE_PDF);

  // Compress the bytes here to compare the size.
  std::vector<uint8_t> compressed_bytes_buffer(20);
  const size_t expected_compressed_size = ZSTD_compress(
      compressed_bytes_buffer.data(), compressed_bytes_buffer.size(),
      kFakeContentBytes.data(), kFakeContentBytes.size(),
      lens::features::GetZstdCompressionLevel());

  // Verify the bytes were included and compressed.
  ASSERT_EQ(content_data.data().size(), expected_compressed_size);
  ASSERT_EQ(content_data.compression_type(), lens::CompressionType::ZSTD);
}

TEST_F(LensOverlayQueryControllerTest, FetchInteraction_WithDetectedText) {
  feature_list_.Reset();
  feature_list_.InitWithFeaturesAndParameters(
      {{lens::features::kLensOverlayLatencyOptimizations,
        {{"enable-cluster-info-optimization", "true"}}}},
      {});

  base::test::TestFuture<lens::mojom::TextPtr> interaction_response_future;
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_response_future.GetRepeatingCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  // Set up fake detected text.
  fake_interaction_response.mutable_text()->set_content_language("und");
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;

  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_page_contents=*/{},
      /*primary_content_type=*/lens::MimeType::kUnknown,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  ASSERT_TRUE(query_controller.last_cluster_info_request().has_value());

  query_controller.SendRegionSearch(
      kTestTime, std::move(region), lens::REGION_SEARCH,
      additional_search_query_params, std::nullopt);
  ASSERT_TRUE(url_response_future.Wait());
  query_controller.EndQuery();

  ASSERT_TRUE(interaction_response_future.Wait());
  EXPECT_EQ(interaction_response_future.Take()->content_language, "und");
}

TEST_F(LensOverlayQueryControllerTest, UploadChunkingPDF) {
  InitFeaturesWithUploadChunkingAndNewContentPayload();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakePdfPageContents,
      lens::MimeType::kPdf, std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());
  WaitForSuggestInputsWithEncodedImageSignals(&query_controller);
  query_controller.EndQuery();

  // Verify the content bytes were not included with the image bytes request.
  auto full_image_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(full_image_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(full_image_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(full_image_request.payload().content_data().empty());
  ASSERT_EQ(full_image_request.payload().content().content_data().size(), 0);

  // Verify the content bytes were included in a followup request.
  auto page_content_request =
      query_controller.sent_page_content_objects_request();
  ASSERT_EQ(page_content_request.payload().content().content_data().size(), 1);
  // When chunking, content data should be empty but content type should still
  // be set.
  ASSERT_TRUE(
      page_content_request.payload().content().content_data(0).data().empty());
  ASSERT_EQ(
      page_content_request.payload().content().content_data(0).content_type(),
      lens::ContentData::CONTENT_TYPE_PDF);
  ASSERT_EQ(page_content_request.payload()
                .content()
                .content_data(0)
                .compression_type(),
            lens::CompressionType::ZSTD);

  // Verify the page url and title were included in the request.
  ASSERT_EQ(page_content_request.payload().content().webpage_url(),
            kTestPageUrl);
  ASSERT_EQ(page_content_request.payload().content().webpage_title(),
            kTestPageTitle);

  // Verify the page content request has the correct request id.
  ASSERT_EQ(1,
            page_content_request.request_context().request_id().sequence_id());

  // Verify chunks are set correctly on payload.
  ASSERT_TRUE(page_content_request.payload()
                  .content()
                  .content_data(0)
                  .has_stored_chunk_options());
  EXPECT_EQ(2, page_content_request.payload()
                   .content()
                   .content_data(0)
                   .stored_chunk_options()
                   .total_stored_chunks());
  EXPECT_TRUE(page_content_request.payload()
                  .content()
                  .content_data(0)
                  .stored_chunk_options()
                  .read_stored_chunks());
  EXPECT_FALSE(page_content_request.payload()
                   .content()
                   .content_data(0)
                   .stored_chunk_options()
                   .is_read_retry());
  EXPECT_EQ(2, query_controller.num_upload_chunk_requests_sent());

  // Check interaction request is correct.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::PDF_QUERY);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_selection_metadata());

  // Check search URL is correct.
  ASSERT_TRUE(url_response_future.IsReady());
  std::string unused_client_upload_duration;
  bool has_client_upload_duration = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kClientUploadDurationQueryParameter, &unused_client_upload_duration);
  std::string unused_query_submission_time;
  bool has_query_submission_time = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kQuerySubmissionTimeQueryParameter,
      &unused_query_submission_time);
  std::string visual_input_type;
  bool has_visual_input_type = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kVisualInputTypeParameterKey,
      &visual_input_type);
  std::string invocation_source;
  bool has_invocation_source = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kInvocationSourceParameterKey,
      &invocation_source);
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  const auto latest_suggest_inputs = query_controller.GetLensSuggestInputs();

  ASSERT_TRUE(has_vsint);
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::MULTIMODAL_SEARCH);
  ASSERT_TRUE(has_client_upload_duration);
  ASSERT_TRUE(has_query_submission_time);
  ASSERT_TRUE(has_visual_input_type);
  ASSERT_EQ(visual_input_type, "pdf");
  ASSERT_TRUE(has_invocation_source);
  ASSERT_EQ(invocation_source, "chrome.cr.menu");
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kPageContentUploadLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(latest_suggest_inputs.encoded_image_signals(), kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(latest_suggest_inputs.contextual_visual_input_type(), "pdf");
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs.encoded_request_id());
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialPageContentRequestSent),
            1);

  // Test controller calls the progress callback with position 10 for each
  // chunk, so expect total of 20 for the 2 chunks.
  EXPECT_EQ(query_controller.total_chunk_progress_for_testing(), 20UL);
  EXPECT_EQ(query_controller.total_chunk_upload_size_for_testing(), 22UL);
}

TEST_F(LensOverlayQueryControllerTest, UploadChunkingPDF_SmallPdf) {
  InitFeaturesWithUploadChunkingAndNewContentPayload();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      kFakeSmallPdfPageContents, lens::MimeType::kPdf, std::nullopt, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());
  WaitForSuggestInputsWithEncodedImageSignals(&query_controller);
  query_controller.EndQuery();

  // Verify the content bytes were not included with the image bytes request.
  auto full_image_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(full_image_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(full_image_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(full_image_request.payload().content_data().empty());
  ASSERT_EQ(full_image_request.payload().content().content_data().size(), 0);

  // Verify the content bytes were included in a followup request.
  auto page_content_request =
      query_controller.sent_page_content_objects_request();
  ASSERT_EQ(page_content_request.payload().content().content_data().size(), 1);

  // When chunking is enabled, the PDFs smaller than 2MB should not be chunked
  // and therefore still included in the content data.
  ASSERT_FALSE(
      page_content_request.payload().content().content_data(0).data().empty());
  ASSERT_EQ(
      page_content_request.payload().content().content_data(0).content_type(),
      lens::ContentData::CONTENT_TYPE_PDF);

  // Verify chunking was skipped.
  ASSERT_FALSE(page_content_request.payload()
                   .content()
                   .content_data(0)
                   .has_stored_chunk_options());

  // Verify the page url and title were included in the request.
  ASSERT_EQ(page_content_request.payload().content().webpage_url(),
            kTestPageUrl);
  ASSERT_EQ(page_content_request.payload().content().webpage_title(),
            kTestPageTitle);

  // Verify the page content request has the correct request id.
  ASSERT_EQ(1,
            page_content_request.request_context().request_id().sequence_id());

  // Check interaction request is correct.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::PDF_QUERY);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_selection_metadata());

  // Check search URL is correct.
  ASSERT_TRUE(url_response_future.IsReady());
  std::string unused_client_upload_duration;
  bool has_client_upload_duration = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kClientUploadDurationQueryParameter, &unused_client_upload_duration);
  std::string unused_query_submission_time;
  bool has_query_submission_time = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kQuerySubmissionTimeQueryParameter,
      &unused_query_submission_time);
  std::string visual_input_type;
  bool has_visual_input_type = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kVisualInputTypeParameterKey,
      &visual_input_type);
  std::string invocation_source;
  bool has_invocation_source = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kInvocationSourceParameterKey,
      &invocation_source);
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  const auto latest_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_TRUE(has_vsint);
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::MULTIMODAL_SEARCH);
  ASSERT_TRUE(has_client_upload_duration);
  ASSERT_TRUE(has_query_submission_time);
  ASSERT_TRUE(has_visual_input_type);
  ASSERT_EQ(visual_input_type, "pdf");
  ASSERT_TRUE(has_invocation_source);
  ASSERT_EQ(invocation_source, "chrome.cr.menu");
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kPageContentUploadLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(latest_suggest_inputs.encoded_image_signals(), kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(latest_suggest_inputs.contextual_visual_input_type(), "pdf");
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs.encoded_request_id());
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialPageContentRequestSent),
            1);
}

TEST_F(LensOverlayQueryControllerTest,
       UploadChunkingPDF_RetryAfterMetadataError) {
  InitFeaturesWithUploadChunkingAndNewContentPayload();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);

  // Set first page content upload request to return missing chunks error.
  query_controller
      .set_next_page_content_objects_request_should_return_metadata_error(true);

  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakePdfPageContents,
      lens::MimeType::kPdf, std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());
  WaitForSuggestInputsWithEncodedImageSignals(&query_controller);
  query_controller.EndQuery();

  // Verify the content bytes were not included with the image bytes request.
  auto full_image_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(full_image_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(full_image_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(full_image_request.payload().content_data().empty());
  ASSERT_EQ(full_image_request.payload().content().content_data().size(), 0);

  // Verify that page content update request was resent.
  ASSERT_EQ(query_controller.num_page_content_update_requests_sent(), 2);

  // Verify the content bytes were included in a followup request.
  auto page_content_request =
      query_controller.sent_page_content_objects_request();
  ASSERT_EQ(page_content_request.payload().content().content_data().size(), 1);
  // When chunking, content data should be empty but content type should still
  // be set.
  ASSERT_TRUE(
      page_content_request.payload().content().content_data(0).data().empty());
  ASSERT_EQ(
      page_content_request.payload().content().content_data(0).content_type(),
      lens::ContentData::CONTENT_TYPE_PDF);
  ASSERT_EQ(page_content_request.payload()
                .content()
                .content_data(0)
                .compression_type(),
            lens::CompressionType::ZSTD);

  // Verify the page url and title were included in the request.
  ASSERT_EQ(page_content_request.payload().content().webpage_url(),
            kTestPageUrl);
  ASSERT_EQ(page_content_request.payload().content().webpage_title(),
            kTestPageTitle);

  // Verify the page content request has the correct request id.
  ASSERT_EQ(1,
            page_content_request.request_context().request_id().sequence_id());

  // Verify chunks are set correctly on payload.
  ASSERT_TRUE(page_content_request.payload()
                  .content()
                  .content_data(0)
                  .has_stored_chunk_options());
  EXPECT_EQ(2, page_content_request.payload()
                   .content()
                   .content_data(0)
                   .stored_chunk_options()
                   .total_stored_chunks());
  EXPECT_TRUE(page_content_request.payload()
                  .content()
                  .content_data(0)
                  .stored_chunk_options()
                  .read_stored_chunks());
  EXPECT_TRUE(page_content_request.payload()
                  .content()
                  .content_data(0)
                  .stored_chunk_options()
                  .is_read_retry());
  EXPECT_EQ(2, query_controller.num_upload_chunk_requests_sent());

  // Check interaction request is correct.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::PDF_QUERY);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_selection_metadata());

  // Check search URL is correct.
  ASSERT_TRUE(url_response_future.IsReady());
  std::string unused_client_upload_duration;
  bool has_client_upload_duration = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kClientUploadDurationQueryParameter, &unused_client_upload_duration);
  std::string unused_query_submission_time;
  bool has_query_submission_time = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kQuerySubmissionTimeQueryParameter,
      &unused_query_submission_time);
  std::string visual_input_type;
  bool has_visual_input_type = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kVisualInputTypeParameterKey,
      &visual_input_type);
  std::string invocation_source;
  bool has_invocation_source = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kInvocationSourceParameterKey,
      &invocation_source);
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  const auto latest_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_TRUE(has_vsint);
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::MULTIMODAL_SEARCH);
  ASSERT_TRUE(has_client_upload_duration);
  ASSERT_TRUE(has_query_submission_time);
  ASSERT_TRUE(has_visual_input_type);
  ASSERT_EQ(visual_input_type, "pdf");
  ASSERT_TRUE(has_invocation_source);
  ASSERT_EQ(invocation_source, "chrome.cr.menu");
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kPageContentUploadLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(latest_suggest_inputs.encoded_image_signals(), kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(latest_suggest_inputs.contextual_visual_input_type(), "pdf");
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs.encoded_request_id());
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialPageContentRequestSent),
            1);
}

TEST_F(LensOverlayQueryControllerTest,
       UploadChunkingPDF_RetryAfterChunksError) {
  InitFeaturesWithUploadChunkingAndNewContentPayload();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);

  // Set first page content upload request to return missing chunks error.
  query_controller
      .set_next_page_content_objects_request_should_return_chunks_error(true);

  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakePdfPageContents,
      lens::MimeType::kPdf, std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());

  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());
  WaitForSuggestInputsWithEncodedImageSignals(&query_controller);
  query_controller.EndQuery();

  // Verify the content bytes were not included with the image bytes request.
  auto full_image_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(full_image_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(full_image_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(full_image_request.payload().content_data().empty());
  ASSERT_EQ(full_image_request.payload().content().content_data().size(), 0);

  // Verify that page content update request was resent.
  ASSERT_EQ(query_controller.num_page_content_update_requests_sent(), 2);

  // Verify the content bytes were included in a followup request.
  auto page_content_request =
      query_controller.sent_page_content_objects_request();
  ASSERT_EQ(page_content_request.payload().content().content_data().size(), 1);
  // When chunking, content data should be empty but content type should still
  // be set.
  ASSERT_TRUE(
      page_content_request.payload().content().content_data(0).data().empty());
  ASSERT_EQ(
      page_content_request.payload().content().content_data(0).content_type(),
      lens::ContentData::CONTENT_TYPE_PDF);
  ASSERT_EQ(page_content_request.payload()
                .content()
                .content_data(0)
                .compression_type(),
            lens::CompressionType::ZSTD);

  // Verify the page url and title were included in the request.
  ASSERT_EQ(page_content_request.payload().content().webpage_url(),
            kTestPageUrl);
  ASSERT_EQ(page_content_request.payload().content().webpage_title(),
            kTestPageTitle);

  // Verify the page content request has the correct request id.
  ASSERT_EQ(1,
            page_content_request.request_context().request_id().sequence_id());

  // Verify chunks are set correctly on payload.
  ASSERT_TRUE(page_content_request.payload()
                  .content()
                  .content_data(0)
                  .has_stored_chunk_options());
  EXPECT_EQ(2, page_content_request.payload()
                   .content()
                   .content_data(0)
                   .stored_chunk_options()
                   .total_stored_chunks());
  EXPECT_TRUE(page_content_request.payload()
                  .content()
                  .content_data(0)
                  .stored_chunk_options()
                  .read_stored_chunks());
  EXPECT_TRUE(page_content_request.payload()
                  .content()
                  .content_data(0)
                  .stored_chunk_options()
                  .is_read_retry());

  // Verify that one chunk was resent.
  EXPECT_EQ(3, query_controller.num_upload_chunk_requests_sent());

  // Check interaction request is correct.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::PDF_QUERY);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_selection_metadata());

  // Check search URL is correct.
  ASSERT_TRUE(url_response_future.IsReady());
  std::string unused_client_upload_duration;
  bool has_client_upload_duration = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kClientUploadDurationQueryParameter, &unused_client_upload_duration);
  std::string unused_query_submission_time;
  bool has_query_submission_time = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kQuerySubmissionTimeQueryParameter,
      &unused_query_submission_time);
  std::string visual_input_type;
  bool has_visual_input_type = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kVisualInputTypeParameterKey,
      &visual_input_type);
  std::string invocation_source;
  bool has_invocation_source = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kInvocationSourceParameterKey,
      &invocation_source);
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  const auto latest_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_TRUE(has_vsint);
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::MULTIMODAL_SEARCH);
  ASSERT_TRUE(has_client_upload_duration);
  ASSERT_TRUE(has_query_submission_time);
  ASSERT_TRUE(has_visual_input_type);
  ASSERT_EQ(visual_input_type, "pdf");
  ASSERT_TRUE(has_invocation_source);
  ASSERT_EQ(invocation_source, "chrome.cr.menu");
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kPageContentUploadLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(latest_suggest_inputs.encoded_image_signals(), kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(latest_suggest_inputs.contextual_visual_input_type(), "pdf");
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs.encoded_request_id());
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialPageContentRequestSent),
            1);
}

TEST_F(LensOverlayQueryControllerTest, UploadChunkingHTML) {
  InitFeaturesWithUploadChunkingAndNewContentPayload();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeApcPageContents,
      lens::MimeType::kHtml, std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());
  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  ASSERT_TRUE(url_response_future.Wait());
  WaitForSuggestInputsWithEncodedImageSignals(&query_controller);
  query_controller.EndQuery();

  ASSERT_TRUE(full_image_response_future.IsReady());

  // Verify the content bytes were not included with the image bytes request.
  auto full_image_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(full_image_request.image_data().image_metadata().width(), 100);
  ASSERT_EQ(full_image_request.image_data().image_metadata().height(), 100);
  ASSERT_TRUE(full_image_request.payload().content_data().empty());
  ASSERT_EQ(full_image_request.payload().content().content_data().size(), 0);

  // Verify the content bytes were included in a followup request.
  auto page_content_request =
      query_controller.sent_page_content_objects_request();
  ASSERT_EQ(page_content_request.payload().content().content_data().size(), 1);

  // When chunking is enabled, the HTML should not be chunked and therefore
  // still included in the content data.
  ASSERT_FALSE(
      page_content_request.payload().content().content_data(0).data().empty());
  ASSERT_EQ(
      page_content_request.payload().content().content_data(0).content_type(),
      lens::ContentData::CONTENT_TYPE_ANNOTATED_PAGE_CONTENT);

  // Verify the page url and title were included in the request.
  ASSERT_EQ(page_content_request.payload().content().webpage_url(),
            kTestPageUrl);
  ASSERT_EQ(page_content_request.payload().content().webpage_title(),
            kTestPageTitle);

  // The full image and page content requests should have the same request id.
  ASSERT_EQ(full_image_request.request_context().request_id().sequence_id(),
            page_content_request.request_context().request_id().sequence_id());

  // Verify chunks are not sent.
  ASSERT_FALSE(page_content_request.payload()
                   .content()
                   .content_data(0)
                   .has_stored_chunk_options());
  EXPECT_EQ(0, page_content_request.payload()
                   .content()
                   .content_data(0)
                   .stored_chunk_options()
                   .total_stored_chunks());
  EXPECT_FALSE(page_content_request.payload()
                   .content()
                   .content_data(0)
                   .stored_chunk_options()
                   .read_stored_chunks());
  EXPECT_FALSE(page_content_request.payload()
                   .content()
                   .content_data(0)
                   .stored_chunk_options()
                   .is_read_retry());
  EXPECT_EQ(0, query_controller.num_upload_chunk_requests_sent());

  // Check interaction request is correct.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
  ASSERT_EQ(
      sent_interaction_request.request_context().request_id().sequence_id(), 2);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata().type(),
            lens::LensOverlayInteractionRequestMetadata::WEBPAGE_QUERY);
  ASSERT_EQ(sent_interaction_request.interaction_request_metadata()
                .query_metadata()
                .text_query()
                .query(),
            kTestQueryText);
  ASSERT_FALSE(sent_interaction_request.interaction_request_metadata()
                   .has_selection_metadata());

  // Check search URL is correct.
  ASSERT_TRUE(url_response_future.IsReady());
  std::string unused_client_upload_duration;
  bool has_client_upload_duration = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kClientUploadDurationQueryParameter, &unused_client_upload_duration);
  std::string unused_query_submission_time;
  bool has_query_submission_time = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kQuerySubmissionTimeQueryParameter,
      &unused_query_submission_time);
  std::string visual_input_type;
  bool has_visual_input_type = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kVisualInputTypeParameterKey,
      &visual_input_type);
  std::string invocation_source;
  bool has_invocation_source = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()), kInvocationSourceParameterKey,
      &invocation_source);
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  const auto latest_suggest_inputs = query_controller.GetLensSuggestInputs();
  ASSERT_TRUE(has_vsint);
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::MULTIMODAL_SEARCH);
  ASSERT_TRUE(has_client_upload_duration);
  ASSERT_TRUE(has_query_submission_time);
  ASSERT_TRUE(has_visual_input_type);
  ASSERT_EQ(visual_input_type, "wp");
  ASSERT_TRUE(has_invocation_source);
  ASSERT_EQ(invocation_source, "chrome.cr.menu");
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            1);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kPageContentUploadLatency),
            1);
  ASSERT_TRUE(url_response_future.Get().has_url());
  ASSERT_EQ(latest_suggest_inputs.encoded_image_signals(), kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(latest_suggest_inputs.contextual_visual_input_type(), "wp");
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs.encoded_request_id());
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialPageContentRequestSent),
            1);
}
class LensOverlayQueryControllerMockTimeTest
    : public LensOverlayQueryControllerTest {
 public:
  // Pass a MOCK_TIME task environment to the base class.
  explicit LensOverlayQueryControllerMockTimeTest()
      : LensOverlayQueryControllerTest(
            std::unique_ptr<content::BrowserTaskEnvironment>(
                std::make_unique<content::BrowserTaskEnvironment>(
                    content::BrowserTaskEnvironment::MainThreadType::UI,
                    content::BrowserTaskEnvironment::TimeSource::MOCK_TIME))) {}
};

TEST_F(LensOverlayQueryControllerMockTimeTest,
       FetchInteraction_StartsNewQueryFlowAfterTimeout) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&, const SkBitmap&>
      thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      thumbnail_created_future.GetRepeatingCallback(), base::NullCallback(),
      fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);
  lens::LensOverlayInteractionResponse fake_interaction_response;
  fake_interaction_response.set_encoded_response(kTestSuggestSignals);
  query_controller.set_fake_interaction_response(fake_interaction_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;

  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakePdfPageContents,
      lens::MimeType::kPdf, /*pdf_current_page=*/std::nullopt,
      /*ui_scale_factor=*/0, base::TimeTicks::Now());

  // Wait for the full image response to be received, then clear the future.
  ASSERT_TRUE(full_image_response_future.Wait());
  CheckClusterInfoRequestMatchesDefaultClientContentRequest(
      *query_controller.last_cluster_info_request());
  full_image_response_future.Clear();

  task_environment_->FastForwardBy(base::TimeDelta(base::Minutes(60)));
  query_controller.SendRegionSearch(
      kTestTime, std::move(region), lens::REGION_SEARCH,
      additional_search_query_params, std::nullopt);
  ASSERT_TRUE(url_response_future.Wait());
  query_controller.EndQuery();

  // The full image response having another value, after it was already
  // cleared, indicates that the query controller successfully started a
  // new query flow due to the timeout occurring.
  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            2);
  CheckGen204IdsMatch(query_controller.sent_client_logs(),
                      url_response_future.Get());

  // Verify the full image request has the correct request id.
  auto full_image_request = query_controller.sent_full_image_objects_request();
  ASSERT_EQ(full_image_request.request_context().request_id().sequence_id(), 1);
  ASSERT_EQ(
      full_image_request.request_context().request_id().image_sequence_id(), 1);

  // Verify the page content request has the correct request id.
  auto page_content_request =
      query_controller.sent_page_content_objects_request();
  ASSERT_EQ(page_content_request.request_context().request_id().sequence_id(),
            2);
  ASSERT_EQ(
      page_content_request.request_context().request_id().image_sequence_id(),
      1);

  // Verify the interaction request has the correct request id.
  auto interaction_request = query_controller.sent_interaction_request();
  ASSERT_EQ(interaction_request.request_context().request_id().sequence_id(),
            3);
  ASSERT_EQ(
      interaction_request.request_context().request_id().image_sequence_id(),
      1);
}

TEST_F(LensOverlayQueryControllerTest,
       ContextualPdfQuery_ShouldHaveFullImageZoomedCropInVsint) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      base::NullCallback(), base::NullCallback(), fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakePdfPageContents,
      lens::MimeType::kPdf, /*pdf_current_page=*/std::nullopt, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());

  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH, {});
  ASSERT_TRUE(url_response_future.Wait());

  // Verify zoomed_crop is set in vsint.
  auto vsint = GetVsintFromUrl(url_response_future.Get().url());
  ASSERT_TRUE(vsint.has_zoomed_crop());
  const auto& crop = vsint.zoomed_crop().crop();
  EXPECT_EQ(crop.center_x(), 0.5f);
  EXPECT_EQ(crop.center_y(), 0.5f);
  EXPECT_EQ(crop.width(), 1);
  EXPECT_EQ(crop.height(), 1);
  EXPECT_EQ(crop.coordinate_type(), lens::CoordinateType::NORMALIZED);
  EXPECT_EQ(vsint.zoomed_crop().zoom(), 1);

  query_controller.EndQuery();
}

TEST_F(LensOverlayQueryControllerTest,
       ContextualWebpageQuery_ShouldHaveFullImageZoomedCropInVsint) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), base::NullCallback(),
      base::NullCallback(), base::NullCallback(), fake_variations_client_.get(),
      IdentityManagerFactory::GetForProfile(profile()), profile(),
      lens::LensOverlayInvocationSource::kAppMenu,
      /*use_dark_mode=*/false, GetGen204Controller());

  // Set up the query controller responses.
  lens::LensOverlayServerClusterInfoResponse fake_cluster_info_response;
  fake_cluster_info_response.set_server_session_id(kTestServerSessionId);
  fake_cluster_info_response.set_search_session_id(kTestSearchSessionId);
  query_controller.set_fake_cluster_info_response(fake_cluster_info_response);
  lens::LensOverlayObjectsResponse fake_objects_response;
  fake_objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeApcPageContents,
      lens::MimeType::kAnnotatedPageContent,
      /*pdf_current_page=*/std::nullopt, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());

  query_controller.SendContextualTextQuery(
      kTestTime, kTestQueryText,
      lens::LensOverlaySelectionType::MULTIMODAL_SEARCH, {});
  ASSERT_TRUE(url_response_future.Wait());

  // Verify zoomed_crop is set in vsint.
  auto vsint = GetVsintFromUrl(url_response_future.Get().url());
  ASSERT_TRUE(vsint.has_zoomed_crop());
  const auto& crop = vsint.zoomed_crop().crop();
  EXPECT_EQ(crop.center_x(), 0.5f);
  EXPECT_EQ(crop.center_y(), 0.5f);
  EXPECT_EQ(crop.width(), 1);
  EXPECT_EQ(crop.height(), 1);
  EXPECT_EQ(crop.coordinate_type(), lens::CoordinateType::NORMALIZED);
  EXPECT_EQ(vsint.zoomed_crop().zoom(), 1);

  query_controller.EndQuery();
}

}  // namespace lens
