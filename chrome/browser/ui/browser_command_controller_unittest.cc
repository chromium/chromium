// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include "base/command_line.h"
#include "base/stl_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
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
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"

typedef BrowserWithTestWindowTest BrowserCommandControllerTest;

TEST_F(BrowserCommandControllerTest, IsReservedCommandOrKey) {
#if defined(OS_CHROMEOS)
  // F1-3 keys are reserved Chrome accelerators on Chrome OS.
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_BACK, content::NativeWebKeyboardEvent(
                    ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_BROWSER_BACK,
                                 ui::DomCode::BROWSER_BACK, 0))));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FORWARD, content::NativeWebKeyboardEvent(ui::KeyEvent(
                       ui::ET_KEY_PRESSED, ui::VKEY_BROWSER_FORWARD,
                       ui::DomCode::BROWSER_FORWARD, 0))));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD, content::NativeWebKeyboardEvent(
                      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_BROWSER_REFRESH,
                                   ui::DomCode::BROWSER_REFRESH, 0))));

  // When there are modifier keys pressed, don't reserve.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD_BYPASSING_CACHE, content::NativeWebKeyboardEvent(ui::KeyEvent(
                                      ui::ET_KEY_PRESSED, ui::VKEY_F3,
                                      ui::DomCode::F3, ui::EF_SHIFT_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD_BYPASSING_CACHE, content::NativeWebKeyboardEvent(ui::KeyEvent(
                                      ui::ET_KEY_PRESSED, ui::VKEY_F3,
                                      ui::DomCode::F3, ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FULLSCREEN, content::NativeWebKeyboardEvent(
                          ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_F4,
                                       ui::DomCode::F4, ui::EF_SHIFT_DOWN))));

  // F4-10 keys are not reserved since they are Ash accelerators.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F4, ui::DomCode::F4, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F5, ui::DomCode::F5, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F6, ui::DomCode::F6, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F7, ui::DomCode::F7, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F8, ui::DomCode::F8, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F9, ui::DomCode::F9, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F10, ui::DomCode::F10, 0))));

  // Shift+Control+Alt+F3 is also an Ash accelerator. Don't reserve it.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F3, ui::DomCode::F3,
              ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN))));
#endif  // OS_CHROMEOS

#if defined(USE_AURA)
  // Ctrl+n, Ctrl+w are reserved while Ctrl+f is not.

  // The content::NativeWebKeyboardEvent constructor is available only when
  // USE_AURA is #defined.
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_NEW_WINDOW, content::NativeWebKeyboardEvent(ui::KeyEvent(
                          ui::ET_KEY_PRESSED, ui::VKEY_N, ui::DomCode::US_N,
                          ui::EF_CONTROL_DOWN))));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_CLOSE_TAB, content::NativeWebKeyboardEvent(ui::KeyEvent(
                         ui::ET_KEY_PRESSED, ui::VKEY_W, ui::DomCode::US_W,
                         ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FIND, content::NativeWebKeyboardEvent(
                    ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_F,
                                 ui::DomCode::US_F, ui::EF_CONTROL_DOWN))));
#endif  // USE_AURA
}

TEST_F(BrowserCommandControllerTest, IsReservedCommandOrKeyIsApp) {
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "app",
      /*trusted_source=*/true, browser()->window()->GetBounds(), profile(),
      /*user_gesture=*/true);
  params.window = browser()->window();
  set_browser(new Browser(params));

  ASSERT_TRUE(browser()->is_type_app());

  // When is_type_app(), no keys are reserved.
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_BACK, content::NativeWebKeyboardEvent(ui::KeyEvent(
                    ui::ET_KEY_PRESSED, ui::VKEY_F1, ui::DomCode::F1, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FORWARD, content::NativeWebKeyboardEvent(ui::KeyEvent(
                       ui::ET_KEY_PRESSED, ui::VKEY_F2, ui::DomCode::F2, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD, content::NativeWebKeyboardEvent(ui::KeyEvent(
                      ui::ET_KEY_PRESSED, ui::VKEY_F3, ui::DomCode::F3, 0))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::ET_KEY_PRESSED, ui::VKEY_F4, ui::DomCode::F4, 0))));
