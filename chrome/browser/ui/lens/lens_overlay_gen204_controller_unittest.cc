// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lens_overlay_gen204_controller.h"

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/lens/core/mojom/lens.mojom-shared.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/base32/base32.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {

using LatencyType = LensOverlayGen204Controller::LatencyType;

// The gen204 id for testing.
constexpr uint64_t kGen204Id = 0;

// The test latency.
constexpr base::TimeDelta kRequestLatency = base::Milliseconds(100);
constexpr base::TimeDelta kClusterInfoLatency = base::Milliseconds(200);

// The test invocation source.
const lens::LensOverlayInvocationSource kInvocationSource =
    lens::LensOverlayInvocationSource::kAppMenu;

// The test encoded analytics id.
constexpr char kEncodedAnalyticsId[] = "test";

// Query parameter keys.
constexpr char kEncodedAnalyticsIdParameter[] = "cad";
constexpr char kGen204IdentifierQueryParameter[] = "plla";
constexpr char kSemanticEventTimestampParameter[] = "zx";
constexpr char kLatencyRequestTypeQueryParameter[] = "rt";
constexpr char kVisualInputTypeQueryParameter[] = "vit";
constexpr char kUserActionParameter[] = "uact";
// Event id param used for both semantic events and task completions.
constexpr char kEventIdParameter[] = "rcid";

// Task completion ids.
constexpr int kCopyAsImageTaskCompletionID = 233325;
constexpr int kCopyTextTaskCompletionID = 198153;
constexpr int kSaveAsImageTaskCompletionID = 233326;
constexpr int kSelectTextTaskCompletionID = 198157;
constexpr int kTranslateTaskCompletionID = 198158;

// Semantic event ids.
constexpr int kTextGleamsViewStartSemanticEventID = 234181;
constexpr int kTextGleamsViewEndSemanticEventID = 234180;

class LensOverlayGen204ControllerMock : public LensOverlayGen204Controller {
 public:
  LensOverlayGen204ControllerMock() = default;
  ~LensOverlayGen204ControllerMock() override = default;

  int num_gen204s_sent_ = 0;

  // The last gen204 url sent.
  GURL last_url_sent_;

 protected:
  void CheckMetricsConsentAndIssueGen204NetworkRequest(GURL url) override {
    num_gen204s_sent_++;
    last_url_sent_ = url;
  }
};

class LensOverlayGen204ControllerTest : public testing::Test {
 public:
  std::optional<lens::mojom::UserAction> GetTaskCompletionIdFromUrl(GURL url) {
    std::string event_id_param;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(url, kEventIdParameter, &event_id_param));
    int event_id;
    base::StringToInt(event_id_param, &event_id);
    switch (event_id) {
      case kCopyAsImageTaskCompletionID:
        return std::make_optional<lens::mojom::UserAction>(
            lens::mojom::UserAction::kCopyAsImage);
      case kCopyTextTaskCompletionID:
        return std::make_optional<lens::mojom::UserAction>(
            lens::mojom::UserAction::kCopyText);
      case kSaveAsImageTaskCompletionID:
        return std::make_optional<lens::mojom::UserAction>(
            lens::mojom::UserAction::kSaveAsImage);
      case kSelectTextTaskCompletionID:
        return std::make_optional<lens::mojom::UserAction>(
            lens::mojom::UserAction::kTextSelection);
      case kTranslateTaskCompletionID:
        return std::make_optional<lens::mojom::UserAction>(
            lens::mojom::UserAction::kTranslateText);
      default:
        return std::nullopt;
    }
  }

  std::optional<lens::mojom::SemanticEvent> GetSemanticEventFromUrl(GURL url) {
    std::string event_id_param;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(url, kEventIdParameter, &event_id_param));
    int event_id;
    base::StringToInt(event_id_param, &event_id);
    switch (event_id) {
      case kTextGleamsViewStartSemanticEventID:
        return std::make_optional<lens::mojom::SemanticEvent>(
            lens::mojom::SemanticEvent::kTextGleamsViewStart);
      case kTextGleamsViewEndSemanticEventID:
        return std::make_optional<lens::mojom::SemanticEvent>(
            lens::mojom::SemanticEvent::kTextGleamsViewEnd);
      default:
        return std::nullopt;
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  TestingProfile* profile() { return profile_.get(); }

 private:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    profile_ = profile_builder.Build();
  }
};

