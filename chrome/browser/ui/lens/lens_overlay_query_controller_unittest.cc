// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base64url.h"
#include "base/containers/span.h"
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
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_mime_type.h"
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
#include "third_party/lens_server_proto/lens_overlay_document.pb.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_visual_search_interaction_data.pb.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "url/gurl.h"

namespace lens {

using LatencyType = LensOverlayGen204Controller::LatencyType;

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

// kFakeContentBytes and kNewFakeContentBytes needs to outlive the query
// controller, so initialize it here.
const std::vector<uint8_t> kFakeContentBytes({1, 2, 3, 4});
const std::vector<uint8_t> kNewFakeContentBytes({5, 6, 7, 8});

const std::vector<std::u16string> kShortPartialContent({u"page 1", u"page 2",
                                                        u"page 3"});
const std::vector<std::u16string> kLongPartialContent(
    {u"this is a page with over 100 characters to ensure that the average "
     "characters per page is above the heuristic."});

const base::test::FeatureRefAndParams
    kDefaultLensOverlayContextualSearchboxParams =
        base::test::FeatureRefAndParams(
            lens::features::kLensOverlayContextualSearchbox,
            {{"use-video-context-for-text-only-requests", "true"},
             {"use-pdf-vit-param", "true"},
             {"use-webpage-vit-param", "true"},
             {"use-pdf-interaction-type", "true"},
             {"use-webpage-interaction-type", "true"},
             {"send-lens-inputs-for-contextual-suggest", "true"},
             {"send-page-url-for-contextualization", "true"},
             {"send-lens-inputs-for-lens-suggest", "true"},
             {"send-lens-visual-interaction-data-for-lens-suggest", "true"},
             {"characters-per-page-heuristic", "50"}});

MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

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

  // We can't use a TestFuture::GetRepeatingCallback() because the callback
  // may be called multiple times before the test can consume the value.
  // Instead, this method returns a callback that can be called multiple times
  // and stores the latest suggest inputs in a member variable.
  LensOverlaySuggestInputsCallback GetSuggestInputsCallback() {
    return base::BindRepeating(
        &LensOverlayQueryControllerTest::HandleSuggestInputsResponse,
        weak_factory_.GetWeakPtr());
  }

  void WaitForSuggestInputsWithEncodedImageSignals() {
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return latest_suggest_inputs_.has_encoded_image_signals(); }));
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
    ASSERT_TRUE(client_logs.has_paella_id());
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
    if (interaction_request.has_image_crop()) {
      EXPECT_THAT(vsint.zoomed_crop(),
                  EqualsProto(interaction_request.image_crop().zoomed_crop()));
    } else {
      ASSERT_FALSE(vsint.has_zoomed_crop());
    }
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

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<lens::FakeLensOverlayGen204Controller> gen204_controller_;
  std::unique_ptr<FakeVariationsClient> fake_variations_client_;
  lens::proto::LensOverlaySuggestInputs latest_suggest_inputs_;
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
    latest_suggest_inputs_.Clear();

    feature_list_.InitWithFeaturesAndParameters(
        {kDefaultLensOverlayContextualSearchboxParams}, {});
  }

  void HandleSuggestInputsResponse(
      lens::proto::LensOverlaySuggestInputs suggest_inputs) {
    latest_suggest_inputs_ = suggest_inputs;
  }
};

