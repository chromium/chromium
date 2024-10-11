// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/shell.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "url/gurl.h"

using ::ash::ProfileHelper;
using user_manager::UserManager;

namespace {

const char kManager[] = "admin@example.com";
const char16_t kManager16[] = u"admin@example.com";
const char kNewUser[] = "new_test_user@gmail.com";
const char kNewGaiaID[] = "11111";
const char kManagedUser[] = "user@example.com";
const char kManagedGaiaID[] = "33333";

}  // namespace

using SystemTrayClientEnterpriseTest = policy::DevicePolicyCrosBrowserTest;

IN_PROC_BROWSER_TEST_F(SystemTrayClientEnterpriseTest, TrayEnterprise) {
  auto test_api = ash::SystemTrayTestApi::Create();

  // Managed devices show an item in the menu.
  EXPECT_TRUE(test_api->IsBubbleViewVisible(ash::VIEW_ID_QS_MANAGED_BUTTON,
                                            true /* open_tray */));
  std::u16string expected_text =
      l10n_util::GetStringFUTF16(IDS_ASH_SHORT_MANAGED_BY, u"example.com");
  EXPECT_EQ(expected_text,
            test_api->GetBubbleViewTooltip(ash::VIEW_ID_QS_MANAGED_BUTTON));

  // Clicking the item opens the management page.
  test_api->ClickBubbleView(ash::VIEW_ID_QS_MANAGED_BUTTON);
  EXPECT_EQ(
      GURL(chrome::kChromeUIManagementURL),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

using ConsumerDeviceTest = MixinBasedInProcessBrowserTest;

// Verify that the management device mode is indeed none of the
// enterprise licenses.
IN_PROC_BROWSER_TEST_F(ConsumerDeviceTest, WithNoLicense) {
  EXPECT_EQ(ash::Shell::Get()
                ->system_tray_model()
                ->enterprise_domain()
                ->management_device_mode(),
            ash::ManagementDeviceMode::kNone);
}

class EnterpriseManagedTest : public MixinBasedInProcessBrowserTest {
 public:
  EnterpriseManagedTest() {
    device_state_.set_skip_initial_policy_setup(true);
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kEnableKioskLoginScreen);
  }
  ~EnterpriseManagedTest() override = default;
  EnterpriseManagedTest(const EnterpriseManagedTest&) = delete;
  void operator=(const EnterpriseManagedTest&) = delete;

 protected:
  policy::DevicePolicyCrosTestHelper* policy_helper() {
    return &policy_helper_;
  }

 private:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  policy::DevicePolicyCrosTestHelper policy_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verify that the management device mode is indeed Kiosk Sku.
IN_PROC_BROWSER_TEST_F(EnterpriseManagedTest, WithKioskSku) {
  policy_helper()->device_policy()->policy_data().set_license_sku(
      policy::kKioskSkuName);
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  EXPECT_EQ(ash::Shell::Get()
                ->system_tray_model()
                ->enterprise_domain()
                ->management_device_mode(),
            ash::ManagementDeviceMode::kKioskSku);
}

// Verify that the management device mode is indeed education license.
IN_PROC_BROWSER_TEST_F(EnterpriseManagedTest, WithEducationLicense) {
  policy_helper()->device_policy()->policy_data().set_market_segment(
      enterprise_management::PolicyData::ENROLLED_EDUCATION);
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  EXPECT_EQ(ash::Shell::Get()
                ->system_tray_model()
                ->enterprise_domain()
                ->management_device_mode(),
            ash::ManagementDeviceMode::kChromeEducation);
}

// Verify that the management device mode is indeed enterprise license.
IN_PROC_BROWSER_TEST_F(EnterpriseManagedTest, WithEnterpriseLicense) {
  policy_helper()->device_policy()->policy_data().set_market_segment(
      enterprise_management::PolicyData::ENROLLED_ENTERPRISE);
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  EXPECT_EQ(ash::Shell::Get()
                ->system_tray_model()
                ->enterprise_domain()
                ->management_device_mode(),
            ash::ManagementDeviceMode::kChromeEnterprise);
}

// Verify that the management device mode is indeed unknown when the market
// segment of the device policy data does not have value.
IN_PROC_BROWSER_TEST_F(EnterpriseManagedTest, WithUnknownLicense) {
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  EXPECT_EQ(ash::Shell::Get()
                ->system_tray_model()
                ->enterprise_domain()
                ->management_device_mode(),
            ash::ManagementDeviceMode::kOther);
}

class SystemTrayClientClockTest : public ash::LoginManagerTest {
 public:
  SystemTrayClientClockTest() : LoginManagerTest() {
    // Use consumer emails to avoid having to fake a policy fetch.
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;
  }

  SystemTrayClientClockTest(const SystemTrayClientClockTest&) = delete;
  SystemTrayClientClockTest& operator=(const SystemTrayClientClockTest&) =
      delete;

  ~SystemTrayClientClockTest() override = default;

  void SetupUserProfile(const AccountId& account_id, bool use_24_hour_clock) {
    const user_manager::User* user = UserManager::Get()->FindUser(account_id);
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
    profile->GetPrefs()->SetBoolean(prefs::kUse24HourClock, use_24_hour_clock);
    // Allow clock setting to be sent to ash over mojo.
    content::RunAllPendingInMessageLoop();
  }

 protected:
  AccountId account_id1_;
  AccountId account_id2_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

// Test that clock type is taken from user profile for current active user.
IN_PROC_BROWSER_TEST_F(SystemTrayClientClockTest, TestMultiProfile24HourClock) {
  auto tray_test_api = ash::SystemTrayTestApi::Create();

  // Login a user with a 24-hour clock.
  LoginUser(account_id1_);
  SetupUserProfile(account_id1_, true /* use_24_hour_clock */);
  EXPECT_TRUE(tray_test_api->Is24HourClock());

  // Add a user with a 12-hour clock.
  ash::UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(account_id2_);
  SetupUserProfile(account_id2_, false /* use_24_hour_clock */);
  EXPECT_FALSE(tray_test_api->Is24HourClock());

  // Switch back to the user with the 24-hour clock.
  UserManager::Get()->SwitchActiveUser(account_id1_);
  // Allow clock setting to be sent to ash over mojo.
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(tray_test_api->Is24HourClock());
}

// Test that on the login and lock screen clock type is taken from user profile
// of the focused pod.
IN_PROC_BROWSER_TEST_F(SystemTrayClientClockTest, PRE_FocusedPod24HourClock) {
  auto tray_test_api = ash::SystemTrayTestApi::Create();

  // Login a user with a 24-hour clock.
  LoginUser(account_id1_);
  SetupUserProfile(account_id1_, true /* use_24_hour_clock */);
  EXPECT_TRUE(tray_test_api->Is24HourClock());

  // Add a user with a 12-hour clock.
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  SetupUserProfile(account_id2_, false /* use_24_hour_clock */);
  EXPECT_FALSE(tray_test_api->Is24HourClock());

  // Test lock screen.
  ash::ScreenLockerTester locker;
  locker.Lock();

  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id1_));
  EXPECT_TRUE(tray_test_api->Is24HourClock());

  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id2_));
  EXPECT_FALSE(tray_test_api->Is24HourClock());
}

