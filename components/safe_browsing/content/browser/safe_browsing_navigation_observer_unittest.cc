// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace safe_browsing {

class SBNavigationObserverTest : public content::RenderViewHostTestHarness {
 public:
  SBNavigationObserverTest() = default;
  SBNavigationObserverTest(const SBNavigationObserverTest&) = delete;
  SBNavigationObserverTest& operator=(const SBNavigationObserverTest&) = delete;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("http://foo/0"));

    HostContentSettingsMap::RegisterProfilePrefs(pref_service_.registry());
    safe_browsing::RegisterProfilePrefs(pref_service_.registry());
    settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &pref_service_, false /* is_off_the_record */,
        false /* store_last_modified */, false /* restore_session*/,
        false /* should_record_metrics */);
    navigation_observer_manager_ =
        std::make_unique<SafeBrowsingNavigationObserverManager>(
            &pref_service_, &service_worker_context_);

    navigation_observer_ = std::make_unique<SafeBrowsingNavigationObserver>(
        web_contents(), settings_map_.get(),
        navigation_observer_manager_.get());
  }
  void TearDown() override {
    service_worker_context_.RemoveObserver(navigation_observer_manager_.get());
    navigation_observer_.reset();
    settings_map_->ShutdownOnUIThread();
    content::RenderViewHostTestHarness::TearDown();
  }
  void VerifyNavigationEvent(
      const GURL& expected_source_url,
      const GURL& expected_source_main_frame_url,
      const GURL& expected_original_request_url,
      const GURL& expected_destination_url,
      SessionID expected_source_tab,
      SessionID expected_target_tab,
      ReferrerChainEntry::NavigationInitiation expected_nav_initiation,
      bool expected_has_committed,
      bool expected_has_server_redirect,
      NavigationEvent* actual_nav_event) {
    EXPECT_EQ(expected_source_url, actual_nav_event->source_url);
    EXPECT_EQ(expected_source_main_frame_url,
              actual_nav_event->source_main_frame_url);
    EXPECT_EQ(expected_original_request_url,
              actual_nav_event->original_request_url);
    EXPECT_EQ(expected_destination_url, actual_nav_event->GetDestinationUrl());
    EXPECT_EQ(expected_source_tab, actual_nav_event->source_tab_id);
    EXPECT_EQ(expected_target_tab, actual_nav_event->target_tab_id);
    EXPECT_EQ(expected_nav_initiation, actual_nav_event->navigation_initiation);
    EXPECT_EQ(expected_has_committed, actual_nav_event->has_committed);
    EXPECT_EQ(expected_has_server_redirect,
              !actual_nav_event->server_redirect_urls.empty());
  }

  NavigationEventList* navigation_event_list() {
    return navigation_observer_manager_->navigation_event_list();
  }

  SafeBrowsingNavigationObserverManager::UserGestureMap* user_gesture_map() {
    return &navigation_observer_manager_->user_gesture_map_;
  }

  SafeBrowsingNavigationObserverManager::HostToIpMap* host_to_ip_map() {
    return &navigation_observer_manager_->host_to_ip_map_;
  }

  base::flat_map<GURL, std::unique_ptr<NavigationEvent>>*
  notification_navigation_events() {
    return &navigation_observer_manager_->notification_navigation_events_;
  }

  void RecordNotificationNavigationEvent(const GURL& script_url,
                                         const GURL& url) {
    navigation_observer_manager_->RecordNotificationNavigationEvent(script_url,
                                                                    url);
  }

  void RecordHostToIpMapping(const std::string& host, const std::string& ip) {
    navigation_observer_manager_->RecordHostToIpMapping(host, ip);
  }

  std::unique_ptr<NavigationEvent> CreateNavigationEventUniquePtr(
      const GURL& destination_url,
      const base::Time& timestamp) {
    std::unique_ptr<NavigationEvent> nav_event_ptr =
        std::make_unique<NavigationEvent>();
    nav_event_ptr->original_request_url = destination_url;
    nav_event_ptr->source_url = GURL("http://dummy.com");
    nav_event_ptr->last_updated = timestamp;
    return nav_event_ptr;
  }

  void CreateNonUserGestureReferrerChain() {
    user_gesture_map()->clear();
    base::Time now = base::Time::Now();
    base::Time half_second_ago = base::Time::FromSecondsSinceUnixEpoch(
        now.InSecondsFSinceUnixEpoch() - 0.5);
    base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
        now.InSecondsFSinceUnixEpoch() - 1.0);
    base::Time two_seconds_ago = base::Time::FromSecondsSinceUnixEpoch(
        now.InSecondsFSinceUnixEpoch() - 2.0);

    // Add 13 navigations and one starting page. The first is BROWSER_INITIATED
    // to A. Then from A to B, then 10 redirects to C, then back to A.
    std::unique_ptr<NavigationEvent> first_navigation =
        std::make_unique<NavigationEvent>();
    first_navigation->original_request_url = GURL("http://A.com");
    first_navigation->last_updated = two_seconds_ago;
    first_navigation->navigation_initiation =
        ReferrerChainEntry::BROWSER_INITIATED;
    navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

    std::unique_ptr<NavigationEvent> second_navigation =
        std::make_unique<NavigationEvent>();
    second_navigation->source_url = GURL("http://A.com");
    second_navigation->original_request_url = GURL("http://B.com");
    second_navigation->last_updated = one_second_ago;
    second_navigation->navigation_initiation =
        ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
    navigation_event_list()->RecordNavigationEvent(
        std::move(second_navigation));

    GURL prev_url = GURL("http://B.com");
    GURL current_url = GURL("http://C.com?utm=0");
    for (int i = 1; i < 11; i++) {
      std::unique_ptr<NavigationEvent> navigation =
          std::make_unique<NavigationEvent>();
      navigation->source_url = prev_url;
      navigation->original_request_url = current_url;
      navigation->last_updated = one_second_ago;
      navigation->navigation_initiation =
          ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE;
      navigation_event_list()->RecordNavigationEvent(std::move(navigation));
      prev_url = current_url;
      current_url = GURL("http://C.com?utm=" + base::NumberToString(i));
    }

    std::unique_ptr<NavigationEvent> last_navigation =
        std::make_unique<NavigationEvent>();
    last_navigation->source_url = prev_url;
    last_navigation->original_request_url = GURL("http://A.com");
    last_navigation->last_updated = half_second_ago;
    last_navigation->navigation_initiation =
        ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
    navigation_event_list()->RecordNavigationEvent(std::move(last_navigation));
    ASSERT_EQ(13U, navigation_event_list()->NavigationEventsSize());
  }

  void CleanUpNavigationEvents() {
    navigation_observer_manager_->CleanUpNavigationEvents();
  }

  void CleanUpIpAddresses() {
    navigation_observer_manager_->CleanUpIpAddresses();
  }

  void CleanUpUserGestures() {
    navigation_observer_manager_->CleanUpUserGestures();
  }

  void CleanUpNotificationNavigationEvents() {
    navigation_observer_manager_->CleanUpNotificationNavigationEvents();
  }

  void SetEnhancedProtection(bool esb_enabled) {
    SetEnhancedProtectionPrefForTests(&pref_service_, esb_enabled);
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  content::FakeServiceWorkerContext service_worker_context_;
  std::unique_ptr<SafeBrowsingNavigationObserverManager>
      navigation_observer_manager_;
  std::unique_ptr<SafeBrowsingNavigationObserver> navigation_observer_;
};