TEST_F(LensOverlayQueryControllerTest, FetchInitialQuery_ReturnsResponse) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(), base::NullCallback(),
      GetSuggestInputsCallback(), base::NullCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());

  task_environment_->RunUntilIdle();
  query_controller.EndQuery();
  ASSERT_TRUE(full_image_response_future.IsReady());

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
  ASSERT_EQ(query_controller.sent_client_logs().lens_overlay_entry_point(),
            lens::LensOverlayClientLogs::APP_MENU);
  ASSERT_TRUE(query_controller.sent_client_logs().has_paella_id());
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
      GetSuggestInputsCallback(), base::NullCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());

  task_environment_->RunUntilIdle();
  query_controller.EndQuery();
  ASSERT_TRUE(full_image_response_future.IsReady());

  // Check the server session id is attached to the fetch url.
  std::string session_id_value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(query_controller.sent_fetch_url(),
                                         kSessionIdQueryParameterKey,
                                         &session_id_value));
  ASSERT_EQ(session_id_value, kTestServerSessionId);

  ASSERT_FALSE(latest_suggest_inputs_.has_encoded_image_signals());
  ASSERT_FALSE(
      latest_suggest_inputs_.has_encoded_visual_search_interaction_log_data());
  ASSERT_EQ(latest_suggest_inputs_.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(GetEncodedRequestId(query_controller.sent_request_id()),
            latest_suggest_inputs_.encoded_request_id());
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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());

  // Wait for the access token request for the cluster info to be sent.
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kFakeOAuthToken, base::Time::Max());
  // Wait for the access token request for the full image request to be sent.
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kFakeOAuthToken, base::Time::Max());

  task_environment_->RunUntilIdle();
  ASSERT_EQ(1, query_controller.num_cluster_info_fetch_requests_sent());

  // Reset the cluster info state.
  query_controller.ResetRequestClusterInfoStateForTesting();
  full_image_response_future.Clear();

  // Send interaction to trigger new query flow.
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(std::move(region), lens::REGION_SEARCH,
                                    additional_search_query_params,
                                    std::nullopt);

  // Wait for the access token request for the interaction request to be sent.
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kFakeOAuthToken, base::Time::Max());
  // Wait for the access token request for the cluster info request to be sent.
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kFakeOAuthToken, base::Time::Max());

  task_environment_->RunUntilIdle();
  query_controller.EndQuery();

  // Verify the cluster info was refetched.
  ASSERT_EQ(2, query_controller.num_cluster_info_fetch_requests_sent());
}