IN_PROC_BROWSER_TEST_F(SystemTrayClientClockTest, FocusedPod24HourClock) {
  auto tray_test_api = ash::SystemTrayTestApi::Create();
  // Test login screen.
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id1_));
  EXPECT_TRUE(tray_test_api->Is24HourClock());

  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id2_));
  EXPECT_FALSE(tray_test_api->Is24HourClock());
}

class SystemTrayClientClockUnknownPrefTest
    : public SystemTrayClientClockTest,
      public ash::LocalStateMixin::Delegate {
 public:
  SystemTrayClientClockUnknownPrefTest() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kSystemUse24HourClock, true);
  }
  // ash::localStateMixin::Delegate:
  void SetUpLocalState() override {
    user_manager::KnownUser known_user(g_browser_process->local_state());
    // First user does not have a preference.
    ASSERT_FALSE(known_user.FindBoolPath(account_id1_, ::prefs::kUse24HourClock)
                     .has_value());

    // Set preference for the second user only.
    known_user.SetBooleanPref(account_id2_, ::prefs::kUse24HourClock, false);
  }

 protected:
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ash::LocalStateMixin local_state_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(SystemTrayClientClockUnknownPrefTest, SwitchToDefault) {
  // Check default value.
  ASSERT_EQ(base::GetHourClockType(), base::k12HourClock);

  auto tray_test_api = ash::SystemTrayTestApi::Create();
  EXPECT_EQ(ash::LoginScreenTestApi::GetFocusedUser(), account_id1_);
  // Should be system setting because the first user does not have a preference.
  EXPECT_TRUE(tray_test_api->Is24HourClock());

  // Check user with the set preference.
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id2_));
  EXPECT_FALSE(tray_test_api->Is24HourClock());

  // Should get back to the system settings.
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id1_));
  EXPECT_TRUE(tray_test_api->Is24HourClock());
}