#endif  // OS_CHROMEOS

#if defined(USE_AURA)
  // The content::NativeWebKeyboardEvent constructor is available only when
  // USE_AURA is #defined.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_NEW_WINDOW, content::NativeWebKeyboardEvent(ui::KeyEvent(
                          ui::ET_KEY_PRESSED, ui::VKEY_N, ui::DomCode::US_N,
                          ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_CLOSE_TAB, content::NativeWebKeyboardEvent(ui::KeyEvent(
                         ui::ET_KEY_PRESSED, ui::VKEY_W, ui::DomCode::US_W,
                         ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FIND, content::NativeWebKeyboardEvent(
                    ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_F,
                                 ui::DomCode::US_F, ui::EF_CONTROL_DOWN))));
#endif  // USE_AURA
}

TEST_F(BrowserCommandControllerTest, IncognitoCommands) {
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_SHOW_SIGNIN));

  TestingProfile* testprofile = browser()->profile()->AsTestingProfile();
  EXPECT_TRUE(testprofile);
  testprofile->SetGuestSession(true);
  chrome::BrowserCommandController ::
      UpdateSharedCommandsForIncognitoAvailability(
          browser()->command_controller(), testprofile);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_SIGNIN));

  testprofile->SetGuestSession(false);
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  chrome::BrowserCommandController ::
      UpdateSharedCommandsForIncognitoAvailability(
          browser()->command_controller(), testprofile);
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_SIGNIN));
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
  set_browser(new Browser(params));
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

  bool enabled = true;
  size_t profiles_count = 1U;
#if defined(OS_CHROMEOS)
  // Chrome OS uses system tray menu to handle multi-profiles.
  enabled = false;
  profiles_count = 2U;
#endif

  ASSERT_EQ(profiles_count, profile_manager->GetNumberOfProfiles());
  EXPECT_EQ(enabled, command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager->CreateTestingProfile("p2");
  profiles_count++;
  ASSERT_EQ(profiles_count, profile_manager->GetNumberOfProfiles());
  EXPECT_EQ(enabled, command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager->DeleteTestingProfile("p2");
  profiles_count--;
  ASSERT_EQ(profiles_count, profile_manager->GetNumberOfProfiles());
  EXPECT_EQ(enabled, command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
}

TEST_F(BrowserCommandControllerTest, AvatarMenuAlwaysEnabledInIncognitoMode) {
  // Set up a profile with an off the record profile.
  TestingProfile::Builder normal_builder;
  std::unique_ptr<TestingProfile> original_profile = normal_builder.Build();

  // Create a new browser based on the off the record profile.
  Browser::CreateParams profile_params(
      original_profile->GetOffTheRecordProfile(), true);
  std::unique_ptr<Browser> otr_browser(
      CreateBrowserWithTestWindowForParams(&profile_params));

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

  ~FullscreenTestBrowserWindow() override {}

  // TestBrowserWindow overrides:
  bool ShouldHideUIForFullscreen() const override { return fullscreen_; }
  bool IsFullscreen() const override { return fullscreen_; }
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType type) override {
    fullscreen_ = true;
  }
  void ExitFullscreen() override { fullscreen_ = false; }
  bool IsToolbarShowing() const override { return toolbar_showing_; }

  ExclusiveAccessContext* GetExclusiveAccessContext() override { return this; }

  // Exclusive access interface:
  Profile* GetProfile() override;
  content::WebContents* GetActiveWebContents() override;
  void HideDownloadShelf() override {}
  void UnhideDownloadShelf() override {}
  void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type,
      ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
      bool force_update) override {}
  void OnExclusiveAccessUserInput() override {}
  bool CanUserExitFullscreen() const override { return true; }

  void set_toolbar_showing(bool showing) { toolbar_showing_ = showing; }

 private:
  bool fullscreen_;
  bool toolbar_showing_;
  BrowserCommandControllerFullscreenTest* test_browser_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenTestBrowserWindow);
};