TEST_F(SBNavigationObserverTest, TestNavigationEventList) {
  NavigationEventList events(3);

  EXPECT_FALSE(events.FindNavigationEvent(
      base::Time::Now(), GURL("http://invalid.com"), GURL(),
      SessionID::InvalidValue(), content::GlobalRenderFrameHostId(),
      navigation_event_list()->navigation_events().size() - 1));
  EXPECT_EQ(0U, events.CleanUpNavigationEvents());
  EXPECT_EQ(0U, events.NavigationEventsSize());

  // Add 2 events to the list.
  base::Time now = base::Time::Now();
  base::Time one_hour_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 60.0 * 60.0);
  events.RecordNavigationEvent(
      CreateNavigationEventUniquePtr(GURL("http://foo1.com"), one_hour_ago));
  events.RecordNavigationEvent(
      CreateNavigationEventUniquePtr(GURL("http://foo1.com"), now));
  EXPECT_EQ(2U, events.NavigationEventsSize());
  // FindNavigationEvent should return the latest matching event.
  auto index = events.FindNavigationEvent(
      base::Time::Now(), GURL("http://foo1.com"), GURL(),
      SessionID::InvalidValue(), content::GlobalRenderFrameHostId(),
      events.navigation_events().size() - 1);
  EXPECT_TRUE(index);

  EXPECT_EQ(now, events.GetNavigationEvent(*index)->last_updated);
  // One event should get removed.
  EXPECT_EQ(1U, events.CleanUpNavigationEvents());
  EXPECT_EQ(1U, events.NavigationEventsSize());

  // Add 3 more events, previously recorded events should be overridden.
  events.RecordNavigationEvent(
      CreateNavigationEventUniquePtr(GURL("http://foo3.com"), one_hour_ago));
  events.RecordNavigationEvent(
      CreateNavigationEventUniquePtr(GURL("http://foo4.com"), one_hour_ago));
  events.RecordNavigationEvent(
      CreateNavigationEventUniquePtr(GURL("http://foo5.com"), now));
  ASSERT_EQ(3U, events.NavigationEventsSize());
  EXPECT_EQ(GURL("http://foo3.com"),
            events.GetNavigationEvent(0)->original_request_url);
  EXPECT_EQ(GURL("http://foo4.com"),
            events.GetNavigationEvent(1)->original_request_url);
  EXPECT_EQ(GURL("http://foo5.com"),
            events.GetNavigationEvent(2)->original_request_url);
  EXPECT_EQ(2U, events.CleanUpNavigationEvents());
  EXPECT_EQ(1U, events.NavigationEventsSize());
}

TEST_F(SBNavigationObserverTest, TestInfiniteLoop) {
  user_gesture_map()->clear();
  base::Time now = base::Time::Now();
  base::Time half_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 0.5);
  base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 1.0);
  base::Time two_seconds_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 2.0);

  // Add 5 navigations and one starting page. The first is BROWSER_INITIATED
  // to A. Then from A to B, then 2 redirects back and forth between B and C,
  // then back to A.
  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->original_request_url = GURL("http://A.com");
  first_navigation->last_updated = two_seconds_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->source_url = GURL("http://A.com");
  second_navigation->original_request_url = GURL("http://B.com");
  second_navigation->last_updated = one_second_ago;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));
  GURL prev_url = GURL("http://B.com");
  GURL current_url = GURL("http://C.com?utm=0");
  for (int i = 1; i < 4; i++) {
    std::unique_ptr<NavigationEvent> navigation =
        std::make_unique<NavigationEvent>();
    navigation->source_url = prev_url;
    navigation->original_request_url = current_url;
    navigation->last_updated = one_second_ago;
    navigation->navigation_initiation =
        ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE;
    navigation_event_list()->RecordNavigationEvent(std::move(navigation));
    GURL current_prev_url = prev_url;
    prev_url = current_url;
    current_url = current_prev_url;
  }

  std::unique_ptr<NavigationEvent> last_navigation =
      std::make_unique<NavigationEvent>();
  last_navigation->source_url = prev_url;
  last_navigation->original_request_url = GURL("http://A.com");
  last_navigation->last_updated = half_second_ago;
  last_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  navigation_event_list()->RecordNavigationEvent(std::move(last_navigation));
  ASSERT_EQ(6U, navigation_event_list()->NavigationEventsSize());
  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://A.com/"), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 10, &referrer_chain);
  ASSERT_EQ(6, referrer_chain.size());
}