TEST_F(LensOverlayGen204ControllerTest,
       LatencyGen204IfEnabled_AttachesAllParams) {
  auto gen204_controller = std::make_unique<LensOverlayGen204ControllerMock>();
  gen204_controller->OnQueryFlowStart(kInvocationSource, profile(), kGen204Id);
  gen204_controller->SendLatencyGen204IfEnabled(
      LatencyType::kFullPageObjectsRequestFetchLatency, kRequestLatency,
      /*vit_query_param_value=*/"image",
      /*cluster_info_latency=*/std::nullopt,
      /*encoded_analytics_id=*/std::nullopt);

  auto url = gen204_controller->last_url_sent_;

  // Check that the gen204 id param is present.
  std::string gen204_id_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, kGen204IdentifierQueryParameter,
                                         &gen204_id_param));

  // Check that the request type param is present and contains the latency.
  std::string request_type_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, kLatencyRequestTypeQueryParameter,
                                         &request_type_param));
  ASSERT_EQ(request_type_param, "fpof.100");

  // Check that the visual input type param is present.
  std::string visual_input_type_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, kVisualInputTypeQueryParameter,
                                         &visual_input_type_param));
  ASSERT_EQ(visual_input_type_param, "image");

  ASSERT_EQ(gen204_controller->num_gen204s_sent_, 1);

  // Send a translate query.
  gen204_controller->SendLatencyGen204IfEnabled(
      LatencyType::kFullPageTranslateRequestFetchLatency, kRequestLatency,
      /*vit_query_param_value=*/"pdf",
      /*cluster_info_latency=*/std::nullopt,
      /*encoded_analytics_id=*/std::nullopt);

  // Check that the new request type param is present and contains the latency.
  EXPECT_TRUE(net::GetValueForKeyInQuery(gen204_controller->last_url_sent_,
                                         kLatencyRequestTypeQueryParameter,
                                         &request_type_param));
  ASSERT_EQ(request_type_param, "fptf.100");

  // Check that the visual input type param is present.
  EXPECT_TRUE(net::GetValueForKeyInQuery(gen204_controller->last_url_sent_,
                                         kVisualInputTypeQueryParameter,
                                         &visual_input_type_param));
  ASSERT_EQ(visual_input_type_param, "pdf");

  ASSERT_EQ(gen204_controller->num_gen204s_sent_, 2);

  // Send an objects query with cluster info.
  gen204_controller->SendLatencyGen204IfEnabled(
      LatencyType::kFullPageObjectsRequestFetchLatency, kRequestLatency,
      /*vit_query_param_value=*/"wp",
      std::make_optional<base::TimeDelta>(kClusterInfoLatency),
      /*encoded_analytics_id=*/std::nullopt);

  // Check that the new request type param is present and contains the latency.
  EXPECT_TRUE(net::GetValueForKeyInQuery(gen204_controller->last_url_sent_,
                                         kLatencyRequestTypeQueryParameter,
                                         &request_type_param));
  ASSERT_EQ(request_type_param, "fpof.100,sct.200");

  // Check that the visual input type param is present.
  EXPECT_TRUE(net::GetValueForKeyInQuery(gen204_controller->last_url_sent_,
                                         kVisualInputTypeQueryParameter,
                                         &visual_input_type_param));
  ASSERT_EQ(visual_input_type_param, "wp");

  ASSERT_EQ(gen204_controller->num_gen204s_sent_, 3);

  // Send an objects query with an encoded analytics id.
  gen204_controller->SendLatencyGen204IfEnabled(
      LatencyType::kFullPageObjectsRequestFetchLatency, kRequestLatency,
      /*vit_query_param_value=*/"wp",
      std::make_optional<base::TimeDelta>(kClusterInfoLatency),
      std::make_optional<std::string>(kEncodedAnalyticsId));

  // Check that the encoded analytics id param is present.
  std::string encoded_analytics_id_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(gen204_controller->last_url_sent_,
                                         kEncodedAnalyticsIdParameter,
                                         &encoded_analytics_id_param));
  ASSERT_EQ(encoded_analytics_id_param, kEncodedAnalyticsId);

  ASSERT_EQ(gen204_controller->num_gen204s_sent_, 4);
}

TEST_F(LensOverlayGen204ControllerTest,
       TaskCompletionGen204IfEnabled_AttachesAllParams) {
  auto gen204_controller = std::make_unique<LensOverlayGen204ControllerMock>();
  gen204_controller->OnQueryFlowStart(kInvocationSource, profile(), kGen204Id);
  gen204_controller->SendTaskCompletionGen204IfEnabled(
      kEncodedAnalyticsId, lens::mojom::UserAction::kCopyText);

  auto url = gen204_controller->last_url_sent_;
  EXPECT_THAT(GetTaskCompletionIdFromUrl(url),
              testing::Optional(lens::mojom::UserAction::kCopyText));

  // Check for the uact param.
  std::string uact_param;
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(url, kUserActionParameter, &uact_param));
  ASSERT_EQ(uact_param, "4");

  // Check that the gen204 id param is present.
  std::string gen204_id_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, kGen204IdentifierQueryParameter,
                                         &gen204_id_param));

  // Check that the encoded analytics id param is correct.
  std::string encoded_analytics_id_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, kEncodedAnalyticsIdParameter,
                                         &encoded_analytics_id_param));
  EXPECT_EQ(encoded_analytics_id_param, kEncodedAnalyticsId);

  ASSERT_EQ(gen204_controller->num_gen204s_sent_, 1);
}

