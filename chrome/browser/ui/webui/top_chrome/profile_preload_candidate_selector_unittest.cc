// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/profile_preload_candidate_selector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/top_chrome/per_profile_webui_tracker.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_constants.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using site_engagement::SiteEngagementScore;
using site_engagement::SiteEngagementService;
using testing::_;
using testing::Return;

namespace {

// The mock webui tracker simulates the presence state of WebUIs under
// a profile.
class MockPerProfileWebUITracker : public PerProfileWebUITracker {
 public:
  MOCK_METHOD(void, AddWebContents, (content::WebContents*), (override));
  MOCK_METHOD(bool,
              ProfileHasWebUI,
              (Profile*, std::string webui_url),
              (const, override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
};

// kWebUIUrl1 and kWebUIUrl2 are the two preloadable WebUIs set up
// by this test harness.
// The trailing '/' is important due to the assumption that
// url == GURL(url).spec(), which simplifies mocking the WebUI tracker.
constexpr char kWebUIUrl1[] = "chrome://example1/";
constexpr char kWebUIUrl2[] = "chrome://example2/";

class TestWebUIController1 : public TopChromeWebUIController {
 public:
  explicit TestWebUIController1(content::WebUI* web_ui)
      : TopChromeWebUIController(web_ui) {}

  static constexpr std::string GetWebUIName() { return "Test1"; }
};

class TestWebUIConfig1
    : public DefaultTopChromeWebUIConfig<TestWebUIController1> {
 public:
  explicit TestWebUIConfig1(bool& enabled)
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme, "example1"),
        enabled_(enabled) {}

  // DefaultWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* context) override {
    return *enabled_;
  }

 private:
  raw_ref<bool> enabled_;
};

class TestWebUIController2 : public TopChromeWebUIController {
 public:
  explicit TestWebUIController2(content::WebUI* web_ui)
      : TopChromeWebUIController(web_ui) {}

  static constexpr std::string GetWebUIName() { return "Test2"; }
};

class TestWebUIConfig2
    : public DefaultTopChromeWebUIConfig<TestWebUIController2> {
 public:
  TestWebUIConfig2()
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme, "example2") {}
};

}  // namespace

namespace webui {

class ProfilePreloadCandidateSelectorTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ProfilePreloadCandidateSelectorTest()
    : registration1_(std::make_unique<TestWebUIConfig1>(enabled_webui_1_)),
      registration2_(std::make_unique<TestWebUIConfig2>()) {}
  ~ProfilePreloadCandidateSelectorTest() override = default;
  ProfilePreloadCandidateSelectorTest(
      const ProfilePreloadCandidateSelectorTest&) = delete;
  ProfilePreloadCandidateSelectorTest& operator=(
      const ProfilePreloadCandidateSelectorTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    candidate_selector_ =
        std::make_unique<ProfilePreloadCandidateSelector>(&mock_webui_tracker_);
    candidate_selector_->Init(GetAllPreloableURLs());

    // Assumes that no WebUIs are present. This may be overridden by individual
    // tests.
    ON_CALL(mock_webui_tracker(), ProfileHasWebUI(_, _))
        .WillByDefault(Return(false));
  }

  const std::vector<GURL>& GetAllPreloableURLs() const {
    // GURL cannot be static variables in global scope.
    // See DoSchemeModificationPreamble() in url/url_util.cc.
    static const std::vector<GURL> s_all_preloable_urls{GURL(kWebUIUrl1),
                                                        GURL(kWebUIUrl2)};
    return s_all_preloable_urls;
  }

  std::optional<GURL> GetURLToPreload(Profile* profile) {
    return candidate_selector_->GetURLToPreload(PreloadContext::From(profile));
  }

  MockPerProfileWebUITracker& mock_webui_tracker() {
    return mock_webui_tracker_;
  }

  void SetEngagementScore(Profile* profile, GURL url, double score) {
    SiteEngagementService* service = SiteEngagementService::Get(profile);
    ASSERT_TRUE(service);
    service->ResetBaseScoreForURL(url, score);
  }

 protected:
  bool enabled_webui_1_ = true;

 private:
  // Use NiceMock because the exact interaction between the candidate selector
  // and the WebUI tracker is an implementation detail that we don't care.
  // The tests only use the mock tracker to simulate presence state of WebUIs.
  testing::NiceMock<MockPerProfileWebUITracker> mock_webui_tracker_;
  std::unique_ptr<ProfilePreloadCandidateSelector> candidate_selector_;
  content::ScopedWebUIConfigRegistration registration1_, registration2_;
};