TEST_F(SBNavigationObserverTest, TestFindEventInCorrectOutermostFrame) {
  NavigationEventList events(4);

  // Add 2 events to the list.
  base::Time now = base::Time::Now();
  const int child_id = 1;
  const int frame_id_a = 2;
  const int frame_id_b = 3;

  auto outermost_main_frame_id_a =
      content::GlobalRenderFrameHostId(child_id, frame_id_a);

  auto event_page_a =
      CreateNavigationEventUniquePtr(GURL("http://foo1.com"), now);
  event_page_a->outermost_main_frame_id = outermost_main_frame_id_a;
  events.RecordNavigationEvent(std::move(event_page_a));

  auto event_subframe_a = CreateNavigationEventUniquePtr(
      GURL("http://foo1.com/subframe.html"), now);
  event_subframe_a->outermost_main_frame_id = outermost_main_frame_id_a;
  events.RecordNavigationEvent(std::move(event_subframe_a));

  auto outermost_main_frame_id_b =
      content::GlobalRenderFrameHostId(child_id, frame_id_b);

  auto event_page_b =
      CreateNavigationEventUniquePtr(GURL("http://foo1.com/bar.html"), now);
  event_page_b->outermost_main_frame_id = outermost_main_frame_id_b;
  events.RecordNavigationEvent(std::move(event_page_b));

  auto event_subframe_b = CreateNavigationEventUniquePtr(
      GURL("http://foo1.com/subframe.html"), now);
  event_subframe_b->outermost_main_frame_id = outermost_main_frame_id_b;
  events.RecordNavigationEvent(std::move(event_subframe_b));

  // Should match outermost main frame id, where possible.
  EXPECT_EQ(
      1U, *events.FindNavigationEvent(
              base::Time::Now(), GURL("http://foo1.com/subframe.html"), GURL(),
              SessionID::InvalidValue(), outermost_main_frame_id_a,
              events.NavigationEventsSize() - 1));
  EXPECT_EQ(
      3U, *events.FindNavigationEvent(
              base::Time::Now(), GURL("http://foo1.com/subframe.html"), GURL(),
              SessionID::InvalidValue(), outermost_main_frame_id_b,
              events.NavigationEventsSize() - 1));

  // Should match the most recent if main_frame_id is not given.
  EXPECT_EQ(
      3U, *events.FindNavigationEvent(
              base::Time::Now(), GURL("http://foo1.com/subframe.html"), GURL(),
              SessionID::InvalidValue(), content::GlobalRenderFrameHostId(),
              events.NavigationEventsSize() - 1));
}

TEST_F(SBNavigationObserverTest, TestBasicReferrerChain) {
  base::Time now = base::Time::Now();
  base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 1.0);
  base::Time two_seconds_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 2.0);

  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->original_request_url = GURL("http://A.com");
  first_navigation->last_updated = two_seconds_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->source_url = GURL("http://A.com");
  second_navigation->original_request_url = GURL("http://B.com");
  second_navigation->last_updated = one_second_ago;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  std::unique_ptr<NavigationEvent> third_navigation =
      std::make_unique<NavigationEvent>();
  third_navigation->source_url = GURL("http://B.com");
  third_navigation->original_request_url = GURL("http://C.com");
  third_navigation->last_updated = now;
  third_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  navigation_event_list()->RecordNavigationEvent(std::move(third_navigation));

  ASSERT_EQ(3U, navigation_event_list()->NavigationEventsSize());

  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://C.com/"), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 10, &referrer_chain);
  ASSERT_EQ(3, referrer_chain.size());

  ReferrerChain referrer_chain_without_render_frame_host;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://C.com/"), SessionID::InvalidValue(), 10,
      &referrer_chain_without_render_frame_host);
  ASSERT_EQ(3, referrer_chain_without_render_frame_host.size());
}

TEST_F(SBNavigationObserverTest, BasicNavigationAndCommit) {
  // Navigation in current tab.
  NavigateAndCommit(GURL("http://foo/1"), ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents());
  auto* nav_list = navigation_event_list();
  ASSERT_EQ(1U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),                // source_url
                        GURL(),                // source_main_frame_url
                        GURL("http://foo/1"),  // original_request_url
                        GURL("http://foo/1"),  // destination_url
                        tab_id,                // source_tab_id
                        tab_id,                // target_tab_id
                        ReferrerChainEntry::BROWSER_INITIATED,
                        true,   // has_committed
                        false,  // has_server_redirect
                        nav_list->GetNavigationEvent(0U));
}