TEST_F(LensOverlayQueryControllerTest,
       FetchInitialQuery_SuggestInputsHaveFlagValues) {
  feature_list_.Reset();
  feature_list_.InitWithFeaturesAndParameters(
      {{lens::features::kLensOverlayLatencyOptimizations,
        {{"enable-early-interaction-optimization", "true"},
         {"enable-cluster-info-optimization", "true"}}},
       {lens::features::kLensOverlayContextualSearchbox,
        {{"send-lens-inputs-for-contextual-suggest", "false"},
         {"send-lens-inputs-for-lens-suggest", "false"},
         {"send-lens-visual-interaction-data-for-lens-suggest", "false"}}}},
      {});

  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(), base::NullCallback(),
      GetSuggestInputsCallback(), base::NullCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());

  task_environment_->RunUntilIdle();
  query_controller.EndQuery();
  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_FALSE(
      latest_suggest_inputs_.send_gsession_vsrid_for_contextual_suggest());
  ASSERT_FALSE(latest_suggest_inputs_.send_gsession_vsrid_for_lens_suggest());
  ASSERT_FALSE(latest_suggest_inputs_.send_vsint_for_lens_suggest());
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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());
  task_environment_->RunUntilIdle();

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(std::move(region), lens::REGION_SEARCH,
                                    additional_search_query_params,
                                    std::nullopt);
  task_environment_->RunUntilIdle();
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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());
  task_environment_->RunUntilIdle();

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(std::move(region), lens::REGION_SEARCH,
                                    additional_search_query_params,
                                    std::nullopt);
  task_environment_->RunUntilIdle();
  query_controller.EndQuery();

  std::string unused_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &unused_start_time);

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
  ASSERT_EQ(latest_suggest_inputs_.encoded_image_signals(),
            kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs_.search_session_id(), kTestSearchSessionId);
  ASSERT_TRUE(
      latest_suggest_inputs_.has_encoded_visual_search_interaction_log_data());
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs_.encoded_request_id());
  ASSERT_EQ(sent_object_request.request_context().request_id().sequence_id(),
            1);

  // Verify the interaction request.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());
  task_environment_->RunUntilIdle();

  ASSERT_EQ(query_controller.num_cluster_info_fetch_requests_sent(), 1);

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(std::move(region), lens::REGION_SEARCH,
                                    additional_search_query_params,
                                    std::nullopt);
  task_environment_->RunUntilIdle();
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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());
  task_environment_->RunUntilIdle();

  SkBitmap region_bitmap = CreateNonEmptyBitmap(100, 100);
  region_bitmap.setAlphaType(kOpaque_SkAlphaType);
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(50, 50, 100, 100);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(
      std::move(region), lens::REGION_SEARCH, additional_search_query_params,
      std::make_optional<SkBitmap>(region_bitmap));
  task_environment_->RunUntilIdle();
  query_controller.EndQuery();

  std::string unused_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &unused_start_time);
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
  ASSERT_EQ(latest_suggest_inputs_.encoded_image_signals(),
            kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs_.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs_.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs_.encoded_request_id());
  ASSERT_EQ(sent_object_request.request_context().request_id().sequence_id(),
            1);

  // Verify the interaction request.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());
  task_environment_->RunUntilIdle();

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendMultimodalRequest(
      std::move(region), kTestQueryText, lens::MULTIMODAL_SEARCH,
      additional_search_query_params, std::nullopt);
  task_environment_->RunUntilIdle();
  query_controller.EndQuery();

  std::string unused_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &unused_start_time);
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
  ASSERT_EQ(latest_suggest_inputs_.encoded_image_signals(),
            kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs_.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs_.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs_.encoded_request_id());
  ASSERT_EQ(sent_object_request.request_context().request_id().sequence_id(),
            1);

  // Verify the interaction request.
  auto sent_interaction_request = query_controller.sent_interaction_request();
  CheckVsintMatchesInteractionRequest(
      GetVsintFromUrl(url_response_future.Get().url()),
      sent_interaction_request);
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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());
  task_environment_->RunUntilIdle();

  query_controller.SendTextOnlyQuery(
      "", lens::LensOverlaySelectionType::SELECT_TEXT_HIGHLIGHT,
      additional_search_query_params);
  task_environment_->RunUntilIdle();
  query_controller.EndQuery();

  std::string actual_encoded_video_context;
  net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                             kVideoContextParamKey,
                             &actual_encoded_video_context);

  std::string unused_start_time;
  bool has_start_time =
      net::GetValueForKeyInQuery(GURL(url_response_future.Get().url()),
                                 kStartTimeQueryParam, &unused_start_time);

  auto vsint = GetVsintFromUrl(url_response_future.Get().url());
  ASSERT_EQ(vsint.object_id(), "");
  ASSERT_FALSE(vsint.has_zoomed_crop());
  ASSERT_EQ(vsint.interaction_type(),
            lens::LensOverlayInteractionRequestMetadata::TEXT_SELECTION);

  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_TRUE(url_response_future.IsReady());
  ASSERT_FALSE(latest_suggest_inputs_.has_encoded_image_signals());
  ASSERT_EQ(vsint.log_data().user_selection_data().selection_type(),
            lens::SELECT_TEXT_HIGHLIGHT);
  ASSERT_FALSE(latest_suggest_inputs_.has_contextual_visual_input_type());
  ASSERT_EQ(actual_encoded_video_context, kTestEncodedVideoContext);
  ASSERT_TRUE(has_start_time);
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kFullPageObjectsRequestFetchLatency),
            0);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchTextOnlyInteractionWithPdfBytes_ReturnsResponse) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeContentBytes,
      lens::MimeType::kPdf, 0, base::TimeTicks::Now());
  task_environment_->RunUntilIdle();
  query_controller.SendContextualTextQuery(
      kTestQueryText, lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  task_environment_->RunUntilIdle();
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
  ASSERT_FALSE(page_content_request.payload().content_data().empty());
  ASSERT_EQ(page_content_request.payload().content_type(), "application/pdf");

  // Verify the page url was included in the request.
  ASSERT_EQ(page_content_request.payload().page_url(), kTestPageUrl);

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
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  ASSERT_TRUE(has_vsint);
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::MULTIMODAL_SEARCH);
  ASSERT_TRUE(has_start_time);
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
  ASSERT_EQ(latest_suggest_inputs_.encoded_image_signals(),
            kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs_.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs_.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(latest_suggest_inputs_.contextual_visual_input_type(), "pdf");
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs_.encoded_request_id());
  ASSERT_EQ(query_controller.latency_gen_204_counter(
                LatencyType::kInvocationToInitialPageContentRequestSent),
            1);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchTextOnlyInteractionWithHtml_ReturnsResponse) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeContentBytes,
      lens::MimeType::kHtml, 0, base::TimeTicks::Now());
  task_environment_->RunUntilIdle();
  query_controller.SendContextualTextQuery(
      kTestQueryText, lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  task_environment_->RunUntilIdle();
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
  ASSERT_FALSE(page_content_request.payload().content_data().empty());
  ASSERT_EQ(page_content_request.payload().content_type(), "text/html");

  // Verify the page url was included in the request.
  ASSERT_EQ(page_content_request.payload().page_url(), kTestPageUrl);

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
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  ASSERT_TRUE(has_vsint);
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::MULTIMODAL_SEARCH);
  ASSERT_TRUE(has_start_time);
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
  ASSERT_EQ(latest_suggest_inputs_.encoded_image_signals(),
            kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs_.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs_.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(latest_suggest_inputs_.contextual_visual_input_type(), "wp");
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs_.encoded_request_id());
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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeContentBytes,
      lens::MimeType::kPlainText, 0, base::TimeTicks::Now());
  task_environment_->RunUntilIdle();
  query_controller.SendContextualTextQuery(
      kTestQueryText, lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  task_environment_->RunUntilIdle();
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
  ASSERT_FALSE(page_content_request.payload().content_data().empty());
  ASSERT_EQ(page_content_request.payload().content_type(), "text/plain");

  // Verify the page url was included in the request.
  ASSERT_EQ(page_content_request.payload().page_url(), kTestPageUrl);

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
  std::string encoded_vsint;
  bool has_vsint = net::GetValueForKeyInQuery(
      GURL(url_response_future.Get().url()),
      kVisualSearchInteractionDataQueryParameterKey, &encoded_vsint);
  ASSERT_TRUE(has_vsint);
  ASSERT_EQ(GetVsintFromUrl(url_response_future.Get().url())
                .log_data()
                .user_selection_data()
                .selection_type(),
            lens::MULTIMODAL_SEARCH);
  ASSERT_TRUE(has_start_time);
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
  ASSERT_EQ(latest_suggest_inputs_.encoded_image_signals(),
            kTestSuggestSignals);
  ASSERT_EQ(latest_suggest_inputs_.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(latest_suggest_inputs_.encoded_visual_search_interaction_log_data(),
            encoded_vsint);
  ASSERT_EQ(latest_suggest_inputs_.contextual_visual_input_type(), "wp");
  ASSERT_EQ(GetEncodedRequestIdFromUrl(url_response_future.Get().url()),
            latest_suggest_inputs_.encoded_request_id());
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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeContentBytes,
      lens::MimeType::kPlainText, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());

  // Send an empty partial page content request.
  query_controller.SendPartialPageContentRequest({});

  // Send the contextual text query.
  query_controller.SendContextualTextQuery(
      kTestQueryText, lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);
  task_environment_->RunUntilIdle();

  // Verify the query controller did not issue the search request yet.
  EXPECT_FALSE(url_response_future.IsReady());

  // Simulate the upload progress callback completing the upload.
  query_controller.RunUploadProgressCallback();
  task_environment_->RunUntilIdle();

  query_controller.EndQuery();

  // Verify the query controller issued the search request after the upload
  // completed.

  EXPECT_TRUE(url_response_future.IsReady());
}

