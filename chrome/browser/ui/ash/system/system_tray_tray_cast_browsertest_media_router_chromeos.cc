// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/cast/cast_detailed_view.h"
#include "ash/system/cast/unified_cast_detailed_view_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/cast_config/cast_config_controller_media_router.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/media_router/access_code_cast/access_code_cast_integration_browsertest.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/browser/media_sinks_observer.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/test/test_helper.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"

using ::ash::ProfileHelper;
using testing::_;
using user_manager::UserManager;

namespace {

const char kNotificationId[] = "chrome://cast";

const char kEndpointResponseSuccess[] =
    R"({
    "device": {
      "displayName": "test_device",
      "id": "1234",
      "deviceCapabilities": {
        "videoOut": true,
        "videoIn": true,
        "audioOut": true,
        "audioIn": true,
        "devMode": true
      },
      "networkInfo": {
        "hostName": "GoogleNet",
        "port": "666",
        "ipV4Address": "192.0.2.146",
        "ipV6Address": "2001:0db8:85a3:0000:0000:8a2e:0370:7334"
      }
    }
  })";

// Helper to create a MediaRoute instance.
media_router::MediaRoute MakeRoute(const std::string& route_id,
                                   const std::string& sink_id,
                                   bool is_local) {
  return media_router::MediaRoute(
      route_id, media_router::MediaSource::ForUnchosenDesktop(), sink_id,
      "description", is_local);
}

class SystemTrayTrayCastMediaRouterChromeOSTest : public InProcessBrowserTest {
 public:
  SystemTrayTrayCastMediaRouterChromeOSTest(
      const SystemTrayTrayCastMediaRouterChromeOSTest&) = delete;
  SystemTrayTrayCastMediaRouterChromeOSTest& operator=(
      const SystemTrayTrayCastMediaRouterChromeOSTest&) = delete;

 protected:
  SystemTrayTrayCastMediaRouterChromeOSTest() = default;

  ~SystemTrayTrayCastMediaRouterChromeOSTest() override = default;

  void ShowBubble() { tray_test_api_->ShowBubble(); }

  bool IsViewDrawn(int view_id) {
    return tray_test_api_->IsBubbleViewVisible(view_id, false /* open_tray */);
  }

  bool IsTrayVisible() { return IsViewDrawn(ash::VIEW_ID_FEATURE_TILE_CAST); }

  bool IsCastingNotificationVisible() {
    return !GetNotificationString().empty();
  }

  std::u16string GetNotificationString() {
    message_center::NotificationList::Notifications notification_set =
        message_center::MessageCenter::Get()->GetVisibleNotifications();
    for (message_center::Notification* notification : notification_set) {
      if (notification->id() == kNotificationId) {
        return notification->title();
      }
    }
    return std::u16string();
  }

  media_router::MediaSinksObserver* media_sinks_observer() const {
    DCHECK(media_sinks_observer_);
    return media_sinks_observer_;
  }

  media_router::MediaRoutesObserver* media_routes_observer() const {
    return &(*media_router_->routes_observers().begin());
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

  std::unique_ptr<media_router::MockMediaRouter> media_router_;
  raw_ptr<media_router::MediaSinksObserver, DanglingUntriaged>
      media_sinks_observer_ = nullptr;
  std::unique_ptr<ash::SystemTrayTestApi> tray_test_api_;
};

}  // namespace