class SystemTrayClientEnterpriseAccountTest : public ash::LoginManagerTest {
 protected:
  SystemTrayClientEnterpriseAccountTest() {
    std::unique_ptr<ash::ScopedUserPolicyUpdate> scoped_user_policy_update =
        user_policy_mixin_.RequestPolicyUpdate();
    scoped_user_policy_update->policy_data()->set_managed_by(kManager);
  }

  const ash::LoginManagerMixin::TestUserInfo unmanaged_user_{
      AccountId::FromUserEmailGaiaId(kNewUser, kNewGaiaID)};
  const ash::LoginManagerMixin::TestUserInfo managed_user_{
      AccountId::FromUserEmailGaiaId(kManagedUser, kManagedGaiaID)};
  ash::UserPolicyMixin user_policy_mixin_{&mixin_host_,
                                          managed_user_.account_id};
  ash::LoginManagerMixin login_mixin_{&mixin_host_,
                                      {managed_user_, unmanaged_user_}};
};

IN_PROC_BROWSER_TEST_F(SystemTrayClientEnterpriseAccountTest,
                       TrayEnterpriseManagedAccount) {
  auto test_api = ash::SystemTrayTestApi::Create();

  // User hasn't signed in yet, user management should not be shown in tray.
  EXPECT_FALSE(test_api->IsBubbleViewVisible(ash::VIEW_ID_QS_MANAGED_BUTTON,
                                             true /* open_tray */));

  // After login, the tray should show user management information.
  LoginUser(managed_user_.account_id);
  EXPECT_TRUE(test_api->IsBubbleViewVisible(ash::VIEW_ID_QS_MANAGED_BUTTON,
                                            true /* open_tray */));
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_SHORT_MANAGED_BY, kManager16),
            test_api->GetBubbleViewTooltip(ash::VIEW_ID_QS_MANAGED_BUTTON));

  // Switch to unmanaged account should still show the managed string (since the
  // primary user is managed user). However, the string should not contain the
  // account manager of the primary user.
  ash::UserAddingScreen::Get()->Start();
  AddUser(unmanaged_user_.account_id);
  EXPECT_TRUE(test_api->IsBubbleViewVisible(ash::VIEW_ID_QS_MANAGED_BUTTON,
                                            true /* open_tray */));
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_ENTERPRISE_DEVICE_MANAGED,
                                       ui::GetChromeOSDeviceName()),
            test_api->GetBubbleViewTooltip(ash::VIEW_ID_QS_MANAGED_BUTTON));

  // Switch back to managed account.
  UserManager::Get()->SwitchActiveUser(managed_user_.account_id);
  EXPECT_TRUE(test_api->IsBubbleViewVisible(ash::VIEW_ID_QS_MANAGED_BUTTON,
                                            true /* open_tray */));
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_SHORT_MANAGED_BY, kManager16),
            test_api->GetBubbleViewTooltip(ash::VIEW_ID_QS_MANAGED_BUTTON));
}

