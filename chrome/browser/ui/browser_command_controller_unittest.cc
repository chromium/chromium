// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/browser_command_controller.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/performance_manager/public/features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if ((BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)) && \
     BUILDFLAG(ENABLE_EXTENSIONS))
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#endif  // ((BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)) &&
        // BUILDFLAG(ENABLE_EXTENSIONS))

class BrowserCommandControllerTest : public BrowserWithTestWindowTest {
 public:
  BrowserCommandControllerTest() = default;
};

TEST_F(BrowserCommandControllerTest, IsReservedCommandOrKey) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // F1-3 keys are reserved Chrome accelerators on Chrome OS.
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_BACK, input::NativeWebKeyboardEvent(ui::KeyEvent(
                    ui::EventType::kKeyPressed, ui::VKEY_BROWSER_BACK,
                    ui::DomCode::BROWSER_BACK, 0))));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FORWARD, input::NativeWebKeyboardEvent(ui::KeyEvent(
                       ui::EventType::kKeyPressed, ui::VKEY_BROWSER_FORWARD,
                       ui::DomCode::BROWSER_FORWARD, 0))));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD, input::NativeWebKeyboardEvent(ui::KeyEvent(
                      ui::EventType::kKeyPressed, ui::VKEY_BROWSER_REFRESH,
                      ui::DomCode::BROWSER_REFRESH, 0))));

  // When there are modifier keys pressed, don't reserve.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD_BYPASSING_CACHE, input::NativeWebKeyboardEvent(ui::KeyEvent(
                                      ui::EventType::kKeyPressed, ui::VKEY_F3,
                                      ui::DomCode::F3, ui::EF_SHIFT_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD_BYPASSING_CACHE, input::NativeWebKeyboardEvent(ui::KeyEvent(
                                      ui::EventType::kKeyPressed, ui::VKEY_F3,
                                      ui::DomCode::F3, ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FULLSCREEN, input::NativeWebKeyboardEvent(
                          ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_F4,
                                       ui::DomCode::F4, ui::EF_SHIFT_DOWN))));

  // F4-10 keys are not reserved since they are Ash accelerators.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F4, ui::DomCode::F4, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F5, ui::DomCode::F5, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F6, ui::DomCode::F6, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F7, ui::DomCode::F7, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F8, ui::DomCode::F8, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F9, ui::DomCode::F9, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F10, ui::DomCode::F10, 0))));

  // Shift+Control+Alt+F3 is also an Ash accelerator. Don't reserve it.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F3, ui::DomCode::F3,
              ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN))));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(USE_AURA)
  // Ctrl+n, Ctrl+w are reserved while Ctrl+f is not.

  // The input::NativeWebKeyboardEvent constructor is available only when
  // USE_AURA is #defined.
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_NEW_WINDOW, input::NativeWebKeyboardEvent(ui::KeyEvent(
                          ui::EventType::kKeyPressed, ui::VKEY_N,
                          ui::DomCode::US_N, ui::EF_CONTROL_DOWN))));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_CLOSE_TAB, input::NativeWebKeyboardEvent(ui::KeyEvent(
                         ui::EventType::kKeyPressed, ui::VKEY_W,
                         ui::DomCode::US_W, ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FIND, input::NativeWebKeyboardEvent(
                    ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_F,
                                 ui::DomCode::US_F, ui::EF_CONTROL_DOWN))));
#endif  // USE_AURA
}

TEST_F(BrowserCommandControllerTest, IsReservedCommandOrKeyIsApp) {
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "app",
      /*trusted_source=*/true, browser()->window()->GetBounds(), profile(),
      /*user_gesture=*/true);
  params.window = browser()->window();
  set_browser(Browser::Create(params));

  ASSERT_TRUE(browser()->is_type_app());

  // When is_type_app(), no keys are reserved.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_BACK,
      input::NativeWebKeyboardEvent(ui::KeyEvent(
          ui::EventType::kKeyPressed, ui::VKEY_F1, ui::DomCode::F1, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FORWARD,
      input::NativeWebKeyboardEvent(ui::KeyEvent(
          ui::EventType::kKeyPressed, ui::VKEY_F2, ui::DomCode::F2, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD,
      input::NativeWebKeyboardEvent(ui::KeyEvent(
          ui::EventType::kKeyPressed, ui::VKEY_F3, ui::DomCode::F3, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F4, ui::DomCode::F4, 0))));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(USE_AURA)
  // The input::NativeWebKeyboardEvent constructor is available only when
  // USE_AURA is #defined.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_NEW_WINDOW, input::NativeWebKeyboardEvent(ui::KeyEvent(
                          ui::EventType::kKeyPressed, ui::VKEY_N,
                          ui::DomCode::US_N, ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_CLOSE_TAB, input::NativeWebKeyboardEvent(ui::KeyEvent(
                         ui::EventType::kKeyPressed, ui::VKEY_W,
                         ui::DomCode::US_W, ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FIND, input::NativeWebKeyboardEvent(
                    ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_F,
                                 ui::DomCode::US_F, ui::EF_CONTROL_DOWN))));
#endif  // USE_AURA
}

