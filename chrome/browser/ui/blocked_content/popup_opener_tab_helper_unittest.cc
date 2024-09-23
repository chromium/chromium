// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/popup_opener_tab_helper.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/popup_tracker.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/framebust_intervention/framebust_blocked_delegate_android.h"
#include "components/infobars/android/infobar_android.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#else
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#endif

namespace infobars {
class InfoBarAndroid;
}

class PopupOpenerTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  PopupOpenerTabHelperTest() : ChromeRenderViewHostTestHarness() {}

  PopupOpenerTabHelperTest(const PopupOpenerTabHelperTest&) = delete;
  PopupOpenerTabHelperTest& operator=(const PopupOpenerTabHelperTest&) = delete;

  ~PopupOpenerTabHelperTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    blocked_content::PopupOpenerTabHelper::CreateForWebContents(
        web_contents(), &raw_clock_,
        HostContentSettingsMapFactory::GetForProfile(profile()));
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
    blocked_content::PopupBlockerTabHelper::CreateForWebContents(
        web_contents());
#if BUILDFLAG(IS_ANDROID)
    message_dispatcher_bridge_.SetMessagesEnabledForEmbedder(true);
    messages::MessageDispatcherBridge::SetInstanceForTesting(
        &message_dispatcher_bridge_);
    blocked_content::FramebustBlockedMessageDelegate::CreateForWebContents(
        web_contents());
    framebust_blocked_message_delegate_ =
        blocked_content::FramebustBlockedMessageDelegate::FromWebContents(
            web_contents());
#endif
#if !BUILDFLAG(IS_ANDROID)
    FramebustBlockTabHelper::CreateForWebContents(web_contents());
#endif

    // The tick clock needs to be advanced manually so it isn't set to null,
    // which the code uses to determine if it is set yet.
    raw_clock_.Advance(base::Milliseconds(1));

    EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  }

  void TearDown() override {
    popups_.clear();
#if BUILDFLAG(IS_ANDROID)
    messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Returns the RenderFrameHost the navigation commit in, or nullptr if the
  // navigation failed.
  content::RenderFrameHost* NavigateAndCommitWithoutGesture(const GURL& url) {
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
    simulator->SetHasUserGesture(false);
    simulator->Commit();
    return simulator->GetLastThrottleCheckResult().action() ==
                   content::NavigationThrottle::PROCEED
               ? simulator->GetFinalRenderFrameHost()
               : nullptr;
  }

  // Simulates a popup opened by |web_contents()|.
  content::WebContents* SimulatePopup() {
    std::unique_ptr<content::WebContents> popup(CreateTestWebContents());
    content::WebContents* raw_popup = popup.get();
    popups_.push_back(std::move(popup));

    blocked_content::PopupTracker::CreateForWebContents(
        raw_popup, web_contents() /* opener */,
        WindowOpenDisposition::NEW_POPUP);
    web_contents()->WasHidden();
    raw_popup->WasShown();
    return raw_popup;
  }

  base::SimpleTestTickClock* raw_clock() { return &raw_clock_; }

#if BUILDFLAG(IS_ANDROID)
  messages::MessageWrapper* message_wrapper() {
    return framebust_blocked_message_delegate_->message_for_testing();
  }
#endif

  void expect_message() {
#if BUILDFLAG(IS_ANDROID)
    EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
        .WillOnce(testing::Return(true));
#endif
  }

 private:
  base::SimpleTestTickClock raw_clock_;
  std::vector<std::unique_ptr<content::WebContents>> popups_;
#if BUILDFLAG(IS_ANDROID)
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  raw_ptr<blocked_content::FramebustBlockedMessageDelegate>
      framebust_blocked_message_delegate_;
#endif
};

