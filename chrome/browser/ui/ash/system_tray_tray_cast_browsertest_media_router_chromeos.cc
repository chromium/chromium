// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/ui/ash/cast_config_controller_media_router.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/message_center/message_center.h"
#include "url/gurl.h"

using testing::_;

namespace {

const char kNotificationId[] = "chrome://cast";

// Helper to create a MediaSink intance.
media_router::MediaSink MakeSink(const std::string& id,
                                 const std::string& name) {
  return media_router::MediaSink(id, name, media_router::SinkIconType::GENERIC);
}

// Helper to create a MediaRoute instance.
media_router::MediaRoute MakeRoute(const std::string& route_id,
                                   const std::string& sink_id,
                                   bool is_local) {
  return media_router::MediaRoute(
      route_id, media_router::MediaSource::ForDesktop(), sink_id, "description",
      is_local, true /*for_display*/);
}

class SystemTrayTrayCastMediaRouterChromeOSTest : public InProcessBrowserTest {
 protected:
  SystemTrayTrayCastMediaRouterChromeOSTest() : InProcessBrowserTest() {}
  ~SystemTrayTrayCastMediaRouterChromeOSTest() override {}

  void ShowBubble() { tray_test_api_->ShowBubble(); }

  bool IsViewDrawn(int view_id) {
    return tray_test_api_->IsBubbleViewVisible(view_id, false /* open_tray */);
  }

  bool IsTrayVisible() { return IsViewDrawn(ash::VIEW_ID_CAST_MAIN_VIEW); }

  bool IsCastingNotificationVisible() {
    return !GetNotificationString().empty();
  }

  base::string16 GetNotificationString() {
    message_center::NotificationList::Notifications notification_set =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    for (auto* notification : notification_set) {
      if (notification->id() == kNotificationId)
        return notification->title();
    }
    return base::string16();
  }

  media_router::MediaSinksObserver* media_sinks_observer() const {
    DCHECK(media_sinks_observer_);
    return media_sinks_observer_;
  }

  media_router::MediaRoutesObserver* media_routes_observer() const {
    DCHECK(media_routes_observer_);
    return media_routes_observer_;
  }

 private:
  // InProcessBrowserTest:
  void SetUp() override {
    // This makes sure CastDeviceCache is not initialized until after the
    // MockMediaRouter is ready. (MockMediaRouter can't be constructed yet.)
    CastConfigControllerMediaRouter::SetMediaRouterForTest(nullptr);
    InProcessBrowserTest::SetUp();
  }

  void PreRunTestOnMainThread() override {
    media_router_ = std::make_unique<media_router::MockMediaRouter>();
    ON_CALL(*media_router_, RegisterMediaSinksObserver(_))
        .WillByDefault(Invoke(
            this, &SystemTrayTrayCastMediaRouterChromeOSTest::CaptureSink));
    ON_CALL(*media_router_, RegisterMediaRoutesObserver(_))
        .WillByDefault(Invoke(
            this, &SystemTrayTrayCastMediaRouterChromeOSTest::CaptureRoutes));
    CastConfigControllerMediaRouter::SetMediaRouterForTest(media_router_.get());
    InProcessBrowserTest::PreRunTestOnMainThread();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    tray_test_api_ = ash::SystemTrayTestApi::Create();
  }

  void PostRunTestOnMainThread() override {
    InProcessBrowserTest::PostRunTestOnMainThread();
    CastConfigControllerMediaRouter::SetMediaRouterForTest(nullptr);
  }

  bool CaptureSink(media_router::MediaSinksObserver* media_sinks_observer) {
    media_sinks_observer_ = media_sinks_observer;
    return true;
  }

  void CaptureRoutes(media_router::MediaRoutesObserver* media_routes_observer) {
    media_routes_observer_ = media_routes_observer;
  }

  std::unique_ptr<media_router::MockMediaRouter> media_router_;
  media_router::MediaSinksObserver* media_sinks_observer_ = nullptr;
  media_router::MediaRoutesObserver* media_routes_observer_ = nullptr;
  std::unique_ptr<ash::SystemTrayTestApi> tray_test_api_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayTrayCastMediaRouterChromeOSTest);
};

}  // namespace

// Verifies that we only show the tray view if there are available cast
// targets/sinks.
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastMediaRouterChromeOSTest,
                       VerifyCorrectVisiblityWithSinks) {
  ShowBubble();

  std::vector<media_router::MediaSink> zero_sinks;
  std::vector<media_router::MediaSink> one_sink;
  std::vector<media_router::MediaSink> two_sinks;
  one_sink.push_back(MakeSink("id1", "name"));
  two_sinks.push_back(MakeSink("id1", "name"));
  two_sinks.push_back(MakeSink("id2", "name"));

  // The tray should be hidden when there are no sinks.
  EXPECT_FALSE(IsTrayVisible());
  media_sinks_observer()->OnSinksUpdated(zero_sinks,
                                         std::vector<url::Origin>());
  // Flush mojo messages from the chrome object to the ash object.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsTrayVisible());

  // The tray should be visible with any more than zero sinks.
  media_sinks_observer()->OnSinksUpdated(one_sink, std::vector<url::Origin>());
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsTrayVisible());
  media_sinks_observer()->OnSinksUpdated(two_sinks, std::vector<url::Origin>());
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsTrayVisible());

  // And if all of the sinks go away, it should be hidden again.
  media_sinks_observer()->OnSinksUpdated(zero_sinks,
                                         std::vector<url::Origin>());
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsTrayVisible());
}

// Verifies that we show the cast view when we start a casting session, and that
// we display the correct cast session if there are multiple active casting
// sessions.
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastMediaRouterChromeOSTest,
                       VerifyCastingShowsCastView) {
  ShowBubble();

  // Setup the sinks.
  const std::vector<media_router::MediaSink> sinks = {
      MakeSink("remote_sink", "Remote Sink"),
      MakeSink("local_sink", "Local Sink")};
  media_sinks_observer()->OnSinksUpdated(sinks, std::vector<url::Origin>());
  content::RunAllPendingInMessageLoop();

  // Create route combinations. More details below.
  const media_router::MediaRoute non_local_route =
      MakeRoute("remote_route", "remote_sink", false /*is_local*/);
  const media_router::MediaRoute local_route =
      MakeRoute("local_route", "local_sink", true /*is_local*/);
  const std::vector<media_router::MediaRoute> no_routes;
  const std::vector<media_router::MediaRoute> non_local_routes{non_local_route};
  // We put the non-local route first to make sure that we prefer the local one.
  const std::vector<media_router::MediaRoute> multiple_routes{non_local_route,
                                                              local_route};

  // We do not show the cast view for non-local routes.
  media_routes_observer()->OnRoutesUpdated(
      non_local_routes, std::vector<media_router::MediaRoute::Id>());
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsCastingNotificationVisible());

  // If there are multiple routes active at the same time, then we need to
  // display the local route over a non-local route. This also verifies that we
  // display the cast view when we're casting.
  media_routes_observer()->OnRoutesUpdated(
      multiple_routes, std::vector<media_router::MediaRoute::Id>());
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsCastingNotificationVisible());
  EXPECT_NE(base::string16::npos,
            GetNotificationString().find(base::ASCIIToUTF16("Local Sink")));

  // When a casting session stops, we shouldn't display the cast view.
  media_routes_observer()->OnRoutesUpdated(
      no_routes, std::vector<media_router::MediaRoute::Id>());
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsCastingNotificationVisible());
}
