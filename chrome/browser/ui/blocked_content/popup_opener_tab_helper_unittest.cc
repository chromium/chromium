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

class BlockTabUnderDisabledTest : public PopupOpenerTabHelperTest {
 public:
  void ExpectUIShown(bool shown) {
#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(shown, !!message_wrapper());
#else
    EXPECT_EQ(shown, FramebustBlockTabHelper::FromWebContents(web_contents())
                         ->HasBlockedUrls());
#endif
  }
};

TEST_F(BlockTabUnderDisabledTest, NoFeature_NoBlocking) {
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://first.test/")));
  SimulatePopup();
  EXPECT_TRUE(NavigateAndCommitWithoutGesture(GURL("https://example.test/")));
  ExpectUIShown(false);
}