IN_PROC_BROWSER_TEST_F(SystemTrayClientEnterpriseAccountTest,
                       TrayEnterpriseUnmanagedAccount) {
  auto test_api = ash::SystemTrayTestApi::Create();

  EXPECT_FALSE(test_api->IsBubbleViewVisible(ash::VIEW_ID_QS_MANAGED_BUTTON,
                                             true /* open_tray */));

  // After login with unmanaged user, the tray should not show user management
  // information.
  LoginUser(unmanaged_user_.account_id);
  EXPECT_FALSE(test_api->IsBubbleViewVisible(ash::VIEW_ID_QS_MANAGED_BUTTON,
                                             true /* open_tray */));
}

class SystemTrayClientEnterpriseSessionRestoreTest
    : public SystemTrayClientEnterpriseAccountTest {
 protected:
  SystemTrayClientEnterpriseSessionRestoreTest() {
    login_mixin_.set_session_restore_enabled();
  }
};

IN_PROC_BROWSER_TEST_F(SystemTrayClientEnterpriseSessionRestoreTest,
                       PRE_SessionRestore) {
  LoginUser(managed_user_.account_id);
}

IN_PROC_BROWSER_TEST_F(SystemTrayClientEnterpriseSessionRestoreTest,
                       SessionRestore) {
  auto test_api = ash::SystemTrayTestApi::Create();

  // Verify that tray is showing info on chrome restart.
  EXPECT_TRUE(test_api->IsBubbleViewVisible(ash::VIEW_ID_QS_MANAGED_BUTTON,
                                            true /* open_tray */));
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_SHORT_MANAGED_BY, kManager16),
            test_api->GetBubbleViewTooltip(ash::VIEW_ID_QS_MANAGED_BUTTON));
}

class SystemTrayClientShowCalendarTest : public ash::LoginManagerTest {
 public:
  SystemTrayClientShowCalendarTest() {
    login_mixin_.AppendRegularUsers(1);
    account_id_ = login_mixin_.users()[0].account_id;
  }

  SystemTrayClientShowCalendarTest(const SystemTrayClientShowCalendarTest&) =
      delete;
  SystemTrayClientShowCalendarTest& operator=(
      const SystemTrayClientShowCalendarTest&) = delete;

  ~SystemTrayClientShowCalendarTest() override = default;

  apps::AppPtr MakeApp(const char* app_id, const char* name) {
    auto google_meet_filter =
        apps_util::MakeIntentFilterForUrlScope(GURL("https://meet.google.com"));
    auto calendar_filter = apps_util::MakeIntentFilterForUrlScope(
        GURL("https://calendar.google.com"));
    apps::AppPtr app =
        std::make_unique<apps::App>(apps::AppType::kChromeApp, app_id);
    app->name = name;
    app->short_name = name;
    app->readiness = apps::Readiness::kReady;
    app->handles_intents = true;
    app->intent_filters.push_back(google_meet_filter->Clone());
    app->intent_filters.push_back(calendar_filter->Clone());
    return app;
  }

  apps::AppServiceProxyAsh* proxy() {
    const user_manager::User* user = UserManager::Get()->FindUser(account_id_);
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
    return apps::AppServiceProxyFactory::GetForProfile(profile);
  }

  void InstallApp(const char* app_id, const char* name) {
    std::vector<apps::AppPtr> registry_deltas;
    registry_deltas.push_back(MakeApp(app_id, name));
    proxy()->OnApps(std::move(registry_deltas), apps::AppType::kChromeApp,
                    /*should_notify_initialized=*/false);
  }

  void SetPreferredApp(const char* app_id) {
    proxy()->SetSupportedLinksPreference(app_id);
  }