// Navigate to a site without pop-ups, verify the user popup settings are not
// logged to ukm.
TEST_F(PopupOpenerTabHelperTest,
       PageWithNoPopups_NoProfileSettingsLoggedInUkm) {
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  const GURL url("https://first.test/");
  NavigateAndCommitWithoutGesture(url);
  DeleteContents();

  auto entries =
      test_ukm_recorder.GetEntriesByName(ukm::builders::Popup_Page::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

// Navigate to a site, verify histogram for user site pop up settings
// when there is at least one popup.
TEST_F(PopupOpenerTabHelperTest,
       PageWithDefaultPopupBlocker_ProfileSettingsLoggedInUkm) {
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  const GURL url("https://first.test/");
  NavigateAndCommitWithoutGesture(url);
  SimulatePopup();
  DeleteContents();

  auto entries =
      test_ukm_recorder.GetEntriesByName(ukm::builders::Popup_Page::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder.ExpectEntrySourceHasUrl(entries[0], url);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Popup_Page::kAllowedName, false);
}

// Same as the test with the default pop up blocker, however, with the user
// explicitly allowing popups on the site.
TEST_F(PopupOpenerTabHelperTest,
       PageWithPopupsAllowed_ProfileSettingsLoggedInUkm) {
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  const GURL url("https://first.test/");

  // Allow popups on url for the test profile.
  TestingProfile* test_profile = profile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(test_profile);
  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);

  NavigateAndCommitWithoutGesture(url);
  SimulatePopup();
  DeleteContents();

  auto entries =
      test_ukm_recorder.GetEntriesByName(ukm::builders::Popup_Page::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder.ExpectEntrySourceHasUrl(entries[0], url);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Popup_Page::kAllowedName, true);
}

class BlockTabUnderTest : public PopupOpenerTabHelperTest {
 public:
  BlockTabUnderTest() {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(kBlockTabUnders);
  }

  BlockTabUnderTest(const BlockTabUnderTest&) = delete;
  BlockTabUnderTest& operator=(const BlockTabUnderTest&) = delete;

  ~BlockTabUnderTest() override = default;

  void ExpectUIShown(bool shown) {
#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(shown, !!message_wrapper());
#else
    EXPECT_EQ(shown, FramebustBlockTabHelper::FromWebContents(web_contents())
                         ->HasBlockedUrls());
#endif
  }

  // content::WebContentsDelegate:
  void OnDidBlockFramebust(content::WebContents* web_contents,
                           const GURL& url) {
    blocked_urls_.push_back(url);
  }

 protected:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

 private:
  std::vector<GURL> blocked_urls_;
};

TEST_F(BlockTabUnderTest, SimpleTabUnder_IsBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();
  const GURL blocked_url("https://example.test/");
  expect_message();
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  ExpectUIShown(true);
}

TEST_F(BlockTabUnderTest, NoPopup_NoBlocking) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  web_contents()->WasHidden();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://example.test/")));
  ExpectUIShown(false);
}

TEST_F(BlockTabUnderTest, SameOriginRedirect_NoBlocking) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/path")));
  ExpectUIShown(false);
}

TEST_F(BlockTabUnderTest, SameSiteRedirect_NoBlocking) {
  EXPECT_TRUE(
      NavigateAndCommitWithoutGesture(GURL("https://sub1.blah.co.uk/")));
  SimulatePopup();
  EXPECT_TRUE(
      NavigateAndCommitWithoutGesture(GURL("https://sub2.blah.co.uk/path")));
  ExpectUIShown(false);
}

TEST_F(BlockTabUnderTest, BrowserInitiatedNavigation_NoBlocking) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://example.test"), web_contents());
  simulator->SetHasUserGesture(false);
  simulator->Commit();
  ExpectUIShown(false);
}

TEST_F(BlockTabUnderTest, TabUnderCrossOriginRedirect_IsBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  // Navigate to a same-origin URL that redirects cross origin.
  const GURL same_origin("https://first.test/path");
  const GURL blocked_url("https://example.test/");
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(same_origin,
                                                            main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Start();
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult());
  expect_message();
  simulator->Redirect(blocked_url);
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            simulator->GetLastThrottleCheckResult());
  ExpectUIShown(true);
}

// There is no time limit to this intervention. Explicitly test that navigations
// will be blocked for even days until the tab receives another user gesture.
TEST_F(BlockTabUnderTest, TabUnderWithLongWait_IsBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  // Delay a long time before navigating the opener. Since there is no threshold
  // we always classify as a tab-under.
  raw_clock()->Advance(base::Days(10));

  expect_message();
  const GURL blocked_url("https://example.test/");
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  ExpectUIShown(true);
}

TEST_F(BlockTabUnderTest, TabUnderWithSubsequentGesture_IsNotBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  // First, try navigating without a gesture. It should be blocked.
  const GURL blocked_url("https://example.test/");
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));

  // Now, let the opener get a user gesture. Cast to avoid reaching into private
  // members.
  static_cast<content::WebContentsObserver*>(
      blocked_content::PopupOpenerTabHelper::FromWebContents(web_contents()))
      ->DidGetUserInteraction(blink::WebMouseEvent());

  // A subsequent navigation should be allowed, even if it is classified as a
  // suspicious redirect.
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://example.test2/")));
  ExpectUIShown(false);
}