TEST_F(BrowserWithTestWindowTest, IncognitoCommands) {
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_PERFORMANCE));

  TestingProfile* testprofile = browser()->profile()->AsTestingProfile();
  EXPECT_TRUE(testprofile);
  testprofile->SetGuestSession(true);
  chrome::BrowserCommandController ::
      UpdateSharedCommandsForIncognitoAvailability(
          browser()->command_controller(), testprofile);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_PERFORMANCE));

  testprofile->SetGuestSession(false);
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  chrome::BrowserCommandController ::
      UpdateSharedCommandsForIncognitoAvailability(
          browser()->command_controller(), testprofile);
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_PERFORMANCE));
}

TEST_F(BrowserCommandControllerTest, AppFullScreen) {
  // Enable for tabbed browser.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Enabled for app windows.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "app",
      /*trusted_source=*/true, browser()->window()->GetBounds(), profile(),
      /*user_gesture=*/true);
  params.window = browser()->window();
  set_browser(Browser::Create(params));
  ASSERT_TRUE(browser()->is_type_app());
  browser()->command_controller()->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));
}

TEST_F(BrowserCommandControllerTest, AvatarAcceleratorEnabledOnDesktop) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  TestingProfileManager* testing_profile_manager = profile_manager();
  ProfileManager* profile_manager = testing_profile_manager->profile_manager();
  chrome::BrowserCommandController command_controller(browser());
  const CommandUpdater* command_updater = &command_controller;

  // Chrome OS uses system tray menu to handle multi-profiles.
  bool enabled = !BUILDFLAG(IS_CHROMEOS_ASH);

  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());
  EXPECT_EQ(enabled, command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager->CreateTestingProfile("p2");
  ASSERT_EQ(2u, profile_manager->GetNumberOfProfiles());
  EXPECT_EQ(enabled, command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager->DeleteTestingProfile("p2");
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());
  EXPECT_EQ(enabled, command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
}

TEST_F(BrowserCommandControllerTest, AvatarMenuAlwaysEnabledInIncognitoMode) {
  // Set up a profile with an off the record profile.
  TestingProfile::Builder normal_builder;
  std::unique_ptr<TestingProfile> original_profile = normal_builder.Build();

  // Create a new browser based on the off the record profile.
  Browser::CreateParams profile_params(
      original_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true), true);
  std::unique_ptr<Browser> otr_browser(
      CreateBrowserWithTestWindowForParams(profile_params));

  chrome::BrowserCommandController command_controller(otr_browser.get());
  const CommandUpdater* command_updater = &command_controller;

  // The avatar menu should be enabled.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
  // The command line is reset at the end of every test by the test suite.
}

//////////////////////////////////////////////////////////////////////////////
class BrowserCommandControllerFullscreenTest;

// A test browser window that can toggle fullscreen state.
class FullscreenTestBrowserWindow : public TestBrowserWindow,
                                    ExclusiveAccessContext {
 public:
  FullscreenTestBrowserWindow(
      BrowserCommandControllerFullscreenTest* test_browser)
      : fullscreen_(false),
        toolbar_showing_(false),
        test_browser_(test_browser) {}

  FullscreenTestBrowserWindow(const FullscreenTestBrowserWindow&) = delete;
  FullscreenTestBrowserWindow& operator=(const FullscreenTestBrowserWindow&) =
      delete;

  ~FullscreenTestBrowserWindow() override {}

  // TestBrowserWindow overrides:
  bool ShouldHideUIForFullscreen() const override { return fullscreen_; }
  bool IsFullscreen() const override { return fullscreen_; }
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType type,
                       int64_t display_id) override {
    fullscreen_ = true;
  }
  void ExitFullscreen() override { fullscreen_ = false; }
  bool IsToolbarShowing() const override { return toolbar_showing_; }
  bool IsLocationBarVisible() const override { return true; }

  ExclusiveAccessContext* GetExclusiveAccessContext() override { return this; }

  // Exclusive access interface:
  Profile* GetProfile() override;
  content::WebContents* GetWebContentsForExclusiveAccess() override;
  void UpdateExclusiveAccessBubble(
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback) override {}
  bool IsExclusiveAccessBubbleDisplayed() const override { return false; }
  void OnExclusiveAccessUserInput() override {}
  bool CanUserExitFullscreen() const override { return true; }

  void set_toolbar_showing(bool showing) { toolbar_showing_ = showing; }

 private:
  bool fullscreen_;
  bool toolbar_showing_;
  raw_ptr<BrowserCommandControllerFullscreenTest> test_browser_;
};