TEST_F(LensOverlayQueryControllerTest,
       SendContextualTextQuery_WithScannedPdfPartialUpload_HoldsRequest) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeContentBytes,
      lens::MimeType::kPlainText, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());

  // Send a partial page content request that does not meet the per page
  // character limit to be considered as a non-scanned pdf. This emulates the
  // case where the users PDF is scanned and therefore the search query should
  // be held until the page content upload is finished.
  query_controller.SendPartialPageContentRequest(kShortPartialContent);

  // Send the contextual text query.
  query_controller.SendContextualTextQuery(
      kTestQueryText, lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
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
       SendContextualTextQuery_WithFullTextPdfPartialUpload_SendsRequest) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeContentBytes,
      lens::MimeType::kPlainText, 0, base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());

  // Send a partial page content request that does meet the per page
  // character limit to be considered as a non-scanned pdf.
  query_controller.SendPartialPageContentRequest(kLongPartialContent);

  // Send the contextual text query.
  query_controller.SendContextualTextQuery(
      kTestQueryText, lens::LensOverlaySelectionType::MULTIMODAL_SEARCH,
      additional_search_query_params);

  // Verify the query controller did issue the search request.
  ASSERT_TRUE(url_response_future.Wait());

  query_controller.EndQuery();
}

TEST_F(LensOverlayQueryControllerTest,
       SendPageContentUpdateRequest_IncrementsSequence) {
  InitFeaturesWithClusterInfoOptimization();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), std::vector<uint8_t>(),
      lens::MimeType::kUnknown, 0, base::TimeTicks::Now());

  // Immediately send a page content update request.
  query_controller.SendPageContentUpdateRequest(
      kFakeContentBytes, lens::MimeType::kPlainText, GURL(kTestPageUrl));
  task_environment_->RunUntilIdle();

  ASSERT_TRUE(full_image_response_future.IsReady());

  // The full image and page content requests should have the same request id.
  ASSERT_EQ(query_controller.sent_request_id().sequence_id(), 1);
  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            1);

  // Send a new page content update request.
  query_controller.SendPageContentUpdateRequest(
      kNewFakeContentBytes, lens::MimeType::kPlainText, GURL(kTestPageUrl));
  task_environment_->RunUntilIdle();

  // The new page content request should have a different sequence ID.
  ASSERT_EQ(query_controller.sent_request_id().sequence_id(), 1);
  ASSERT_EQ(query_controller.sent_page_content_objects_request()
                .request_context()
                .request_id()
                .sequence_id(),
            2);
}

