// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/companion/exps_registration_success_observer.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace companion {

namespace {
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

constexpr char kDefaultUrl[] = "http://example.com";
constexpr char kBlocklistedUrl[] = "https://labs.google.com/search";
constexpr char kBlocklistedUrl2[] = "https://labs.google.com/search/playground";
constexpr char kExpsRegistationSuccessUrl[] = "https://labs.google.com/search";

class MockExpsRegistrationSuccessObserver
    : public ExpsRegistrationSuccessObserver {
 public:
  explicit MockExpsRegistrationSuccessObserver(
      content::WebContents* web_contents)
      : ExpsRegistrationSuccessObserver(web_contents) {}
  ~MockExpsRegistrationSuccessObserver() override {}

  MOCK_METHOD0(IsSearchInCompanionSidePanelSupported, bool());
  MOCK_METHOD0(pref_service, PrefService*());
  MOCK_METHOD0(ShowIPH, void());
};

class ExpsRegistrationSuccessObserverTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  void SetupExpsObserver() {
    exps_observer_ =
        std::make_unique<MockExpsRegistrationSuccessObserver>(web_contents());
    EXPECT_CALL(*exps_observer_, pref_service())
        .WillRepeatedly(testing::Return(&pref_service_));
    EXPECT_CALL(*exps_observer_, IsSearchInCompanionSidePanelSupported())
        .WillRepeatedly(testing::Return(true));
  }

  void SetPrefValues(bool exps_granted,
                     bool has_seen_success_page,
                     bool pinned_entry) {
    pref_service_.registry()->RegisterBooleanPref(
        companion::kExpsOptInStatusGrantedPref, exps_granted);
    pref_service_.registry()->RegisterBooleanPref(
        companion::kHasNavigatedToExpsSuccessPage, has_seen_success_page);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSidePanelCompanionEntryPinnedToToolbar, pinned_entry);
  }

  void SetUpFeatureList() {
    base::FieldTrialParams enabled_params;
    enabled_params["exps-registration-success-page-urls"] =
        base::StrCat({kExpsRegistationSuccessUrl});
    enabled_params["companion-iph-blocklisted-page-urls"] =
        base::StrCat({kBlocklistedUrl, ",", kBlocklistedUrl2});

    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.emplace_back(
        features::internal::kCompanionEnabledByObservingExpsNavigations,
        enabled_params);
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<MockExpsRegistrationSuccessObserver> exps_observer_;
};

}  // namespace

TEST_F(ExpsRegistrationSuccessObserverTest, ShowIPHIfAllCriteriaMeets) {
  SetUpFeatureList();
  SetPrefValues(/*exps_granted=*/true,
                /*has_seen_success_page=*/true, /*pinned_entry=*/true);
  SetupExpsObserver();

  EXPECT_CALL(*exps_observer_, ShowIPH()).Times(1);
  NavigateAndCommit(GURL(kDefaultUrl));
}

TEST_F(ExpsRegistrationSuccessObserverTest, IPHNotShownForChromeUrls) {
  SetUpFeatureList();
  SetPrefValues(/*exps_granted=*/true,
                /*has_seen_success_page=*/true, /*pinned_entry=*/true);
  SetupExpsObserver();

  EXPECT_CALL(*exps_observer_, ShowIPH()).Times(0);
  NavigateAndCommit(GURL("chrome://newtab"));
}

TEST_F(ExpsRegistrationSuccessObserverTest, IPHNotShownIfCscNotPinned) {
  SetUpFeatureList();
  SetPrefValues(/*exps_granted=*/true,
                /*has_seen_success_page=*/true, /*pinned_entry=*/false);
  SetupExpsObserver();

  EXPECT_CALL(*exps_observer_, ShowIPH()).Times(0);
  NavigateAndCommit(GURL(kDefaultUrl));
}

TEST_F(ExpsRegistrationSuccessObserverTest, IPHNotShownForBlockListedUrls) {
  SetUpFeatureList();
  SetPrefValues(/*exps_granted=*/true,
                /*has_seen_success_page=*/true, /*pinned_entry=*/true);
  SetupExpsObserver();

  EXPECT_CALL(*exps_observer_, ShowIPH()).Times(0);
  NavigateAndCommit(GURL(kBlocklistedUrl));
}

TEST_F(ExpsRegistrationSuccessObserverTest,
       ExpsRegistrationSuccessUpdatesThePref) {
  SetUpFeatureList();
  SetPrefValues(/*exps_granted=*/false,
                /*has_seen_success_page=*/false, /*pinned_entry=*/false);
  SetupExpsObserver();

  EXPECT_CALL(*exps_observer_, ShowIPH()).Times(0);
  NavigateAndCommit(GURL(kExpsRegistationSuccessUrl));

  EXPECT_TRUE(
      pref_service_.GetBoolean(companion::kHasNavigatedToExpsSuccessPage));
}

TEST_F(ExpsRegistrationSuccessObserverTest,
       RandomUrlDoesntUpdateExpsRegistrationPref) {
  SetUpFeatureList();
  SetPrefValues(/*exps_granted=*/false,
                /*has_seen_success_page=*/false, /*pinned_entry=*/false);
  SetupExpsObserver();

  EXPECT_CALL(*exps_observer_, ShowIPH()).Times(0);
  NavigateAndCommit(GURL(kDefaultUrl));

  EXPECT_FALSE(
      pref_service_.GetBoolean(companion::kHasNavigatedToExpsSuccessPage));
}

TEST_F(ExpsRegistrationSuccessObserverTest, MatchURL) {
  SetupExpsObserver();

  std::vector<std::string> url_patterns;
  url_patterns.emplace_back("https://labs.google.com/search/experiment");
  url_patterns.emplace_back("https://labs.google.com/search/otherexperiment");

  // Empty URL.
  EXPECT_FALSE(
      exps_observer_->DoesUrlMatchPatternsInList(GURL(), url_patterns));
  EXPECT_FALSE(exps_observer_->DoesUrlMatchPatternsInList(
      GURL("http://example.com"), url_patterns));

  // Valid URLs.
  EXPECT_TRUE(exps_observer_->DoesUrlMatchPatternsInList(
      GURL("https://labs.google.com/search/experiment"), url_patterns));
  EXPECT_TRUE(exps_observer_->DoesUrlMatchPatternsInList(
      GURL("https://labs.google.com/search/experiment?q=some_val"),
      url_patterns));
  EXPECT_TRUE(exps_observer_->DoesUrlMatchPatternsInList(
      GURL("https://labs.google.com/search/experiment#fragment1"),
      url_patterns));
  EXPECT_TRUE(exps_observer_->DoesUrlMatchPatternsInList(
      GURL("https://labs.google.com/search/experiment/v3"), url_patterns));
  EXPECT_TRUE(exps_observer_->DoesUrlMatchPatternsInList(
      GURL("https://labs.google.com/search/experiment2"), url_patterns));

  // Valid URLs matching pattern 2.
  EXPECT_TRUE(exps_observer_->DoesUrlMatchPatternsInList(
      GURL("https://labs.google.com/search/otherexperiment"), url_patterns));
  EXPECT_TRUE(exps_observer_->DoesUrlMatchPatternsInList(
      GURL("https://labs.google.com/search/otherexperiment/v3"), url_patterns));
}

}  // namespace companion