 protected:
  AccountId account_id_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(SystemTrayClientShowCalendarTest, NoEventUrl) {
  auto tray_test_api = ash::SystemTrayTestApi::Create();

  // Login a user.
  LoginUser(account_id_);

  // Show the calendar, no URL and no PWA installed. We expect to navigate to
  // `date`.
  const char kExpectedUrlStr[] =
      "https://calendar.google.com/calendar/r/week/2021/11/18";
  GURL final_url;
  bool opened_pwa = false;
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  ash::Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
      std::nullopt, date, opened_pwa, final_url);
  EXPECT_FALSE(opened_pwa);
  EXPECT_EQ(final_url.spec(), GURL(kExpectedUrlStr).spec());

  // Now install the calendar PWA.
  InstallApp(web_app::kGoogleCalendarAppId, "Google Calendar");
  opened_pwa = false;
  final_url = GURL();
  ash::Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
      std::nullopt, date, opened_pwa, final_url);
  EXPECT_TRUE(opened_pwa);
  EXPECT_EQ(final_url.spec(), GURL(kExpectedUrlStr).spec());
}

IN_PROC_BROWSER_TEST_F(SystemTrayClientShowCalendarTest, OfficialEventUrl) {
  auto tray_test_api = ash::SystemTrayTestApi::Create();

  LoginUser(account_id_);

  // Show the calendar, with an event URL that has the "official" prefix, no PWA
  // installed.
  const char kOfficialCalendarEventUrl[] =
      "https://calendar.google.com/calendar/event?eid=m00n";
  GURL event_url(kOfficialCalendarEventUrl);
  GURL final_url;
  bool opened_pwa = false;
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  ash::Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
      event_url, date, opened_pwa, final_url);
  EXPECT_FALSE(opened_pwa);
  EXPECT_EQ(final_url.spec(), event_url.spec());

  // Install the calendar PWA.
  InstallApp(web_app::kGoogleCalendarAppId, "Google Calendar");
  opened_pwa = false;
  final_url = GURL();
  ash::Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
      event_url, date, opened_pwa, final_url);
  EXPECT_TRUE(opened_pwa);
  EXPECT_EQ(final_url.spec(), event_url.spec());
}

IN_PROC_BROWSER_TEST_F(SystemTrayClientShowCalendarTest, UnofficialEventUrl) {
  auto tray_test_api = ash::SystemTrayTestApi::Create();

  LoginUser(account_id_);

  // Show the calendar, with an event URL that does not have the "official"
  // prefix, no PWA installed.
  const char kOfficialCalendarEventUrl[] =
      "https://calendar.google.com/calendar/event?eid=m00n";
  const char kUnofficialCalendarEventUrl[] =
      "https://www.google.com/calendar/event?eid=m00n";
  GURL event_url(kUnofficialCalendarEventUrl);
  GURL final_url;
  bool opened_pwa = false;
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  ash::Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
      event_url, date, opened_pwa, final_url);
  EXPECT_FALSE(opened_pwa);
  EXPECT_EQ(final_url.spec(), GURL(kOfficialCalendarEventUrl).spec());

  // Install the calendar PWA.
  InstallApp(web_app::kGoogleCalendarAppId, "Google Calendar");
  opened_pwa = false;
  final_url = GURL();
  ash::Shell::Get()->system_tray_model()->client()->ShowCalendarEvent(
      event_url, date, opened_pwa, final_url);
  EXPECT_TRUE(opened_pwa);
  EXPECT_EQ(final_url.spec(), GURL(kOfficialCalendarEventUrl).spec());
}