// Test that uses FullscreenTestBrowserWindow for its window.
class BrowserCommandControllerFullscreenTest
    : public BrowserWithTestWindowTest {
 public:
  BrowserCommandControllerFullscreenTest() = default;

  BrowserCommandControllerFullscreenTest(
      const BrowserCommandControllerFullscreenTest&) = delete;
  BrowserCommandControllerFullscreenTest& operator=(
      const BrowserCommandControllerFullscreenTest&) = delete;

  ~BrowserCommandControllerFullscreenTest() override = default;

  Browser* GetBrowser() { return BrowserWithTestWindowTest::browser(); }

  // BrowserWithTestWindowTest overrides:
  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    return std::make_unique<FullscreenTestBrowserWindow>(this);
  }
};

Profile* FullscreenTestBrowserWindow::GetProfile() {
  return test_browser_->GetBrowser()->profile();
}

content::WebContents*
FullscreenTestBrowserWindow::GetWebContentsForExclusiveAccess() {
  return test_browser_->GetBrowser()->tab_strip_model()->GetActiveWebContents();
}

TEST_F(BrowserCommandControllerFullscreenTest,
       UpdateCommandsForFullscreenMode) {
  struct {
    int command_id;
    // Whether the command is enabled in tab mode.
    bool enabled_in_tab;
    // Whether the keyboard shortcut is reserved in tab mode.
    bool reserved_in_tab;
    // Whether the command is enabled in fullscreen mode.
    bool enabled_in_fullscreen;
    // Whether the keyboard shortcut is reserved in fullscreen mode.
    bool reserved_in_fullscreen;
  } commands[] = {
    // 1. Most commands are disabled in fullscreen.
    // 2. In fullscreen, only the exit fullscreen commands are reserved. All
    // other shortcuts should be delivered to the web page. See
    // http://crbug.com/680809.

    //         Command ID        |      tab mode      |      fullscreen     |
    //                           | enabled | reserved | enabled  | reserved |
    // clang-format off
    { IDC_OPEN_CURRENT_URL,        true,     false,     false,     false    },
    { IDC_FOCUS_TOOLBAR,           true,     false,     false,     false    },
    { IDC_FOCUS_LOCATION,          true,     false,     false,     false    },
    { IDC_FOCUS_SEARCH,            true,     false,     false,     false    },
    { IDC_FOCUS_MENU_BAR,          true,     false,     false,     false    },
    { IDC_FOCUS_NEXT_PANE,         true,     false,     false,     false    },
    { IDC_FOCUS_PREVIOUS_PANE,     true,     false,     false,     false    },
    { IDC_FOCUS_BOOKMARKS,         true,     false,     false,     false    },
    { IDC_DEVELOPER_MENU,          true,     false,     false,     false    },
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    { IDC_FEEDBACK,                true,     false,     false,     false    },
#endif
    { IDC_OPTIONS,                 true,     false,     false,     false    },
    { IDC_IMPORT_SETTINGS,         true,     false,     false,     false    },
    { IDC_EDIT_SEARCH_ENGINES,     true,     false,     false,     false    },
    { IDC_VIEW_PASSWORDS,          true,     false,     false,     false    },
    { IDC_ABOUT,                   true,     false,     false,     false    },
    { IDC_SHOW_APP_MENU,           true,     false,     false,     false    },
    { IDC_FULLSCREEN,              true,     false,     true,      true     },
    { IDC_CLOSE_TAB,               true,     true,      true,      false    },
    { IDC_CLOSE_WINDOW,            true,     true,      true,      false    },
    { IDC_NEW_INCOGNITO_WINDOW,    true,     true,      true,      false    },
    { IDC_NEW_TAB,                 true,     true,      true,      false    },
    { IDC_NEW_WINDOW,              true,     true,      true,      false    },
    { IDC_SELECT_NEXT_TAB,         true,     true,      true,      false    },
    { IDC_SELECT_PREVIOUS_TAB,     true,     true,      true,      false    },
    { IDC_EXIT,                    true,     true,      true,      true     },
    { IDC_SHOW_AS_TAB,             false,    false,     false,     false    },
    // clang-format on
  };
  const input::NativeWebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kUndefined, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  // Defaults for a tabbed browser.
  for (size_t i = 0; i < std::size(commands); i++) {
    SCOPED_TRACE(commands[i].command_id);
    EXPECT_EQ(chrome::IsCommandEnabled(browser(), commands[i].command_id),
              commands[i].enabled_in_tab);
    EXPECT_EQ(browser()->command_controller()->IsReservedCommandOrKey(
                  commands[i].command_id, key_event),
              commands[i].reserved_in_tab);
  }

  // Simulate going fullscreen.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  browser()->command_controller()->FullscreenStateChanged();

  // By default, in fullscreen mode, the toolbar should be hidden; and all
  // platforms behave similarly.
  EXPECT_FALSE(window()->IsToolbarShowing());
  for (size_t i = 0; i < std::size(commands); i++) {
    SCOPED_TRACE(commands[i].command_id);
    EXPECT_EQ(chrome::IsCommandEnabled(browser(), commands[i].command_id),
              commands[i].enabled_in_fullscreen);
    EXPECT_EQ(browser()->command_controller()->IsReservedCommandOrKey(
                  commands[i].command_id, key_event),
              commands[i].reserved_in_fullscreen);
  }

#if BUILDFLAG(IS_MAC)
  // When the toolbar is showing, commands should be reserved as if the content
  // were in a tab; IDC_FULLSCREEN should also be reserved.
  static_cast<FullscreenTestBrowserWindow*>(window())->set_toolbar_showing(
      true);
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FULLSCREEN, key_event));
  for (size_t i = 0; i < std::size(commands); i++) {
    if (commands[i].command_id != IDC_FULLSCREEN) {
      SCOPED_TRACE(commands[i].command_id);
      EXPECT_EQ(browser()->command_controller()->IsReservedCommandOrKey(
                    commands[i].command_id, key_event),
                commands[i].reserved_in_tab);
    }
  }
  // Return to default state.
  static_cast<FullscreenTestBrowserWindow*>(window())->set_toolbar_showing(
      false);