TEST_F(SBNavigationObserverTest, ServerRedirect) {
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://foo/3"), web_contents()->GetPrimaryMainFrame());
  auto* nav_list = navigation_event_list();
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents());

  navigation->Start();
  ASSERT_EQ(1U, nav_list->PendingNavigationEventsSize());
  NavigationEvent* pending_event =
      nav_list->FindPendingNavigationEvent(GURL("http://foo/3"));
  VerifyNavigationEvent(
      GURL("http://foo/0"),       // source_url
      GURL("http://foo/0"),       // source_main_frame_url
      GURL("http://foo/3"),       // original_request_url
      GURL("http://foo/3"),       // destination_url
      tab_id,                     // source_tab_id
      SessionID::InvalidValue(),  // target_tab_id
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      false,  // has_committed
      false,  // has_server_redirect
      pending_event);

  navigation->Redirect(GURL("http://redirect/1"));
  ASSERT_EQ(1U, nav_list->PendingNavigationEventsSize());
  pending_event = nav_list->FindPendingNavigationEvent(GURL("http://foo/3"));
  // The pending event cannot be found because the destination URL has been
  // changed to the redirect URL.
  ASSERT_EQ(nullptr, pending_event);
  pending_event =
      nav_list->FindPendingNavigationEvent(GURL("http://redirect/1"));
  VerifyNavigationEvent(
      GURL("http://foo/0"),       // source_url
      GURL("http://foo/0"),       // source_main_frame_url
      GURL("http://foo/3"),       // original_request_url
      GURL("http://redirect/1"),  // destination_url
      tab_id,                     // source_tab_id
      SessionID::InvalidValue(),  // target_tab_id
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      false,  // has_committed
      true,   // has_server_redirect
      pending_event);

  navigation->Commit();
  // The pending navigation event should be removed because the navigation is
  // completed.
  ASSERT_EQ(0U, nav_list->PendingNavigationEventsSize());
  ASSERT_EQ(1U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(
      GURL("http://foo/0"),       // source_url
      GURL("http://foo/0"),       // source_main_frame_url
      GURL("http://foo/3"),       // original_request_url
      GURL("http://redirect/1"),  // destination_url
      tab_id,                     // source_tab_id
      tab_id,                     // target_tab_id
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      true,  // has_committed
      true,  // has_server_redirect
      nav_list->GetNavigationEvent(0U));
}

TEST_F(SBNavigationObserverTest,
       TestNotificationNavigationEventsNotAddedForNonESBUsers) {
  GURL url_0("http://foo/0");
  GURL url_1("http://foo/1");
  GURL url_2("http://foo/2");
  GURL script_url("https://example.com/script.js");
  SetEnhancedProtection(/*esb_enabled=*/false);
  RecordNotificationNavigationEvent(script_url, url_0);
  RecordNotificationNavigationEvent(script_url, url_1);
  RecordNotificationNavigationEvent(script_url, url_2);
  EXPECT_EQ(0U, notification_navigation_events()->size());
}

TEST_F(SBNavigationObserverTest,
       TestNotificationNavigationEventsAddedForExtensions) {
  GURL url_0("http://foo/0");
  GURL url_1("http://foo/1");
  GURL url_2("http://foo/2");
  GURL script_url("https://example.com/script.js");
  SetEnhancedProtection(/*esb_enabled=*/true);
  RecordNotificationNavigationEvent(script_url, url_0);
  RecordNotificationNavigationEvent(GURL("chrome-extension://some-extension"),
                                    url_1);
  RecordNotificationNavigationEvent(GURL("http://bogus-web-origin.com"), url_2);
  EXPECT_EQ(2U, notification_navigation_events()->size());
}

TEST_F(SBNavigationObserverTest,
       TestNotificationNavigationEventsNotAddedForNonExtensionsAndNonHttps) {
  GURL url_0("http://foo/0");
  GURL url_1("http://foo/1");
  GURL url_2("http://foo/2");
  GURL script_url("https://example.com/script.js");
  SetEnhancedProtection(/*esb_enabled=*/true);
  RecordNotificationNavigationEvent(script_url, url_0);
  RecordNotificationNavigationEvent(GURL("ftp://some-host"), url_1);
  RecordNotificationNavigationEvent(GURL("http://bogus-web-origin.com"), url_2);
  EXPECT_EQ(1U, notification_navigation_events()->size());
}

TEST_F(SBNavigationObserverTest,
       TestCleanUpStableNotificationNavigationEvents) {
  base::Time now = base::Time::Now();  // Fresh
  base::Time one_hour_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 60.0 * 60.0);  // Stale
  base::Time one_minute_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 60.0);  // Fresh
  GURL url_0("http://foo/0");
  GURL url_1("http://foo/1");
  GURL url_2("http://foo/2");
  GURL script_url("https://example.com/script.js");
  SetEnhancedProtection(/*esb_enabled=*/true);
  RecordNotificationNavigationEvent(script_url, url_0);
  RecordNotificationNavigationEvent(script_url, url_1);
  RecordNotificationNavigationEvent(script_url, url_2);
  (*notification_navigation_events())[url_1]->last_updated = one_hour_ago;
  (*notification_navigation_events())[url_2]->last_updated = one_minute_ago;

  CleanUpNotificationNavigationEvents();

  EXPECT_EQ(2U, notification_navigation_events()->size());
  EXPECT_TRUE(notification_navigation_events()->contains(url_0));
  EXPECT_TRUE(notification_navigation_events()->contains(url_2));
}

TEST_F(SBNavigationObserverTest,
       TestAddDesktopNotificationOriginToReferrerChain) {
  GURL url_0("http://foo/0");
  std::unique_ptr<NavigationEvent> nav_event =
      CreateNavigationEventUniquePtr(url_0, base::Time::Now());
  nav_event->source_url = GURL();
  nav_event->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE;
  navigation_event_list()->RecordNavigationEvent(std::move(nav_event));
  GURL script_url("https://example.com/script.js");
  SetEnhancedProtection(/*esb_enabled=*/true);
  RecordNotificationNavigationEvent(script_url, url_0);
  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      url_0, SessionID::InvalidValue(), content::GlobalRenderFrameHostId(), 10,
      &referrer_chain);
  EXPECT_EQ(ReferrerChainEntry::NOTIFICATION_INITIATED,
            referrer_chain[0].navigation_initiation());
  EXPECT_EQ(script_url.spec(), referrer_chain[0].referrer_url());
}