TEST_F(BlockTabUnderTest, MultipleRedirectAttempts_AreBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  const GURL blocked_url("https://example.test/");
  expect_message();
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  ExpectUIShown(true);
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  ExpectUIShown(true);
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  ExpectUIShown(true);
}

TEST_F(BlockTabUnderTest, LogsUkm) {
  using UkmEntry = ukm::builders::AbusiveExperienceHeuristic_TabUnder;

  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  const GURL first_url("https://first.test/");
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(first_url));
  SimulatePopup();
  raw_clock()->Advance(base::Milliseconds(15));
  const GURL blocked_url("https://example.test/");
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));

  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    test_ukm_recorder.ExpectEntrySourceHasUrl(entry, first_url);
    test_ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kDidTabUnderName,
                                        true);
  }

  const GURL final_url("https://final.test/");
  content::NavigationSimulator::NavigateAndCommitFromDocument(final_url,
                                                              main_rfh());

  auto entries2 = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries2.size());
  for (const ukm::mojom::UkmEntry* const entry : entries2) {
    test_ukm_recorder.ExpectEntrySourceHasUrl(entry, first_url);
  }
}

TEST_F(BlockTabUnderTest, LogsToConsole) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();
  const GURL blocked_url("https://example.test/");

  const auto& messages =
      content::RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();

  EXPECT_EQ(0u, messages.size());
  expect_message();
  EXPECT_FALSE(NavigateAndCommitWithoutGesture(blocked_url));
  ExpectUIShown(true);

  EXPECT_EQ(1u, messages.size());
  std::string expected_message = base::StringPrintf(kBlockTabUnderFormatMessage,
                                                    blocked_url.spec().c_str());
  EXPECT_EQ(expected_message, messages.front());
}

// 1. Navigate to a.com
// 2. Start a navigation without a user gesture to b.com
// 3. Open a popup
// 4. Navigation from (2) redirects and should not be blocked.
TEST_F(BlockTabUnderTest, SlowRedirectAfterPopup_IsNotBlocked) {
  NavigateAndCommit(GURL("https://a.com"));

  std::unique_ptr<content::NavigationSimulator> candidate_navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://b.com"), main_rfh());
  candidate_navigation->SetHasUserGesture(false);
  candidate_navigation->Start();

  SimulatePopup();

  candidate_navigation->Redirect(GURL("https://b.com/redirect"));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            candidate_navigation->GetLastThrottleCheckResult());
  candidate_navigation->Commit();
  EXPECT_EQ(main_rfh(), candidate_navigation->GetFinalRenderFrameHost());
}

// Ensure that even though the *redirect* occurred in the background, if the
// navigation started in the foreground there is no blocking.
TEST_F(BlockTabUnderTest,
       TabUnderCrossOriginRedirectFromForeground_IsNotBlocked) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();

  web_contents()->WasShown();

  // Navigate to a same-origin URL that redirects cross origin.
  const GURL same_origin("https://first.test/path");
  const GURL cross_origin("https://example.test/");
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(same_origin,
                                                            main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Start();
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult());

  web_contents()->WasHidden();

  simulator->Redirect(cross_origin);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult());
  simulator->Commit();
  ExpectUIShown(false);
}

TEST_F(BlockTabUnderTest, OnlyCreateThrottleForPrimaryMainframe) {
  content::MockNavigationHandle handle(GURL("http://example.com"), main_rfh());
  handle.set_is_in_primary_main_frame(true);
  auto throttle = TabUnderNavigationThrottle::MaybeCreate(&handle);
  EXPECT_NE(throttle, nullptr);

  handle.set_is_in_primary_main_frame(false);
  auto throttle2 = TabUnderNavigationThrottle::MaybeCreate(&handle);
  EXPECT_EQ(throttle2, nullptr);
}

class BlockTabUnderDisabledTest : public BlockTabUnderTest {
 public:
  BlockTabUnderDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(kBlockTabUnders);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BlockTabUnderDisabledTest, NoFeature_NoBlocking) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://example.test/")));
  ExpectUIShown(false);
}