// Test that uses FullscreenTestBrowserWindow for its window.
class BrowserCommandControllerFullscreenTest
    : public BrowserWithTestWindowTest {
 public:
  BrowserCommandControllerFullscreenTest() {}
  ~BrowserCommandControllerFullscreenTest() override {}

  Browser* GetBrowser() { return BrowserWithTestWindowTest::browser(); }

  // BrowserWithTestWindowTest overrides:
  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    return std::make_unique<FullscreenTestBrowserWindow>(this);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCommandControllerFullscreenTest);
};

Profile* FullscreenTestBrowserWindow::GetProfile() {
  return test_browser_->GetBrowser()->profile();
}

content::WebContents* FullscreenTestBrowserWindow::GetActiveWebContents() {
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
    { IDC_SEND_TAB_TO_SELF,        true,     false,     false,     false    },
    { IDC_SEND_TAB_TO_SELF_SINGLE_TARGET,
                                   true,     false,     false,     false    },
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
    { IDC_SHOW_SIGNIN,             true,     false,      true,      false   },
    // clang-format on
  };
  const content::NativeWebKeyboardEvent key_event(
      blink::WebInputEvent::kUndefined, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  // Defaults for a tabbed browser.
  for (size_t i = 0; i < base::size(commands); i++) {
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
  for (size_t i = 0; i < base::size(commands); i++) {
    SCOPED_TRACE(commands[i].command_id);
    EXPECT_EQ(chrome::IsCommandEnabled(browser(), commands[i].command_id),
              commands[i].enabled_in_fullscreen);
    EXPECT_EQ(browser()->command_controller()->IsReservedCommandOrKey(
                  commands[i].command_id, key_event),
              commands[i].reserved_in_fullscreen);
  }

#if defined(OS_MACOSX)
  // When the toolbar is showing, commands should be reserved as if the content
  // were in a tab; IDC_FULLSCREEN should also be reserved.
  static_cast<FullscreenTestBrowserWindow*>(window())->set_toolbar_showing(
      true);
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FULLSCREEN, key_event));
  for (size_t i = 0; i < base::size(commands); i++) {
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

  for (size_t i = 0; i < base::size(commands); i++) {
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
TEST_F(BrowserCommandControllerTest, OptionsConsistency) {
  TestingProfile* profile = browser()->profile()->AsTestingProfile();
  // Setup guest session.
  profile->SetGuestSession(true);
  // Setup forced incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  // Enter fullscreen.
  browser()->command_controller()->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  // Exit fullscreen
  browser()->command_controller()->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  // Reenter incognito mode, this should trigger
  // UpdateSharedCommandsForIncognitoAvailability() again.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
}

TEST_F(BrowserCommandControllerTest, IncognitoModeOnSigninAllowedPrefChange) {
  // Set up a profile with an off the record profile.
  std::unique_ptr<TestingProfile> profile1 = TestingProfile::Builder().Build();
  Profile* profile2 = profile1->GetOffTheRecordProfile();

  EXPECT_EQ(profile2->GetOriginalProfile(), profile1.get());

  // Create a new browser based on the off the record profile.
  Browser::CreateParams profile_params(profile1->GetOffTheRecordProfile(),
                                       true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(&profile_params));

  chrome::BrowserCommandController command_controller(browser2.get());
  const CommandUpdater* command_updater = &command_controller;

  // Check that the SYNC_SETUP command is updated on preference change.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_SIGNIN));
  profile1->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_SIGNIN));
}

TEST_F(BrowserCommandControllerTest, OnSigninAllowedPrefChange) {
  chrome::BrowserCommandController command_controller(browser());
  const CommandUpdater* command_updater = &command_controller;

  // Check that the SYNC_SETUP command is updated on preference change.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_SIGNIN));
  profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_SIGNIN));
}