TEST_F(SBNavigationObserverTest,
       TestAddAndroidNotificationOriginToReferrerChain) {
  GURL url_0("http://foo/0");
  std::unique_ptr<NavigationEvent> nav_event =
      CreateNavigationEventUniquePtr(url_0, base::Time::Now());
  nav_event->source_url = GURL();
  // How the notification navigation is initiated is the sole difference with
  // desktop.
  nav_event->navigation_initiation = ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(nav_event));
  GURL script_url("https://example.com/script.js");
  SetEnhancedProtection(/*esb_enabled=*/true);
  RecordNotificationNavigationEvent(script_url, url_0);
  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      url_0, SessionID::InvalidValue(), content::GlobalRenderFrameHostId(), 10,
      &referrer_chain);
  EXPECT_EQ(ReferrerChainEntry::NOTIFICATION_INITIATED,
            referrer_chain[0].navigation_initiation());
  EXPECT_EQ(script_url.spec(), referrer_chain[0].referrer_url());
}

TEST_F(SBNavigationObserverTest, TestCleanUpStaleNavigationEvents) {
  // Sets up navigation_event_list() such that it includes fresh, stale and
  // invalid
  // navigation events.
  base::Time now = base::Time::Now();  // Fresh
  base::Time one_hour_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 60.0 * 60.0);  // Stale
  base::Time one_minute_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 60.0);  // Fresh
  base::Time in_an_hour = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() + 60.0 * 60.0);  // Invalid
  GURL url_0("http://foo/0");
  GURL url_1("http://foo/1");
  content::MockNavigationHandle handle_0(url_0,
                                         web_contents()->GetPrimaryMainFrame());
  content::MockNavigationHandle handle_1(url_1,
                                         web_contents()->GetPrimaryMainFrame());
  navigation_event_list()->RecordNavigationEvent(
      CreateNavigationEventUniquePtr(url_0, in_an_hour));
  navigation_event_list()->RecordNavigationEvent(
      CreateNavigationEventUniquePtr(url_0, one_hour_ago));
  navigation_event_list()->RecordNavigationEvent(
      CreateNavigationEventUniquePtr(url_1, one_hour_ago));
  navigation_event_list()->RecordNavigationEvent(
      CreateNavigationEventUniquePtr(url_1, one_hour_ago));
  navigation_event_list()->RecordNavigationEvent(
      CreateNavigationEventUniquePtr(url_0, one_minute_ago));
  navigation_event_list()->RecordNavigationEvent(
      CreateNavigationEventUniquePtr(url_0, now));
  navigation_event_list()->RecordPendingNavigationEvent(
      &handle_1, CreateNavigationEventUniquePtr(url_1, one_hour_ago));
  navigation_event_list()->RecordPendingNavigationEvent(
      &handle_0, CreateNavigationEventUniquePtr(url_0, now));
  ASSERT_EQ(6U, navigation_event_list()->NavigationEventsSize());
  ASSERT_EQ(2U, navigation_event_list()->PendingNavigationEventsSize());

  // Cleans up navigation events.
  CleanUpNavigationEvents();

  // Verifies all stale and invalid navigation events are removed.
  ASSERT_EQ(2U, navigation_event_list()->NavigationEventsSize());
  ASSERT_EQ(1U, navigation_event_list()->PendingNavigationEventsSize());
  EXPECT_FALSE(navigation_event_list()->FindNavigationEvent(
      base::Time::Now(), url_1, GURL(), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(),
      navigation_event_list()->NavigationEventsSize() - 1));
  EXPECT_EQ(nullptr,
            navigation_event_list()->FindPendingNavigationEvent(url_1));
}

TEST_F(SBNavigationObserverTest, TestCleanUpStaleUserGestures) {
  // Sets up user_gesture_map() such that it includes fresh, stale and invalid
  // user gestures.
  base::Time now = base::Time::Now();  // Fresh
  base::Time three_minutes_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 60.0 * 3);  // Stale
  base::Time in_an_hour = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() + 60.0 * 60.0);  // Invalid
  content::WebContents* content0 = web_contents();
  auto content1 = CreateTestWebContents();
  content::WebContentsTester::For(content1.get())
      ->NavigateAndCommit(GURL("http://foo/1"));
  auto content2 = CreateTestWebContents();
  content::WebContentsTester::For(content2.get())
      ->NavigateAndCommit(GURL("http://foo/2"));
  user_gesture_map()->insert(std::make_pair(content0, now));
  user_gesture_map()->insert(std::make_pair(content1.get(), three_minutes_ago));
  user_gesture_map()->insert(std::make_pair(content2.get(), in_an_hour));
  ASSERT_EQ(3U, user_gesture_map()->size());

  // Cleans up user_gesture_map()
  CleanUpUserGestures();

  // Verifies all stale and invalid user gestures are removed.
  ASSERT_EQ(1U, user_gesture_map()->size());
  EXPECT_NE(user_gesture_map()->end(), user_gesture_map()->find(content0));
  EXPECT_EQ(now, (*user_gesture_map())[content0]);
}

TEST_F(SBNavigationObserverTest, TestCleanUpStaleIPAddresses) {
  // Sets up host_to_ip_map() such that it includes fresh, stale and invalid
  // user gestures.
  base::Time now = base::Time::Now();  // Fresh
  base::Time one_hour_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 60.0 * 60.0);  // Stale
  base::Time in_an_hour = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() + 60.0 * 60.0);  // Invalid
  std::string host_0 = GURL("http://foo/0").host();
  std::string host_1 = GURL("http://bar/1").host();
  host_to_ip_map()->insert(
      std::make_pair(host_0, std::vector<ResolvedIPAddress>()));
  (*host_to_ip_map())[host_0].push_back(ResolvedIPAddress(now, "1.1.1.1"));
  (*host_to_ip_map())[host_0].push_back(
      ResolvedIPAddress(one_hour_ago, "2.2.2.2"));
  host_to_ip_map()->insert(
      std::make_pair(host_1, std::vector<ResolvedIPAddress>()));
  (*host_to_ip_map())[host_1].push_back(
      ResolvedIPAddress(in_an_hour, "3.3.3.3"));
  ASSERT_EQ(2U, host_to_ip_map()->size());

  // Cleans up host_to_ip_map()
  CleanUpIpAddresses();

  // Verifies all stale and invalid IP addresses are removed.
  ASSERT_EQ(1U, host_to_ip_map()->size());
  EXPECT_EQ(host_to_ip_map()->end(), host_to_ip_map()->find(host_1));
  ASSERT_EQ(1U, (*host_to_ip_map())[host_0].size());
  EXPECT_EQ(now, (*host_to_ip_map())[host_0].front().timestamp);
}