// Verifies that we only show the tray view if there are available cast
// targets/sinks.
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastMediaRouterChromeOSTest,
                       VerifyCorrectVisiblityWithSinks) {
  ShowBubble();

    // The tray defaults to visible.
    EXPECT_TRUE(IsTrayVisible());

  std::vector<media_router::MediaSink> zero_sinks;
  std::vector<media_router::MediaSink> one_sink;
  std::vector<media_router::MediaSink> two_sinks;
  one_sink.push_back(media_router::CreateCastSink("id1", "name"));
  two_sinks.push_back(media_router::CreateCastSink("id1", "name"));
  two_sinks.push_back(media_router::CreateCastSink("id2", "name"));

  media_sinks_observer()->OnSinksUpdated(zero_sinks,
                                         std::vector<url::Origin>());
  // The tray is always visible.
  EXPECT_TRUE(IsTrayVisible());

  // The tray should be visible with any more than zero sinks.
  media_sinks_observer()->OnSinksUpdated(one_sink, std::vector<url::Origin>());
  EXPECT_TRUE(IsTrayVisible());
  media_sinks_observer()->OnSinksUpdated(two_sinks, std::vector<url::Origin>());
  EXPECT_TRUE(IsTrayVisible());

  // And if all of the sinks go away, it should be hidden again.
  media_sinks_observer()->OnSinksUpdated(zero_sinks,
                                         std::vector<url::Origin>());
  // The tray is always visible.
  EXPECT_TRUE(IsTrayVisible());
}

// Verifies that we show the cast view when we start a casting session, and that
// we display the correct cast session if there are multiple active casting
// sessions.
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastMediaRouterChromeOSTest,
                       VerifyCastingShowsCastView) {
  ShowBubble();

  // Setup the sinks.
  const std::vector<media_router::MediaSink> sinks = {
      media_router::CreateCastSink("remote_sink", "Remote Sink"),
      media_router::CreateCastSink("local_sink", "Local Sink")};
  media_sinks_observer()->OnSinksUpdated(sinks, std::vector<url::Origin>());

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
  media_routes_observer()->OnRoutesUpdated(non_local_routes);
  EXPECT_FALSE(IsCastingNotificationVisible());

  // If there are multiple routes active at the same time, then we need to
  // display the local route over a non-local route. This also verifies that we
  // display the cast view when we're casting.
  media_routes_observer()->OnRoutesUpdated(multiple_routes);
  EXPECT_TRUE(IsCastingNotificationVisible());
  EXPECT_NE(std::u16string::npos, GetNotificationString().find(u"Local Sink"));

  // When a casting session stops, we shouldn't display the cast view.
  media_routes_observer()->OnRoutesUpdated(no_routes);
  EXPECT_FALSE(IsCastingNotificationVisible());
}