#endif

  // Exit fullscreen.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  browser()->command_controller()->FullscreenStateChanged();

  for (size_t i = 0; i < std::size(commands); i++) {
    SCOPED_TRACE(commands[i].command_id);
    EXPECT_EQ(chrome::IsCommandEnabled(browser(), commands[i].command_id),
              commands[i].enabled_in_tab);
    EXPECT_EQ(browser()->command_controller()->IsReservedCommandOrKey(
                  commands[i].command_id, key_event),
              commands[i].reserved_in_tab);
  }

  // Guest Profiles disallow some options.
  TestingProfile* testprofile = browser()->profile()->AsTestingProfile();
  EXPECT_TRUE(testprofile);
  testprofile->SetGuestSession(true);

  browser()->command_controller()->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
}

// Ensure that the logic for enabling IDC_OPTIONS is consistent, regardless of
// the order of entering fullscreen and forced incognito modes. See
// http://crbug.com/694331.
TEST_F(BrowserWithTestWindowTest, OptionsConsistency) {
  TestingProfile* profile = browser()->profile()->AsTestingProfile();
  // Setup guest session.
  profile->SetGuestSession(true);
  // Setup forced incognito mode.
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  // Enter fullscreen.
  browser()->command_controller()->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  // Exit fullscreen
  browser()->command_controller()->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  // Reenter incognito mode, this should trigger
  // UpdateSharedCommandsForIncognitoAvailability() again.
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kDisabled);
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
}

TEST_F(BrowserCommandControllerTest,
       SavePageDisabledByDownloadRestrictionsPolicy) {
  chrome::BrowserCommandController command_controller(browser());
  const CommandUpdater* command_updater = &command_controller;

  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SAVE_PAGE));
  profile()->GetPrefs()->SetInteger(prefs::kDownloadRestrictions,
                                    3 /*ALL_FILES*/);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SAVE_PAGE));
}

TEST_F(BrowserCommandControllerTest,
       SavePageDisabledByAllowFileSelectionDialogsPolicy) {
  chrome::BrowserCommandController command_controller(browser());
  const CommandUpdater* command_updater = &command_controller;

  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SAVE_PAGE));
  g_browser_process->local_state()->SetBoolean(
      prefs::kAllowFileSelectionDialogs, false);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SAVE_PAGE));
}