TEST_F(SBNavigationObserverTest, TestRecordHostToIpMapping) {
  // Setup host_to_ip_map().
  base::Time now = base::Time::Now();  // Fresh
  base::Time one_hour_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 60.0 * 60.0);  // Stale
  std::string host_0 = GURL("http://foo/0").host();
  host_to_ip_map()->insert(
      std::make_pair(host_0, std::vector<ResolvedIPAddress>()));
  (*host_to_ip_map())[host_0].push_back(ResolvedIPAddress(now, "1.1.1.1"));
  (*host_to_ip_map())[host_0].push_back(
      ResolvedIPAddress(one_hour_ago, "2.2.2.2"));

  // Record a host-IP pair, where host is already in the map, and IP has
  // never been seen before.
  RecordHostToIpMapping(host_0, "3.3.3.3");
  ASSERT_EQ(1U, host_to_ip_map()->size());
  EXPECT_EQ(3U, (*host_to_ip_map())[host_0].size());
  EXPECT_EQ("3.3.3.3", (*host_to_ip_map())[host_0][2].ip);

  // Record a host-IP pair which is already in the map. It should simply update
  // its timestamp.
  ASSERT_EQ(now, (*host_to_ip_map())[host_0][0].timestamp);
  RecordHostToIpMapping(host_0, "1.1.1.1");
  ASSERT_EQ(1U, host_to_ip_map()->size());
  EXPECT_EQ(3U, (*host_to_ip_map())[host_0].size());
  EXPECT_LT(now, (*host_to_ip_map())[host_0][2].timestamp);

  // Record a host-ip pair, neither of which has been seen before.
  std::string host_1 = GURL("http://bar/1").host();
  RecordHostToIpMapping(host_1, "9.9.9.9");
  ASSERT_EQ(2U, host_to_ip_map()->size());
  EXPECT_EQ(3U, (*host_to_ip_map())[host_0].size());
  EXPECT_EQ(1U, (*host_to_ip_map())[host_1].size());
  EXPECT_EQ("9.9.9.9", (*host_to_ip_map())[host_1][0].ip);
}

TEST_F(SBNavigationObserverTest, TestContentSettingChange) {
  user_gesture_map()->clear();
  ASSERT_EQ(0U, user_gesture_map()->size());

  content::WebContents* web_content = web_contents();

  // Simulate content setting change via page info UI.
  navigation_observer_->OnContentSettingChanged(
      ContentSettingsPattern::FromURL(web_content->GetLastCommittedURL()),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsTypeSet(ContentSettingsType::NOTIFICATIONS));

  // A user gesture should be recorded.
  ASSERT_EQ(1U, user_gesture_map()->size());
  EXPECT_NE(user_gesture_map()->end(), user_gesture_map()->find(web_content));

  user_gesture_map()->clear();
  ASSERT_EQ(0U, user_gesture_map()->size());

  // Simulate content setting change that cannot be changed via page info UI.
  navigation_observer_->OnContentSettingChanged(
      ContentSettingsPattern::FromURL(web_content->GetLastCommittedURL()),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsTypeSet(ContentSettingsType::SITE_ENGAGEMENT));
  // No user gesture should be recorded.
  EXPECT_EQ(0U, user_gesture_map()->size());
}

TEST_F(SBNavigationObserverTest, TimestampIsDecreasing) {
  base::Time now = base::Time::Now();
  base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 1.0);
  base::Time two_seconds_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 2.0);

  // Add three navigations. The first is BROWSER_INITIATED to A. Then from A to
  // B, and then from B back to A.
  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->original_request_url = GURL("http://A.com");
  first_navigation->last_updated = two_seconds_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->source_url = GURL("http://A.com");
  second_navigation->original_request_url = GURL("http://B.com");
  second_navigation->last_updated = one_second_ago;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  std::unique_ptr<NavigationEvent> third_navigation =
      std::make_unique<NavigationEvent>();
  third_navigation->source_url = GURL("http://B.com");
  third_navigation->original_request_url = GURL("http://A.com");
  third_navigation->last_updated = now;
  third_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  navigation_event_list()->RecordNavigationEvent(std::move(third_navigation));

  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://A.com"), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 10, &referrer_chain);

  ASSERT_EQ(3, referrer_chain.size());

  EXPECT_GE(referrer_chain[0].navigation_time_msec(),
            referrer_chain[1].navigation_time_msec());
  EXPECT_GE(referrer_chain[1].navigation_time_msec(),
            referrer_chain[2].navigation_time_msec());
}