TEST_F(LensOverlayGen204ControllerTest,
       TaskCompletionGen204IfEnabled_AttachesCorrectEventId) {
  auto gen204_controller = std::make_unique<LensOverlayGen204ControllerMock>();
  gen204_controller->OnQueryFlowStart(kInvocationSource, profile(), kGen204Id);

  gen204_controller->SendTaskCompletionGen204IfEnabled(
      kEncodedAnalyticsId, lens::mojom::UserAction::kCopyText);
  EXPECT_THAT(GetTaskCompletionIdFromUrl(gen204_controller->last_url_sent_),
              testing::Optional(lens::mojom::UserAction::kCopyText));

  gen204_controller->SendTaskCompletionGen204IfEnabled(
      kEncodedAnalyticsId, lens::mojom::UserAction::kTranslateText);
  EXPECT_THAT(GetTaskCompletionIdFromUrl(gen204_controller->last_url_sent_),
              testing::Optional(lens::mojom::UserAction::kTranslateText));

  gen204_controller->SendTaskCompletionGen204IfEnabled(
      kEncodedAnalyticsId, lens::mojom::UserAction::kCopyAsImage);
  EXPECT_THAT(GetTaskCompletionIdFromUrl(gen204_controller->last_url_sent_),
              testing::Optional(lens::mojom::UserAction::kCopyAsImage));

  gen204_controller->SendTaskCompletionGen204IfEnabled(
      kEncodedAnalyticsId, lens::mojom::UserAction::kSaveAsImage);
  EXPECT_THAT(GetTaskCompletionIdFromUrl(gen204_controller->last_url_sent_),
              testing::Optional(lens::mojom::UserAction::kSaveAsImage));

  gen204_controller->SendTaskCompletionGen204IfEnabled(
      kEncodedAnalyticsId, lens::mojom::UserAction::kTextSelection);
  EXPECT_THAT(GetTaskCompletionIdFromUrl(gen204_controller->last_url_sent_),
              testing::Optional(lens::mojom::UserAction::kTextSelection));
}

TEST_F(LensOverlayGen204ControllerTest,
       SemanticEventGen204IfEnabled_OnQueryFlowEndSendsTextEndEvent) {
  auto gen204_controller = std::make_unique<LensOverlayGen204ControllerMock>();
  gen204_controller->OnQueryFlowStart(kInvocationSource, profile(), kGen204Id);
  gen204_controller->SendSemanticEventGen204IfEnabled(
      lens::mojom::SemanticEvent::kTextGleamsViewStart);

  EXPECT_THAT(
      GetSemanticEventFromUrl(gen204_controller->last_url_sent_),
      testing::Optional(lens::mojom::SemanticEvent::kTextGleamsViewStart));
  ASSERT_EQ(gen204_controller->num_gen204s_sent_, 1);

  gen204_controller->OnQueryFlowEnd(kEncodedAnalyticsId);

  // The query flow ending should cause another gen204 event to fire.
  EXPECT_THAT(
      GetSemanticEventFromUrl(gen204_controller->last_url_sent_),
      testing::Optional(lens::mojom::SemanticEvent::kTextGleamsViewEnd));
  ASSERT_EQ(gen204_controller->num_gen204s_sent_, 2);
}

TEST_F(LensOverlayGen204ControllerTest,
       SemanticEventGen204IfEnabled_AttachesAllParams) {
  auto gen204_controller = std::make_unique<LensOverlayGen204ControllerMock>();
  gen204_controller->OnQueryFlowStart(kInvocationSource, profile(), kGen204Id);
  gen204_controller->SendSemanticEventGen204IfEnabled(
      lens::mojom::SemanticEvent::kTextGleamsViewStart);

  auto url = gen204_controller->last_url_sent_;
  EXPECT_THAT(
      GetSemanticEventFromUrl(url),
      testing::Optional(lens::mojom::SemanticEvent::kTextGleamsViewStart));

  // Check for the uact param.
  std::string uact_param;
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(url, kUserActionParameter, &uact_param));
  ASSERT_EQ(uact_param, "1");

  // Check that the timestamp param is present.
  std::string timestamp_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, kSemanticEventTimestampParameter,
                                         &timestamp_param));

  // Check that the gen204 id param is present.
  std::string gen204_id_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, kGen204IdentifierQueryParameter,
                                         &gen204_id_param));

  ASSERT_EQ(gen204_controller->num_gen204s_sent_, 1);
}

TEST_F(LensOverlayGen204ControllerTest,
       SemanticEventGen204IfEnabled_AttachesCorrectEventId) {
  auto gen204_controller = std::make_unique<LensOverlayGen204ControllerMock>();
  gen204_controller->OnQueryFlowStart(kInvocationSource, profile(), kGen204Id);

  gen204_controller->SendSemanticEventGen204IfEnabled(
      lens::mojom::SemanticEvent::kTextGleamsViewStart);
  EXPECT_THAT(
      GetSemanticEventFromUrl(gen204_controller->last_url_sent_),
      testing::Optional(lens::mojom::SemanticEvent::kTextGleamsViewStart));

  gen204_controller->SendSemanticEventGen204IfEnabled(
      lens::mojom::SemanticEvent::kTextGleamsViewEnd);
  EXPECT_THAT(
      GetSemanticEventFromUrl(gen204_controller->last_url_sent_),
      testing::Optional(lens::mojom::SemanticEvent::kTextGleamsViewEnd));
}

}  // namespace lens