TEST_F(BrowserWithTestWindowTest, ClearBrowsingDataIsEnabledInIncognito) {
  // Set up a profile with an off the record profile.
  std::unique_ptr<TestingProfile> profile1 = TestingProfile::Builder().Build();
  Profile* incognito_profile =
      profile1->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(incognito_profile->GetOriginalProfile(), profile1.get());

  // Create a new browser based on the off the record profile.
  Browser::CreateParams profile_params(incognito_profile, true);
  std::unique_ptr<Browser> incognito_browser =
      CreateBrowserWithTestWindowForParams(profile_params);

  chrome::BrowserCommandController command_controller(incognito_browser.get());
  EXPECT_EQ(true, command_controller.IsCommandEnabled(IDC_CLEAR_BROWSING_DATA));
}

class BrowserCommandControllerWithBookmarksTest
    : public BrowserCommandControllerTest {
 public:
  BrowserCommandControllerWithBookmarksTest() = default;

  // BrowserWithTestWindowTest overrides:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {TestingProfile::TestingFactory{
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory()}};
  }

  void AddTab() {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    browser()->tab_strip_model()->AppendWebContents(std::move(contents),
                                                    /*foreground=*/false);
  }
};

// Adding and removing background tabs should update the bookmark all tab
// command.
TEST_F(BrowserCommandControllerWithBookmarksTest,
       BookmarkAllTabsUpdatesOnTabStripChanges) {
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return BookmarkModelFactory::GetForBrowserContext(profile())->loaded();
  })) << "Timeout waiting for bookmarks to load";

  chrome::BrowserCommandController command_controller(browser());
  EXPECT_FALSE(command_controller.IsCommandEnabled(IDC_BOOKMARK_ALL_TABS));

  AddTab();
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  browser()->tab_strip_model()->ActivateTabAt(/*index=*/0);
  EXPECT_FALSE(command_controller.IsCommandEnabled(IDC_BOOKMARK_ALL_TABS));

  AddTab();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(command_controller.IsCommandEnabled(IDC_BOOKMARK_ALL_TABS));

  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(command_controller.IsCommandEnabled(IDC_BOOKMARK_ALL_TABS));
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
class CreateShortcutBrowserCommandControllerTest
    : public BrowserCommandControllerTest {
 public:
  CreateShortcutBrowserCommandControllerTest() = default;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<const extensions::Extension> CreateAndInstallExtension() {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("ext").Build();
    CHECK(extension);

    // Simulate installing the extension.
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(browser()->profile()));
    extensions::ExtensionService* extension_service =
        extension_system->CreateExtensionService(
            base::CommandLine::ForCurrentProcess(),
            /*install_directory=*/base::FilePath(),
            /*autoupdate_enabled=*/false);
    extension_service->AddExtension(extension.get());

    return extension;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kShortcutsNotApps};
};

TEST_F(CreateShortcutBrowserCommandControllerTest, BrowserNoSiteNotEnabled) {
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));
}

TEST_F(CreateShortcutBrowserCommandControllerTest, DisabledForOTRProfile) {
  // Set up a profile with an off the record profile.
  std::unique_ptr<TestingProfile> profile1 = TestingProfile::Builder().Build();
  Profile* incognito_profile =
      profile1->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(incognito_profile->GetOriginalProfile(), profile1.get());

  // Create a new browser based on the off the record profile.
  Browser::CreateParams profile_params(incognito_profile, true);
  std::unique_ptr<Browser> incognito_browser =
      CreateBrowserWithTestWindowForParams(profile_params);

  EXPECT_FALSE(
      chrome::IsCommandEnabled(incognito_browser.get(), IDC_CREATE_SHORTCUT));
}

TEST_F(CreateShortcutBrowserCommandControllerTest, DisabledForGuestProfile) {
  TestingProfile* test_profile = browser()->profile()->AsTestingProfile();
  EXPECT_TRUE(test_profile);
  test_profile->SetGuestSession(true);

  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));
}

TEST_F(CreateShortcutBrowserCommandControllerTest, DisabledForSystemProfile) {
  TestingProfile* test_profile = browser()->profile()->AsTestingProfile();
  EXPECT_TRUE(test_profile);

  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));
}

TEST_F(CreateShortcutBrowserCommandControllerTest, EnabledValidUrl) {
  AddTab(browser(), GURL("https://example.com"));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));
}

TEST_F(CreateShortcutBrowserCommandControllerTest, InvalidSchemeDisabled) {
  AddTab(browser(), GURL("abc://apps"));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(CreateShortcutBrowserCommandControllerTest,
       ChromeExtensionSchemeEnabled) {
  const char kResource[] = "resource.html";
  scoped_refptr<const extensions::Extension> extension =
      CreateAndInstallExtension();
  AddTab(browser(), extension->GetResourceURL(kResource));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