TEST_F(SBNavigationObserverTest,
       RemoveMiddleNonUserGestureEntriesForRecentNavigation) {
  CreateNonUserGestureReferrerChain();

  ReferrerChain referrer_chain;
  navigation_observer_manager_->AppendRecentNavigations(10, &referrer_chain);

  int utm_counter = 10;
  GURL expected_current_url = GURL("http://A.com");
  GURL expected_prev_url;

  // AppendRecentNavigations skips some entries based on the time.
  // Before:
  // Gesture(0...1) -> NonGesture(2..11) -> Gesture(12)
  // After:
  // Gesture(0) -> Non Gesture(1...4) -> Empty(5...7) -> NG(5...8) -> G(9..10)
  // (on recording into referrer chain, the navigations are recorded in a
  // reverse order and the first two are skipped due to timing)
  for (int i = 0; i < 10; i++) {
    expected_prev_url = expected_current_url;
    utm_counter--;
    expected_current_url =
        GURL("http://C.com?utm=" + base::NumberToString(utm_counter));
    // The middle entries should have an empty ReferrerChainEntry.
    if (i == 5 || i == 6 || i == 7) {
      EXPECT_EQ("", referrer_chain[i].url());
    } else {
      EXPECT_EQ(expected_prev_url, referrer_chain[i].url());
      EXPECT_EQ(expected_current_url, referrer_chain[i].referrer_url());
    }
  }
  expected_prev_url = expected_current_url;
  EXPECT_EQ(expected_prev_url, referrer_chain[10].url());
  EXPECT_EQ(GURL("http://B.com"), referrer_chain[10].referrer_url());
  EXPECT_EQ(GURL("http://B.com"), referrer_chain[11].url());
  EXPECT_EQ(GURL("http://A.com"), referrer_chain[11].referrer_url());
}

TEST_F(SBNavigationObserverTest,
       RemoveNonUserGestureEntriesWithExcessiveUserGestureEvents) {
  GURL url = GURL("http://A.com");
  base::Time half_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      base::Time::Now().InSecondsFSinceUnixEpoch() - 0.5);
  // Append 10 navigation events with user gesture.
  for (int i = 0; i < 10; i++) {
    std::unique_ptr<NavigationEvent> navigation_event =
        std::make_unique<NavigationEvent>();
    navigation_event->source_url = url;
    navigation_event->last_updated = half_second_ago;
    navigation_event->navigation_initiation =
        ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
    navigation_event_list()->RecordNavigationEvent(std::move(navigation_event));
  }

  ReferrerChain referrer_chain;
  // Get the first 5 events.
  navigation_observer_manager_->AppendRecentNavigations(5, &referrer_chain);
  // The length of the referrer chain should not exceed the limit.
  EXPECT_EQ(5, referrer_chain.size());
}

TEST_F(SBNavigationObserverTest, RemoveMiddleReferrerChains) {
  CreateNonUserGestureReferrerChain();

  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://A.com/"), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 10, &referrer_chain);

  int utm_counter = 10;
  GURL expected_current_url = GURL("http://A.com");
  GURL expected_prev_url;
  for (int i = 0; i < 10; i++) {
    expected_prev_url = expected_current_url;
    utm_counter--;
    expected_current_url =
        GURL("http://C.com?utm=" + base::NumberToString(utm_counter));
    // The middle entries should have an empty ReferrerChainEntry.
    // Before:
    // Gesture(0...1) -> NonGesture(2..11) -> Gesture(12)
    // After:
    // Gesture(0) -> Non Gesture(1...2) -> Empty(5...7) -> NG(8...9) ->
    // G(10...11)
    if (i == 5 || i == 6 || i == 7) {
      EXPECT_EQ("", referrer_chain[i].url());
    } else {
      EXPECT_EQ(expected_prev_url, referrer_chain[i].url());
      EXPECT_EQ(expected_current_url, referrer_chain[i].referrer_url());
    }
  }
  expected_prev_url = expected_current_url;
  EXPECT_EQ(expected_prev_url, referrer_chain[10].url());
  EXPECT_EQ(GURL("http://B.com"), referrer_chain[10].referrer_url());
  EXPECT_EQ(GURL("http://B.com"), referrer_chain[11].url());
  EXPECT_EQ(GURL("http://A.com"), referrer_chain[11].referrer_url());
}

TEST_F(SBNavigationObserverTest, ChainWorksThroughNewTab) {
  base::Time now = base::Time::Now();
  base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 1.0);

  SessionID source_tab = SessionID::NewUnique();
  SessionID target_tab = SessionID::NewUnique();

  // Add two navigations. The first is renderer initiated and retargeting from A
  // to B. The second navigates the new tab to B.
  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->source_url = GURL("http://a.com/");
  first_navigation->original_request_url = GURL("http://b.com/");
  first_navigation->last_updated = one_second_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  first_navigation->source_tab_id = source_tab;
  first_navigation->target_tab_id = target_tab;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->original_request_url = GURL("http://b.com/");
  second_navigation->last_updated = now;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  second_navigation->source_tab_id = target_tab;
  second_navigation->target_tab_id = target_tab;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://b.com/"), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 10, &referrer_chain);

  ASSERT_EQ(1, referrer_chain.size());

  EXPECT_EQ("http://b.com/", referrer_chain[0].url());
  EXPECT_EQ("http://a.com/", referrer_chain[0].referrer_url());
  EXPECT_TRUE(referrer_chain[0].is_retargeting());
}

TEST_F(SBNavigationObserverTest, ChainContinuesThroughBrowserInitiated) {
  base::Time now = base::Time::Now();
  base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 1.0);

  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->original_request_url = GURL("http://a.com/");
  first_navigation->last_updated = one_second_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->source_url = GURL("http://a.com/");
  second_navigation->original_request_url = GURL("http://b.com/");
  second_navigation->last_updated = now;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://b.com/"), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 10, &referrer_chain);

  EXPECT_EQ(2, referrer_chain.size());
}