class SystemTrayTrayCastAccessCodeChromeOSTest
    : public media_router::AccessCodeCastIntegrationBrowserTest {
 public:
  SystemTrayTrayCastAccessCodeChromeOSTest() {
    // Use consumer emails to avoid having to fake a policy fetch.
    login_mixin_.AppendRegularUsers(2);
    login_mixin_.SetShouldLaunchBrowser(true);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;
  }

  SystemTrayTrayCastAccessCodeChromeOSTest(
      const SystemTrayTrayCastAccessCodeChromeOSTest&) = delete;
  SystemTrayTrayCastAccessCodeChromeOSTest& operator=(
      const SystemTrayTrayCastAccessCodeChromeOSTest&) = delete;

  ~SystemTrayTrayCastAccessCodeChromeOSTest() override = default;

  void PreRunTestOnMainThread() override {
    CastConfigControllerMediaRouter::SetMediaRouterForTest(media_router_);
    InProcessBrowserTest::PreRunTestOnMainThread();
  }

  void SetUpOnMainThread() override {
    ash::test::UserSessionManagerTestApi session_manager_test_api(
        ash::UserSessionManager::GetInstance());
    session_manager_test_api.SetShouldLaunchBrowserInTests(true);
    session_manager_test_api.SetShouldObtainTokenHandleInTests(false);

    AccessCodeCastIntegrationBrowserTest::SetUpOnMainThread();
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    tray_test_api_ = ash::SystemTrayTestApi::Create();

    event_generator_ = std::make_unique<ui::test::EventGenerator>(  // IN-TEST
        ash::Shell::GetPrimaryRootWindow());
  }

  ui::test::EventGenerator* GetEventGenerator() {
    return event_generator_.get();
  }

 protected:
  void SetupUserProfile(const AccountId& account_id, bool allow_access_code) {
    user_ = UserManager::Get()->FindUser(account_id);
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user_);
    profile->GetPrefs()->SetBoolean(media_router::prefs::kAccessCodeCastEnabled,
                                    allow_access_code);
    content::RunAllTasksUntilIdle();
  }

  ash::UserContext CreateUserContext(const AccountId& account_id,
                                     const std::string& password) {
    ash::UserContext user_context(user_manager::UserType::kRegular, account_id);
    user_context.SetKey(ash::Key(password));
    user_context.SetPasswordKey(ash::Key(password));
    if (account_id.GetUserEmail() == FakeGaiaMixin::kEnterpriseUser1) {
      user_context.SetRefreshToken(FakeGaiaMixin::kTestRefreshToken1);
    } else if (account_id.GetUserEmail() == FakeGaiaMixin::kEnterpriseUser2) {
      user_context.SetRefreshToken(FakeGaiaMixin::kTestRefreshToken2);
    }
    return user_context;
  }

  void ShowBubble() { tray_test_api_->ShowBubble(); }

  void CloseBubble() { tray_test_api_->CloseBubble(); }

  bool IsViewDrawn(int view_id) {
    return tray_test_api_->IsBubbleViewVisible(view_id, /* open_tray */ false);
  }

  void ClickView(int view_id) { tray_test_api_->ClickBubbleView(view_id); }

  bool IsTrayVisible() { return IsViewDrawn(ash::VIEW_ID_FEATURE_TILE_CAST); }

  // Returns the status area widget.
  ash::StatusAreaWidget* FindStatusAreaWidget() {
    return ash::Shelf::ForWindow(ash::Shell::GetRootWindowForNewWindows())
        ->shelf_widget()
        ->status_area_widget();
  }

  // Performs a tap of the specified |view|.
  void TapOn(const views::View* view) {
    GetEventGenerator()->MoveTouch(view->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickLeftButton();
  }

  ash::CastDetailedView* GetCastDetailedView() {
    return GetUnifiedCastDetailedViewController()
        ->get_cast_detailed_view_for_testing();  // IN-TEST
  }

  ash::UnifiedSystemTrayController* GetUnifiedSystemTrayController() {
    return FindStatusAreaWidget()
        ->unified_system_tray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  ash::UnifiedCastDetailedViewController*
  GetUnifiedCastDetailedViewController() {
    return static_cast<ash::UnifiedCastDetailedViewController*>(
        GetUnifiedSystemTrayController()->detailed_view_controller());
  }

  AccountId account_id1_;
  AccountId account_id2_;

  ash::LoginManagerMixin login_mixin_{&mixin_host_};

  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  raw_ptr<const user_manager::User, DanglingUntriaged> user_;

 private:
  std::unique_ptr<ash::SystemTrayTestApi> tray_test_api_;
};

IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastAccessCodeChromeOSTest,
                       PolicyOffNoSinksNoVisibleTray) {
  const ash::UserContext user_context =
      CreateUserContext(account_id1_, "password");

  // Login a user that does not have access code casting enabled.
  ASSERT_TRUE(login_mixin_.LoginAndWaitForActiveSession(user_context));
  SetupUserProfile(account_id1_, /* allow_access_code */ false);

  ShowBubble();

    // The tray is always visible.
  EXPECT_TRUE(IsTrayVisible());
}

IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastAccessCodeChromeOSTest,
                       PolicyOnNoSinksVisibleTray) {
  const ash::UserContext user_context =
      CreateUserContext(account_id2_, "password");

  // Login a user that does have access code casting enabled.
  ASSERT_TRUE(login_mixin_.LoginAndWaitForActiveSession(user_context));
  SetupUserProfile(account_id2_, /* allow_access_code */ true);

  ShowBubble();

  // Even though there are no sinks, since this user does have access code
  // casting enabled, the tray should be visible.
  EXPECT_TRUE(IsTrayVisible());
}

IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastAccessCodeChromeOSTest,
                       SimulateValidCastingWorkflow) {
  AddScreenplayTag(AccessCodeCastIntegrationBrowserTest::
                       kAccessCodeCastNewDeviceScreenplayTag);

  const ash::UserContext user_context =
      CreateUserContext(account_id2_, "password");

  // Login a user that does have access code casting enabled.
  ASSERT_TRUE(login_mixin_.LoginAndWaitForActiveSession(user_context));
  SetupUserProfile(account_id2_, /* allow_access_code */ true);

  ShowBubble();

  // Show the Cast detailed view menu.
  GetUnifiedSystemTrayController()->ShowCastDetailedView();

  auto* detailed_cast_view = GetCastDetailedView();
  ASSERT_TRUE(detailed_cast_view);

  auto* access_code_cast_button =
      detailed_cast_view->get_add_access_code_device_for_testing();  // IN-TEST
  ASSERT_TRUE(access_code_cast_button);
  ASSERT_TRUE(access_code_cast_button->GetEnabled());

  // Mock a successful fetch from our server.
  SetEndpointFetcherMockResponse(kEndpointResponseSuccess, net::HTTP_OK,
                                 net::OK);

  // Simulate a successful opening of the channel.
  SetMockOpenChannelCallbackResponse(true);

  SetUpPrimaryAccountWithHostedDomain(
      signin::ConsentLevel::kSync,
      ProfileHelper::Get()->GetProfileByUser(user_), /*sign_in_account=*/false);

  content::WebContentsAddedObserver observer;
  TapOn(access_code_cast_button);

  content::WebContents* dialog_contents = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(dialog_contents));

  SetAccessCode("abcdef", dialog_contents);

  // TODO(crbug.com/1291738): There is a validation process with desktop media
  // requests which are unnecessary for the complexity of this browsertest. We
  // are just passing in a hardcoded magic string instead.
  ExpectStartRouteCallFromTabMirroring(
      "cast:<1234>", "urn:x-org.chromium.media:source:desktop", nullptr,
      base::Seconds(120), media_router_);

  PressSubmitAndWaitForClose(dialog_contents);
}

// First open the cast dialog from browser, then open another cast dialog from
// the system tray. Before the change, such behavior will cause a crash. After
// the change, the first dialog will close when the second dialog opens.
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastAccessCodeChromeOSTest,
                       BrowserAndSystemTrayCasting) {
  const ash::UserContext user_context =
      CreateUserContext(account_id2_, "password");

  // Login a user that does have access code casting enabled.
  ASSERT_TRUE(login_mixin_.LoginAndWaitForActiveSession(user_context));
  SetupUserProfile(account_id2_, /* allow_access_code */ true);

  // Show the first cast dialog from the browser.
  SelectFirstBrowser();
  EnableAccessCodeCasting();
  ASSERT_TRUE(ShowDialog());

  ShowBubble();

  // Show the Cast detailed view menu.
  GetUnifiedSystemTrayController()->ShowCastDetailedView();

  auto* detailed_cast_view = GetCastDetailedView();
  ASSERT_TRUE(detailed_cast_view);

  auto* access_code_cast_button =
      detailed_cast_view->get_add_access_code_device_for_testing();  // IN-TEST
  ASSERT_TRUE(access_code_cast_button);
  ASSERT_TRUE(access_code_cast_button->GetEnabled());

  // Mock a successful fetch from our server.
  SetEndpointFetcherMockResponse(kEndpointResponseSuccess, net::HTTP_OK,
                                 net::OK);

  // Simulate a successful opening of the channel.
  SetMockOpenChannelCallbackResponse(true);

  SetUpPrimaryAccountWithHostedDomain(
      signin::ConsentLevel::kSync,
      ProfileHelper::Get()->GetProfileByUser(user_), /*sign_in_account=*/false);

  content::WebContentsAddedObserver observer;

  // Show the second dialog from the system tray.
  TapOn(access_code_cast_button);

  content::WebContents* dialog_contents = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(dialog_contents));

  SetAccessCode("abcdef", dialog_contents);

  // TODO(crbug.com/1291738): There is a validation process with desktop media
  // requests which are unnecessary for the complexity of this browsertest. We
  // are just passing in a hardcoded magic string instead.
  ExpectStartRouteCallFromTabMirroring(
      "cast:<1234>", "urn:x-org.chromium.media:source:desktop", nullptr,
      base::Seconds(120), media_router_);

  PressSubmitAndWaitForClose(dialog_contents);
}