TEST_F(LensOverlayQueryControllerTest,
       SendPartialPageContentRequest_SendsRequest) {
  InitFeaturesWithClusterInfoOptimization();
  TestLensOverlayQueryController query_controller(
      /**full_image_callback=*/base::DoNothing(),
      /**url_callback=*/base::DoNothing(),
      /**suggest_inputs_callback=*/base::DoNothing(),
      /**thumbnail_created_callback=*/base::DoNothing(),
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
      std::vector<lens::mojom::CenterRotatedBoxPtr>(), kFakeContentBytes,
      lens::MimeType::kPdf, 0, base::TimeTicks::Now());

  const std::vector<std::u16string> kFakePartialContent(
      {u"page1", u"page2", u"page3"});
  query_controller.SendPartialPageContentRequest(kFakePartialContent);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_partial_page_content_requests_sent() == 1;
  }));

  // Check the request is correct.
  auto sent_request =
      query_controller.sent_partial_page_content_objects_request();
  ASSERT_EQ(1, sent_request.request_context().request_id().sequence_id());
  ASSERT_EQ(lens::Payload::REQUEST_TYPE_EARLY_PARTIAL_PDF,
            sent_request.payload().request_type());
  ASSERT_EQ(3, sent_request.payload().partial_pdf_document().pages_size());
  ASSERT_EQ(
      1, sent_request.payload().partial_pdf_document().pages(0).page_number());
  ASSERT_EQ(
      "page1",
      sent_request.payload().partial_pdf_document().pages(0).text_segments(0));
  ASSERT_EQ(
      2, sent_request.payload().partial_pdf_document().pages(1).page_number());
  ASSERT_EQ(
      "page2",
      sent_request.payload().partial_pdf_document().pages(1).text_segments(0));
  ASSERT_EQ(
      3, sent_request.payload().partial_pdf_document().pages(2).page_number());
  ASSERT_EQ(
      "page3",
      sent_request.payload().partial_pdf_document().pages(2).text_segments(0));

  // Send a new page content request to incremement the sequence id.
  query_controller.SendPageContentUpdateRequest(
      kFakeContentBytes, lens::MimeType::kPdf, GURL(kTestPageUrl));

  // Send a new request.
  const std::vector<std::u16string> kNewFakePartialContent({u"new page1"});
  query_controller.SendPartialPageContentRequest(kNewFakePartialContent);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller.num_partial_page_content_requests_sent() == 2;
  }));

  // Check the request is correct.
  sent_request = query_controller.sent_partial_page_content_objects_request();
  ASSERT_EQ(2, sent_request.request_context().request_id().sequence_id());
  ASSERT_EQ(lens::Payload::REQUEST_TYPE_EARLY_PARTIAL_PDF,
            sent_request.payload().request_type());
  ASSERT_EQ(1, sent_request.payload().partial_pdf_document().pages_size());
  ASSERT_EQ(
      1, sent_request.payload().partial_pdf_document().pages(0).page_number());
  ASSERT_EQ(
      "new page1",
      sent_request.payload().partial_pdf_document().pages(0).text_segments(0));

  // Send an empty request.
  const std::vector<std::u16string> kEmptyPartialContent({});
  query_controller.ResetPageContentData();
  query_controller.SendPartialPageContentRequest(kEmptyPartialContent);

  // Check that no request is not sent.
  ASSERT_EQ(query_controller.num_partial_page_content_requests_sent(), 2);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchInteraction_UsesSameAnalyticsIdForLensRequestAndUrl) {
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr, bool>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());
  task_environment_->RunUntilIdle();

  ASSERT_TRUE(full_image_response_future.IsReady());
  std::string first_analytics_id =
      query_controller.sent_request_id().analytics_id();
  query_controller.SendRegionSearch(std::move(region), lens::REGION_SEARCH,
                                    additional_search_query_params,
                                    std::nullopt);
  task_environment_->RunUntilIdle();
  query_controller.EndQuery();

  ASSERT_TRUE(url_response_future.IsReady());
  ASSERT_TRUE(latest_suggest_inputs_.has_encoded_image_signals());
  std::string second_analytics_id =
      query_controller.sent_request_id().analytics_id();

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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());

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
      std::move(region), kTestQueryText, lens::MULTIMODAL_SEARCH,
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