TEST_F(SBNavigationObserverTest,
       CanceledRetargetingNavigationHasCorrectEventUrl) {
  base::Time now = base::Time::Now();
  base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 1.0);

  SessionID source_tab = SessionID::NewUnique();
  SessionID target_tab = SessionID::NewUnique();

  // Add two navigations. A initially opens a new tab with url B, but cancels
  // that before it completes. It then navigates the new tab to C. We expect
  // that asking for the referrer chain for C has C as the event url.
  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->source_url = GURL("http://example.com/a");
  first_navigation->original_request_url = GURL("http://example.com/b");
  first_navigation->last_updated = one_second_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  first_navigation->source_tab_id = source_tab;
  first_navigation->target_tab_id = target_tab;
  first_navigation->has_committed = false;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->original_request_url = GURL("http://example.com/c");
  second_navigation->last_updated = now;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  second_navigation->source_tab_id = target_tab;
  second_navigation->target_tab_id = target_tab;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://example.com/c"), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 10, &referrer_chain);

  ASSERT_EQ(1, referrer_chain.size());

  EXPECT_EQ("http://example.com/c", referrer_chain[0].url());
  EXPECT_EQ("http://example.com/a", referrer_chain[0].referrer_url());
  EXPECT_TRUE(referrer_chain[0].is_retargeting());
}

TEST_F(SBNavigationObserverTest,
       CanceledRetargetingNavigationHasCorrectRedirects) {
  base::Time now = base::Time::Now();
  base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 1.0);

  SessionID source_tab = SessionID::NewUnique();
  SessionID target_tab = SessionID::NewUnique();

  // Add two navigations. A initially opens a new tab with url B, but cancels
  // that before it completes. It then navigates the new tab to C. We expect
  // that asking for the referrer chain for C has C as the event url.
  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->source_url = GURL("http://example.com/a");
  first_navigation->original_request_url = GURL("http://example.com/b");
  first_navigation->server_redirect_urls.emplace_back(
      "http://example.com/b_redirect1");
  first_navigation->server_redirect_urls.emplace_back(
      "http://example.com/b_redirect2");
  first_navigation->last_updated = one_second_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  first_navigation->source_tab_id = source_tab;
  first_navigation->target_tab_id = target_tab;
  first_navigation->has_committed = false;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->original_request_url = GURL("http://example.com/c");
  second_navigation->server_redirect_urls.emplace_back(
      "http://example.com/c_redirect1");
  second_navigation->server_redirect_urls.emplace_back(
      "http://example.com/c_redirect2");
  second_navigation->last_updated = now;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  second_navigation->source_tab_id = target_tab;
  second_navigation->target_tab_id = target_tab;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://example.com/c_redirect2"), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 10, &referrer_chain);

  ASSERT_EQ(1, referrer_chain.size());

  EXPECT_EQ("http://example.com/c_redirect2", referrer_chain[0].url());
  ASSERT_EQ(3, referrer_chain[0].server_redirect_chain_size());
  EXPECT_EQ("http://example.com/c",
            referrer_chain[0].server_redirect_chain(0).url());
  EXPECT_EQ("http://example.com/c_redirect1",
            referrer_chain[0].server_redirect_chain(1).url());
  EXPECT_EQ("http://example.com/c_redirect2",
            referrer_chain[0].server_redirect_chain(2).url());
  EXPECT_TRUE(referrer_chain[0].is_retargeting());
}

TEST_F(SBNavigationObserverTest, TestGetLatestPendingNavigationEvent) {
  base::Time now = base::Time::Now();
  base::Time one_minute_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 60.0);
  base::Time two_minute_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 120.0);
  GURL url("http://foo/0");
  content::MockNavigationHandle handle_0(url,
                                         web_contents()->GetPrimaryMainFrame());
  content::MockNavigationHandle handle_1(url,
                                         web_contents()->GetPrimaryMainFrame());
  content::MockNavigationHandle handle_2(url,
                                         web_contents()->GetPrimaryMainFrame());
  navigation_event_list()->RecordPendingNavigationEvent(
      &handle_0, CreateNavigationEventUniquePtr(url, one_minute_ago));
  navigation_event_list()->RecordPendingNavigationEvent(
      &handle_1, CreateNavigationEventUniquePtr(url, now));
  navigation_event_list()->RecordPendingNavigationEvent(
      &handle_2, CreateNavigationEventUniquePtr(url, two_minute_ago));
  ASSERT_EQ(3U, navigation_event_list()->PendingNavigationEventsSize());

  NavigationEvent* event =
      navigation_event_list()->FindPendingNavigationEvent(url);
  ASSERT_NE(nullptr, event);
  // FindPendingNavigationEvent should return the event for handle_1 because it
  // has the latest updated timestamp.
  EXPECT_EQ(now, event->last_updated);
}

TEST_F(SBNavigationObserverTest, SanitizesDataUrls) {
  base::Time now = base::Time::Now();
  base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 1.0);

  SessionID tab_id = SessionID::NewUnique();

  // Add two navigations. The first is renderer initiated from A to a data://
  // URL. The second is from the data:// URL to B.
  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->source_url = GURL("http://a.com/");
  first_navigation->original_request_url = GURL("data://private_data");
  first_navigation->last_updated = one_second_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  first_navigation->source_tab_id = tab_id;
  first_navigation->target_tab_id = tab_id;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->source_url = GURL("data://private_data");
  second_navigation->original_request_url = GURL("http://b.com/");
  second_navigation->last_updated = now;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  second_navigation->source_tab_id = tab_id;
  second_navigation->target_tab_id = tab_id;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://b.com/"), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 10, &referrer_chain);

  SafeBrowsingNavigationObserverManager::SanitizeReferrerChain(&referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());
  EXPECT_EQ(referrer_chain[0].referrer_url(),
            "data://"
            "A2368FB9B5FF3EDDF2860EF4998750024F7E4C6E2697F77269A13ADC84DCAD0E");
  EXPECT_EQ(referrer_chain[1].url(),
            "data://"
            "A2368FB9B5FF3EDDF2860EF4998750024F7E4C6E2697F77269A13ADC84DCAD0E");
}

}  // namespace safe_browsing
