// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lens_overlay_gen204_controller.h"

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
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

// The gen204 id for testing.
constexpr uint64_t kGen204Id = 0;

// The test invocation source.
const lens::LensOverlayInvocationSource kInvocationSource =
    lens::LensOverlayInvocationSource::kAppMenu;

// The test encoded analytics id.
constexpr char kEncodedAnalyticsId[] = "test";

// Query parameter keys.
constexpr char kGen204IdentifierQueryParameter[] = "plla";
constexpr char kSemanticEventTimestampParameter[] = "zx";
constexpr char kSemanticEventIdParameter[] = "rid";
constexpr char kUserActionParameter[] = "uact";

// Semantic event ids.
constexpr int kTextGleamsViewStartSemanticEventID = 234181;
constexpr int kTextGleamsViewEndSemanticEventID = 234180;

// TODO(crbug/369687023): Unit tests for latency and task completion events.
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
  std::optional<lens::mojom::SemanticEvent> GetSemanticEventFromUrl(GURL url) {
    std::string event_id_param;
    EXPECT_TRUE(net::GetValueForKeyInQuery(url, kSemanticEventIdParameter,
                                           &event_id_param));
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

}  // namespace lens