class SystemTrayClientShowVideoConferenceTest
    : public SystemTrayClientShowCalendarTest {
 public:
  SystemTrayClientShowVideoConferenceTest() = default;

  ~SystemTrayClientShowVideoConferenceTest() override = default;

 protected:
  // ash::LoginManagerTest:
  void SetUpOnMainThread() override {
    ash::LoginManagerTest::SetUpOnMainThread();
    LoginUser(account_id_);
    browser_ = CreateBrowser(
        ash::ProfileHelper::Get()->GetProfileByAccountId(account_id_));
    ASSERT_TRUE(browser_);
  }

  raw_ptr<Browser, DanglingUntriaged> browser_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(SystemTrayClientShowVideoConferenceTest,
                       LaunchGoogleMeetUrlInBrowser_WhenAppIsNotInstalled) {
  const auto kVideoConferenceUrl = GURL("https://meet.google.com/abc-123");

  ash::Shell::Get()->system_tray_model()->client()->ShowVideoConference(
      kVideoConferenceUrl);

  EXPECT_EQ(
      GURL(kVideoConferenceUrl),
      browser_->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(
    SystemTrayClientShowVideoConferenceTest,
    LaunchGoogleMeetUrlInBrowser_WhenAppIsInstalledButNotPreferred) {
  const auto kVideoConferenceUrl = GURL("https://meet.google.com/abc-123");
  InstallApp(web_app::kGoogleMeetAppId, "Google Meet");

  ash::Shell::Get()->system_tray_model()->client()->ShowVideoConference(
      kVideoConferenceUrl);

  EXPECT_EQ(
      GURL(kVideoConferenceUrl),
      browser_->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(
    SystemTrayClientShowVideoConferenceTest,
    LaunchGoogleMeetUrlInApp_WhenAppIsInstalledAndPreferred) {
  const auto kVideoConferenceUrl = GURL("https://meet.google.com/abc-123");
  InstallApp(web_app::kGoogleMeetAppId, "Google Meet");
  SetPreferredApp(web_app::kGoogleMeetAppId);

  ASSERT_EQ(
      web_app::kGoogleMeetAppId,
      proxy()->PreferredAppsList().FindPreferredAppForUrl(kVideoConferenceUrl));

  ash::Shell::Get()->system_tray_model()->client()->ShowVideoConference(
      kVideoConferenceUrl);

  // Expect the url not to have opened in the browser.
  EXPECT_NE(
      GURL(kVideoConferenceUrl),
      browser_->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(SystemTrayClientShowVideoConferenceTest,
                       Launch3PVideoConferenceUrlInBrowser) {
  const auto kVideoConferenceUrl = GURL("https://some.third.party.com/abc-123");

  ash::Shell::Get()->system_tray_model()->client()->ShowVideoConference(
      kVideoConferenceUrl);

  EXPECT_EQ(
      GURL(kVideoConferenceUrl),
      browser_->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

class SystemTrayClientShowChannelInfoGiveFeedbackTest
    : public ash::LoginManagerTest {
 public:
  SystemTrayClientShowChannelInfoGiveFeedbackTest() {
    login_mixin_.AppendRegularUsers(1);
    account_id_ = login_mixin_.users()[0].account_id;
  }

  SystemTrayClientShowChannelInfoGiveFeedbackTest(
      const SystemTrayClientShowCalendarTest&) = delete;
  SystemTrayClientShowChannelInfoGiveFeedbackTest& operator=(
      const SystemTrayClientShowChannelInfoGiveFeedbackTest&) = delete;

  ~SystemTrayClientShowChannelInfoGiveFeedbackTest() override = default;

 protected:
  AccountId account_id_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

// TODO(crbug.com/40857702): Flaky on release bots.
#if defined(NDEBUG)
#define MAYBE_RecordFeedbackSourceChannelIndicator \
  DISABLED_RecordFeedbackSourceChannelIndicator
#else
#define MAYBE_RecordFeedbackSourceChannelIndicator \
  RecordFeedbackSourceChannelIndicator
#endif
IN_PROC_BROWSER_TEST_F(SystemTrayClientShowChannelInfoGiveFeedbackTest,
                       MAYBE_RecordFeedbackSourceChannelIndicator) {
  base::HistogramTester histograms;
  auto tray_test_api = ash::SystemTrayTestApi::Create();

  LoginUser(account_id_);

  histograms.ExpectBucketCount("Feedback.RequestSource",
                               feedback::kFeedbackSourceChannelIndicator,
                               /*expected_count=*/0);
  ash::Shell::Get()
      ->system_tray_model()
      ->client()
      ->ShowChannelInfoGiveFeedback();
  histograms.ExpectBucketCount("Feedback.RequestSource",
                               feedback::kFeedbackSourceChannelIndicator,
                               /*expected_count=*/1);
}