// Tests that the candidate selector does not select WebUIs that are present.
TEST_F(ProfilePreloadCandidateSelectorTest, IgnorePresentWebUI) {
  // Set engagement scores to maximum, so that the selector won't reject a URL
  // due to its low engagemen score.
  SetEngagementScore(profile(), GURL(kWebUIUrl1),
                     SiteEngagementService::GetMaxPoints());
  SetEngagementScore(profile(), GURL(kWebUIUrl2),
                     SiteEngagementService::GetMaxPoints());

  // By default no WebUI is present, selects either URL1 or URL2.
  EXPECT_TRUE(
      base::Contains(GetAllPreloableURLs(), *GetURLToPreload(profile())));

  // If URL1 is present, selects URL2.
  ON_CALL(mock_webui_tracker(), ProfileHasWebUI(_, kWebUIUrl1))
      .WillByDefault(Return(true));
  EXPECT_EQ(GURL(kWebUIUrl2), *GetURLToPreload(profile()));

  // If both URL1 and URL2 are present, selects nothing.
  ON_CALL(mock_webui_tracker(), ProfileHasWebUI(_, kWebUIUrl2))
      .WillByDefault(Return(true));
  EXPECT_FALSE(GetURLToPreload(profile()).has_value());
}

// Tests that the candidate selector does not select WebUIs with low
// engagement level.
TEST_F(ProfilePreloadCandidateSelectorTest, IgnoreLowEngagementWebUIs) {
  // Set engagement scores to 0, so that the selector should ignore all URLs
  // due to low engagement level.
  SetEngagementScore(profile(), GURL(kWebUIUrl1), 0);
  SetEngagementScore(profile(), GURL(kWebUIUrl2), 0);
  EXPECT_FALSE(GetURLToPreload(profile()).has_value());

  // Sets URL1 to maximum engagement score so that the selector does not ignore
  // it.
  SetEngagementScore(profile(), GURL(kWebUIUrl1),
                     SiteEngagementService::GetMaxPoints());
  SetEngagementScore(profile(), GURL(kWebUIUrl2), 0);
  EXPECT_EQ(GURL(kWebUIUrl1), *GetURLToPreload(profile()));

  // Sets URL2 to maximum engagement score so that the selector does not ignore
  // it.
  SetEngagementScore(profile(), GURL(kWebUIUrl1), 0);
  SetEngagementScore(profile(), GURL(kWebUIUrl2),
                     SiteEngagementService::GetMaxPoints());
  EXPECT_EQ(GURL(kWebUIUrl2), *GetURLToPreload(profile()));
}

// Tests that the candidate selector selects the WebUI with the highest
// engagement score.
TEST_F(ProfilePreloadCandidateSelectorTest, PreferHighEngagementWebUI) {
  // Sets a higher engagement score for URL1, the selector should select URL1.
  const double high_engagement_score =
      SiteEngagementScore::GetHighEngagementBoundary();
  SetEngagementScore(profile(), GURL(kWebUIUrl1), high_engagement_score + 1);
  SetEngagementScore(profile(), GURL(kWebUIUrl2), high_engagement_score);
  EXPECT_EQ(GURL(kWebUIUrl1), *GetURLToPreload(profile()));

  // Sets a higher engagement score for URL2, the selector should select URL2.
  SetEngagementScore(profile(), GURL(kWebUIUrl1), high_engagement_score);
  SetEngagementScore(profile(), GURL(kWebUIUrl2), high_engagement_score + 1);
  EXPECT_EQ(GURL(kWebUIUrl2), *GetURLToPreload(profile()));
}

TEST_F(ProfilePreloadCandidateSelectorTest,
       IgnoreDisabledWebUIs) {
  // Set engagement scores to maximum, so that the selector won't reject a URL
  // due to its low engagemen score.
  SetEngagementScore(profile(), GURL(kWebUIUrl1),
                     SiteEngagementService::GetMaxPoints());
  // Set URL2 to have a lower engagement score than URL1, so that when URL2 is
  // selected, it is not due to it having a higher engagement score.
  SetEngagementScore(profile(), GURL(kWebUIUrl2),
                     SiteEngagementService::GetMaxPoints()-1);

  // By default no WebUI is present, selects either URL1 or URL2.
  EXPECT_TRUE(
      base::Contains(GetAllPreloableURLs(), *GetURLToPreload(profile())));

  // If URL1 is disabled, selects URL2.
  base::AutoReset<bool> disable_webui_1(&enabled_webui_1_, false);
  EXPECT_EQ(GURL(kWebUIUrl2), *GetURLToPreload(profile()));
}

}  // namespace webui