TEST_F(LensOverlayQueryControllerTest,
       RoutingInfo_FromFullImageReseponse_IncludedInRequestId) {
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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendMultimodalRequest(
      std::move(region), kTestQueryText, lens::MULTIMODAL_SEARCH,
      additional_search_query_params, std::nullopt);
  task_environment_->RunUntilIdle();
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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      kTestServerSessionId);
  query_controller.set_fake_objects_response(fake_objects_response);

  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  std::map<std::string, std::string> additional_search_query_params;
  query_controller.StartQueryFlow(
      bitmap, GURL(kTestPageUrl),
      std::make_optional<std::string>(kTestPageTitle),
      std::vector<lens::mojom::CenterRotatedBoxPtr>(),
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown, 0,
      base::TimeTicks::Now());
  ASSERT_TRUE(full_image_response_future.Wait());

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendMultimodalRequest(
      std::move(region), kTestQueryText, lens::MULTIMODAL_SEARCH,
      additional_search_query_params, std::nullopt);
  task_environment_->RunUntilIdle();
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
  base::test::TestFuture<const std::string&> thumbnail_created_future;
  TestLensOverlayQueryController query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(), GetSuggestInputsCallback(),
      thumbnail_created_future.GetRepeatingCallback(),
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
      /*underlying_content_bytes=*/{}, lens::MimeType::kUnknown,
      /**/ 0, base::TimeTicks::Now());

  // Wait for the full image response to be received, then clear the future.
  ASSERT_TRUE(full_image_response_future.Wait());
  full_image_response_future.Clear();

  task_environment_->FastForwardBy(base::TimeDelta(base::Minutes(60)));
  query_controller.SendRegionSearch(std::move(region), lens::REGION_SEARCH,
                                    additional_search_query_params,
                                    std::nullopt);
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
}

}  // namespace lens
