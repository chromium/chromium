// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_browsertest.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_bar_visibility_state.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/network_change_notifier.h"
#include "ui/actions/actions.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/dice_tab_helper.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)

#include "ash/constants/ash_switches.h"
#include "ash/wm/window_pin_util.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_controller.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ui/chromeos/locked_state/locked_state_controller.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#include "extensions/buildflags/buildflags.h"
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

namespace chrome {

namespace {

struct FullscreenCommandExpectation {
  int command_id;
  // Whether the command is enabled in tab mode.
  bool enabled_in_tab;
  // Whether the keyboard shortcut is reserved in tab mode.
  bool reserved_in_tab;
  // Whether the command is enabled in fullscreen mode.
  bool enabled_in_fullscreen;
  // Whether the keyboard shortcut is reserved in fullscreen mode.
  bool reserved_in_fullscreen;
};

// TODO(crbug.com/549506876): Fix test on MacOS.
void VerifyFullscreenCommandStates(BrowserWindowInterface* browser) {
  const bool is_guest = browser->GetProfile()->IsGuestSession();
  const auto commands = std::to_array<FullscreenCommandExpectation>({
      // 1. Most commands are disabled in fullscreen.
      // 2. In fullscreen, only the exit fullscreen commands are reserved. All
      // other shortcuts should be delivered to the web page. See
      // http://crbug.com/40501396.

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
    { IDC_REPORT_UNSAFE_SITE,      true,     false,     false,     false    },
#endif
    { IDC_OPTIONS,                 true,     false,     false,     false    },
    { IDC_IMPORT_SETTINGS,         !is_guest,false,     false,     false    },
    { IDC_EDIT_SEARCH_ENGINES,     true,     false,     false,     false    },
    { IDC_VIEW_PASSWORDS,          true,     false,     false,     false    },
    { IDC_ABOUT,                   true,     false,     false,     false    },
    { IDC_SHOW_APP_MENU,           true,     false,     false,     false    },
    { IDC_FULLSCREEN,              true,     false,     true,      true     },
    { IDC_CLOSE_TAB,               true,     true,      true,      false    },
    { IDC_CLOSE_WINDOW,            true,     true,      true,      false    },
    { IDC_NEW_INCOGNITO_WINDOW,    !is_guest,true,      !is_guest, false    },
    { IDC_NEW_TAB,                 true,     true,      true,      false    },
    { IDC_NEW_WINDOW,              true,     true,      true,      false    },
    { IDC_SELECT_NEXT_TAB,         true,     true,      true,      false    },
    { IDC_SELECT_PREVIOUS_TAB,     true,     true,      true,      false    },
    { IDC_CYCLE_TO_NEXT_TAB,       true,     true,      true,      false    },
    { IDC_CYCLE_TO_PREV_TAB,       true,     true,      true,      false    },
    { IDC_EXIT,                    true,     true,      true,      true     },
    { IDC_SHOW_AS_TAB,             false,    false,     false,     false    },
      // clang-format on
  });
  const input::NativeWebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kUndefined, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  // Defaults for a tabbed browser.
  for (const auto& command : commands) {
    SCOPED_TRACE(command.command_id);
    EXPECT_EQ(chrome::IsCommandEnabled(browser, command.command_id),
              command.enabled_in_tab);
    EXPECT_EQ(
        chrome::BrowserCommandController::From(browser)->IsReservedCommandOrKey(
            command.command_id, key_event),
        command.reserved_in_tab);
  }

  // Simulate going fullscreen.
  ui_test_utils::ToggleFullscreenModeAndWait(browser);
  ASSERT_TRUE(browser->GetWindow()->IsFullscreen());

  const bool show_main_ui = !BrowserWindowFullscreenController::From(browser)
                                 ->ShouldHideUIForFullscreen();
  for (const auto& command : commands) {
    SCOPED_TRACE(command.command_id);
    bool expected_enabled = command.enabled_in_fullscreen;
    if (show_main_ui && command.command_id != IDC_FOCUS_MENU_BAR) {
      expected_enabled = command.enabled_in_tab;
    }
    EXPECT_EQ(chrome::IsCommandEnabled(browser, command.command_id),
              expected_enabled);
    EXPECT_EQ(
        chrome::BrowserCommandController::From(browser)->IsReservedCommandOrKey(
            command.command_id, key_event),
        command.reserved_in_fullscreen);
  }

  // Exit fullscreen.
  ui_test_utils::ToggleFullscreenModeAndWait(browser);
  ASSERT_FALSE(browser->GetWindow()->IsFullscreen());

  for (const auto& command : commands) {
    SCOPED_TRACE(command.command_id);
    EXPECT_EQ(chrome::IsCommandEnabled(browser, command.command_id),
              command.enabled_in_tab);
    EXPECT_EQ(
        chrome::BrowserCommandController::From(browser)->IsReservedCommandOrKey(
            command.command_id, key_event),
        command.reserved_in_tab);
  }
}

}  // namespace

class BrowserCommandControllerBrowserTest : public InProcessBrowserTest {
 public:
  BrowserCommandControllerBrowserTest() = default;

  BrowserCommandControllerBrowserTest(
      const BrowserCommandControllerBrowserTest&) = delete;
  BrowserCommandControllerBrowserTest& operator=(
      const BrowserCommandControllerBrowserTest&) = delete;

  ~BrowserCommandControllerBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_CHROMEOS)
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
#endif
  }
};

// Test case for menus that only appear after Chrome Refresh.
class BrowserCommandControllerBrowserTestRefreshOnly
    : public BrowserCommandControllerBrowserTest {
 public:
  BrowserCommandControllerBrowserTestRefreshOnly() = default;
  BrowserCommandControllerBrowserTestRefreshOnly(
      const BrowserCommandControllerBrowserTestRefreshOnly&) = delete;
  BrowserCommandControllerBrowserTestRefreshOnly& operator=(
      const BrowserCommandControllerBrowserTestRefreshOnly&) = delete;

  ~BrowserCommandControllerBrowserTestRefreshOnly() override = default;

 protected:
  void LoadAndWaitForLanguage(std::string_view relative_url) {
    ASSERT_TRUE(embedded_test_server()->Start());

    GURL url = embedded_test_server()->GetURL(relative_url);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    ChromeTranslateClient* chrome_translate_client =
        ChromeTranslateClient::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());

    std::unique_ptr<translate::TranslateWaiter> translate_waiter =
        translate::CreateTranslateWaiter(
            browser()->tab_strip_model()->GetActiveWebContents(),
            translate::TranslateWaiter::WaitEvent::kLanguageDetermined);

    while (
        chrome_translate_client->GetLanguageState().source_language().empty()) {
      translate_waiter->Wait();
    }
    translate::TranslateManager::SetIgnoreMissingKeyForTesting(true);
    net::NetworkChangeNotifier::CreateMockIfNeeded();
    chrome::BrowserCommandController::From(browser())->TabStateChanged();
  }
};
// Test case for actions behind Toolbar Pinning.
using BrowserCommandControllerBrowserTestToolbarPinningOnly =
    BrowserCommandControllerBrowserTestRefreshOnly;

// Verify that showing a constrained window disables find.
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest, DisableFind) {
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));

  // Showing constrained window should disable find.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto delegate = std::make_unique<MockTabModalConfirmDialogDelegate>(
      web_contents, nullptr);
  MockTabModalConfirmDialogDelegate* delegate_ptr = delegate.get();
  TabModalConfirmDialog::Create(std::move(delegate), web_contents);
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FIND));

  // Switching to a new (unblocked) tab should reenable it.
  AddBlankTabAndShow(browser());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));

  // Switching back to the blocked tab should disable it again.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FIND));

  // Closing the constrained window should reenable it.
  delegate_ptr->Cancel();
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       DisableCommandsInSingleTab) {
  EXPECT_FALSE(
      chrome::IsCommandEnabled(browser(), IDC_WINDOW_CLOSE_TABS_TO_RIGHT));
  EXPECT_FALSE(
      chrome::IsCommandEnabled(browser(), IDC_WINDOW_CLOSE_OTHER_TABS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_MOVE_TAB_TO_NEW_WINDOW));

  // Add a new tab.
  auto* tab_strip_model = browser()->tab_strip_model();
  AddBlankTabAndShow(browser());
  ASSERT_EQ(2, tab_strip_model->count());
  ASSERT_EQ(1, tab_strip_model->active_index());
  // Active previous tab.
  tab_strip_model->ActivateTabAt(0);
  ASSERT_EQ(2, tab_strip_model->count());
  ASSERT_EQ(0, tab_strip_model->active_index());

  EXPECT_TRUE(
      chrome::IsCommandEnabled(browser(), IDC_WINDOW_CLOSE_TABS_TO_RIGHT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_WINDOW_CLOSE_OTHER_TABS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_MOVE_TAB_TO_NEW_WINDOW));

  // Close the newly added tab.
  tab_strip_model->CloseWebContentsAt(1, TabCloseTypes::CLOSE_USER_GESTURE);
  ASSERT_EQ(1, tab_strip_model->count());

  EXPECT_FALSE(
      chrome::IsCommandEnabled(browser(), IDC_WINDOW_CLOSE_TABS_TO_RIGHT));
  EXPECT_FALSE(
      chrome::IsCommandEnabled(browser(), IDC_WINDOW_CLOSE_OTHER_TABS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_MOVE_TAB_TO_NEW_WINDOW));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       NewAvatarMenuEnabledInGuestMode) {
  EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());

  BrowserWindowInterface* browser = CreateGuestBrowser();
  EXPECT_TRUE(browser);

  const CommandUpdater* command_updater =
      chrome::BrowserCommandController::From(browser);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
}
#endif

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       IsReservedCommandOrKey) {
#if BUILDFLAG(IS_CHROMEOS)
  // F1-3 keys are reserved Chrome accelerators on Chrome OS.
  EXPECT_TRUE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          IDC_BACK, input::NativeWebKeyboardEvent(ui::KeyEvent(
                        ui::EventType::kKeyPressed, ui::VKEY_BROWSER_BACK,
                        ui::DomCode::BROWSER_BACK, 0))));
  EXPECT_TRUE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          IDC_FORWARD, input::NativeWebKeyboardEvent(ui::KeyEvent(
                           ui::EventType::kKeyPressed, ui::VKEY_BROWSER_FORWARD,
                           ui::DomCode::BROWSER_FORWARD, 0))));
  EXPECT_TRUE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          IDC_RELOAD, input::NativeWebKeyboardEvent(ui::KeyEvent(
                          ui::EventType::kKeyPressed, ui::VKEY_BROWSER_REFRESH,
                          ui::DomCode::BROWSER_REFRESH, 0))));

  // When there are modifier keys pressed, don't reserve.
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          IDC_RELOAD_BYPASSING_CACHE,
          input::NativeWebKeyboardEvent(
              ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_F3,
                           ui::DomCode::F3, ui::EF_SHIFT_DOWN))));
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          IDC_RELOAD_BYPASSING_CACHE,
          input::NativeWebKeyboardEvent(
              ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_F3,
                           ui::DomCode::F3, ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          IDC_FULLSCREEN, input::NativeWebKeyboardEvent(ui::KeyEvent(
                              ui::EventType::kKeyPressed, ui::VKEY_F4,
                              ui::DomCode::F4, ui::EF_SHIFT_DOWN))));

  // F4-10 keys are not reserved since they are Ash accelerators.
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          -1,
          input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F4, ui::DomCode::F4, 0))));
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          -1,
          input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F5, ui::DomCode::F5, 0))));
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          -1,
          input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F6, ui::DomCode::F6, 0))));
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          -1,
          input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F7, ui::DomCode::F7, 0))));
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          -1,
          input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F8, ui::DomCode::F8, 0))));
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          -1,
          input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F9, ui::DomCode::F9, 0))));
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          -1,
          input::NativeWebKeyboardEvent(ui::KeyEvent(
              ui::EventType::kKeyPressed, ui::VKEY_F10, ui::DomCode::F10, 0))));

  // Shift+Control+Alt+F3 is also an Ash accelerator. Don't reserve it.
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          -1, input::NativeWebKeyboardEvent(ui::KeyEvent(
                  ui::EventType::kKeyPressed, ui::VKEY_F3, ui::DomCode::F3,
                  ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN))));
#endif  // BUILDFLAG(IS_CHROMEOS)

#if defined(USE_AURA)
  // Ctrl+n, Ctrl+w are reserved while Ctrl+f is not.

  // The input::NativeWebKeyboardEvent constructor is available only when
  // USE_AURA is #defined.
  EXPECT_TRUE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          IDC_NEW_WINDOW, input::NativeWebKeyboardEvent(ui::KeyEvent(
                              ui::EventType::kKeyPressed, ui::VKEY_N,
                              ui::DomCode::US_N, ui::EF_CONTROL_DOWN))));
  EXPECT_TRUE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          IDC_CLOSE_TAB, input::NativeWebKeyboardEvent(ui::KeyEvent(
                             ui::EventType::kKeyPressed, ui::VKEY_W,
                             ui::DomCode::US_W, ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsReservedCommandOrKey(
          IDC_FIND, input::NativeWebKeyboardEvent(
                        ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_F,
                                     ui::DomCode::US_F, ui::EF_CONTROL_DOWN))));
#endif  // USE_AURA
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       IsReservedCommandOrKeyIsApp) {
  auto params = BrowserWindowCreateParams::CreateForApp(
      "app", /*trusted_source=*/true, gfx::Rect(), browser()->GetProfile(),
      /*user_gesture=*/true);
  BrowserWindowInterface* app_browser = CreateBrowserWindow(std::move(params));

  ASSERT_EQ(app_browser->GetType(), BrowserWindowInterface::Type::TYPE_APP);

  // When GetType() == BrowserWindowInterface::Type::TYPE_APP, no keys are
  // reserved.
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(chrome::BrowserCommandController::From(app_browser)
                   ->IsReservedCommandOrKey(
                       IDC_BACK, input::NativeWebKeyboardEvent(ui::KeyEvent(
                                     ui::EventType::kKeyPressed, ui::VKEY_F1,
                                     ui::DomCode::F1, 0))));
  EXPECT_FALSE(chrome::BrowserCommandController::From(app_browser)
                   ->IsReservedCommandOrKey(
                       IDC_FORWARD, input::NativeWebKeyboardEvent(ui::KeyEvent(
                                        ui::EventType::kKeyPressed, ui::VKEY_F2,
                                        ui::DomCode::F2, 0))));
  EXPECT_FALSE(chrome::BrowserCommandController::From(app_browser)
                   ->IsReservedCommandOrKey(
                       IDC_RELOAD, input::NativeWebKeyboardEvent(ui::KeyEvent(
                                       ui::EventType::kKeyPressed, ui::VKEY_F3,
                                       ui::DomCode::F3, 0))));
  EXPECT_FALSE(chrome::BrowserCommandController::From(app_browser)
                   ->IsReservedCommandOrKey(
                       -1, input::NativeWebKeyboardEvent(
                               ui::KeyEvent(ui::EventType::kKeyPressed,
                                            ui::VKEY_F4, ui::DomCode::F4, 0))));
#endif  // BUILDFLAG(IS_CHROMEOS)

#if defined(USE_AURA)
  // The input::NativeWebKeyboardEvent constructor is available only when
  // USE_AURA is #defined.
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(app_browser)
          ->IsReservedCommandOrKey(
              IDC_NEW_WINDOW, input::NativeWebKeyboardEvent(ui::KeyEvent(
                                  ui::EventType::kKeyPressed, ui::VKEY_N,
                                  ui::DomCode::US_N, ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(
      chrome::BrowserCommandController::From(app_browser)
          ->IsReservedCommandOrKey(
              IDC_CLOSE_TAB, input::NativeWebKeyboardEvent(ui::KeyEvent(
                                 ui::EventType::kKeyPressed, ui::VKEY_W,
                                 ui::DomCode::US_W, ui::EF_CONTROL_DOWN))));
  EXPECT_FALSE(chrome::BrowserCommandController::From(app_browser)
                   ->IsReservedCommandOrKey(
                       IDC_FIND, input::NativeWebKeyboardEvent(ui::KeyEvent(
                                     ui::EventType::kKeyPressed, ui::VKEY_F,
                                     ui::DomCode::US_F, ui::EF_CONTROL_DOWN))));
#endif  // USE_AURA
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest, IncognitoCommands) {
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_PERFORMANCE));

#if !BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, guest mode is tested in
  // BrowserCommandControllerBrowserTestChromeOSGuest.IncognitoCommands.
  BrowserWindowInterface* guest_browser = CreateGuestBrowser();
  EXPECT_TRUE(chrome::IsCommandEnabled(guest_browser, IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(guest_browser, IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(guest_browser, IDC_PERFORMANCE));
#endif

  IncognitoModePrefs::SetAvailability(
      browser()->GetProfile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_PERFORMANCE));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       ClearBrowsingDataIsEnabledInIncognito) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  EXPECT_TRUE(
      chrome::IsCommandEnabled(incognito_browser, IDC_CLEAR_BROWSING_DATA));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest, AppFullScreen) {
  // Enable for tabbed browser.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Enabled for app windows.
  auto params = BrowserWindowCreateParams::CreateForApp(
      "app", /*trusted_source=*/true, gfx::Rect(), browser()->GetProfile(),
      /*user_gesture=*/true);
  BrowserWindowInterface* app_browser = CreateBrowserWindow(std::move(params));
  ASSERT_EQ(app_browser->GetType(), BrowserWindowInterface::Type::TYPE_APP);
  chrome::BrowserCommandController::From(app_browser)->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(app_browser, IDC_FULLSCREEN));
}

// Ensure that the logic for enabling IDC_OPTIONS is consistent in guest mode,
// regardless of the order of entering fullscreen and forced incognito modes.
// See http://crbug.com/40507396.
// On ChromeOS, guest mode is tested in
// BrowserCommandControllerBrowserTestChromeOSGuest.OptionsConsistency.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       OptionsConsistency) {
  BrowserWindowInterface* guest_browser = CreateGuestBrowser();
  ASSERT_TRUE(guest_browser);
  // Setup forced incognito mode.
  IncognitoModePrefs::SetAvailability(
      guest_browser->GetProfile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  EXPECT_TRUE(chrome::IsCommandEnabled(guest_browser, IDC_OPTIONS));
  // Enter fullscreen.
  chrome::BrowserCommandController::From(guest_browser)
      ->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(guest_browser, IDC_OPTIONS));
  // Exit fullscreen
  chrome::BrowserCommandController::From(guest_browser)
      ->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(guest_browser, IDC_OPTIONS));
  // Reenter incognito mode, this should trigger
  // UpdateSharedCommandsForIncognitoAvailability() again.
  IncognitoModePrefs::SetAvailability(
      guest_browser->GetProfile()->GetPrefs(),
      policy::IncognitoModeAvailability::kDisabled);
  IncognitoModePrefs::SetAvailability(
      guest_browser->GetProfile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  EXPECT_TRUE(chrome::IsCommandEnabled(guest_browser, IDC_OPTIONS));
}
#endif

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       AvatarAcceleratorEnabledOnDesktop) {
  const CommandUpdater* command_updater =
      chrome::BrowserCommandController::From(browser());

#if BUILDFLAG(IS_CHROMEOS)
  // Chrome OS uses system tray menu to handle multi-profiles. Avatar menu
  // accelerator is disabled for non-incognito windows.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
#else
  if (!profiles::IsMultipleProfilesEnabled()) {
    return;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Verify accelerator is enabled with 1 profile.
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  // Verify accelerator remains enabled with multiple profiles.
  // (Asynchronous profile deletion from unit tests is omitted as testing with
  // 1 and 2 profiles is sufficient to verify accelerator availability).
  base::FilePath path = profile_manager->GenerateNextProfileDirectoryPath();
  profiles::testing::CreateProfileSync(profile_manager, path);
  ASSERT_EQ(2u, profile_manager->GetNumberOfProfiles());
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
#endif
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       AvatarMenuAlwaysEnabledInIncognitoMode) {
  BrowserWindowInterface* otr_browser = CreateIncognitoBrowser();
  const CommandUpdater* command_updater =
      chrome::BrowserCommandController::From(otr_browser);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
}

#if !BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/549506876): Fix test on MacOS.
#if BUILDFLAG(IS_MAC)
#define MAYBE_UpdateCommandsForFullscreenMode \
  DISABLED_UpdateCommandsForFullscreenMode
#else
#define MAYBE_UpdateCommandsForFullscreenMode UpdateCommandsForFullscreenMode
#endif
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       MAYBE_UpdateCommandsForFullscreenMode) {
  VerifyFullscreenCommandStates(browser());

  // Guest Profiles disallow some options.
  BrowserWindowInterface* guest_browser = CreateGuestBrowser();
  chrome::BrowserCommandController::From(guest_browser)
      ->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(guest_browser, IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(guest_browser, IDC_IMPORT_SETTINGS));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       SavePageDisabledByDownloadRestrictionsPolicy) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  const CommandUpdater* command_updater =
      chrome::BrowserCommandController::From(browser());

  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SAVE_PAGE));
  browser()->GetProfile()->GetPrefs()->SetInteger(
      policy::policy_prefs::kDownloadRestrictions, 3 /*ALL_FILES*/);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SAVE_PAGE));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       SavePageDisabledByAllowFileSelectionDialogsPolicy) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  const CommandUpdater* command_updater =
      chrome::BrowserCommandController::From(browser());

  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SAVE_PAGE));
  g_browser_process->local_state()->SetBoolean(
      prefs::kAllowFileSelectionDialogs, false);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SAVE_PAGE));
}

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/438540029): Rename this test suite to "TrustedPinned" to
// disambiguate between LockedFullscreen (which is for extension API) and OnTask
// locked (which is for class tools).
class BrowserCommandControllerBrowserTestLockedFullscreen
    : public BrowserCommandControllerBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowserCommandControllerBrowserTestLockedFullscreen() {
    if (is_unified_locked_state_controller_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kUseUnifiedLockedStateController);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kUseUnifiedLockedStateController);
    }
  }

 protected:
  const bool is_unified_locked_state_controller_enabled_ = GetParam();
  bool is_locked_for_on_task_ = false;

  void SetLockedForOnTask(bool locked) {
    if (is_unified_locked_state_controller_enabled_) {
      is_locked_for_on_task_ = locked;
    } else {
      ash::boca::OnTaskLockedController::From(browser())
          ->set_locked_for_on_task(locked);
    }
  }

  void SetUpOnMainThread() override {
    BrowserCommandControllerBrowserTest::SetUpOnMainThread();

    // Set up browser for testing / validating page navigation and tab
    // management command states. This mostly involves opening a new tab and
    // ensuring that we are able to navigate back and forward for the test.
    OpenUrlWithDisposition(GURL("chrome://new-tab-page/"),
                           WindowOpenDisposition::NEW_FOREGROUND_TAB);
    OpenUrlWithDisposition(GURL("chrome://version/"),
                           WindowOpenDisposition::CURRENT_TAB);
    OpenUrlWithDisposition(GURL("about:blank"),
                           WindowOpenDisposition::CURRENT_TAB);

    // Go back by one page to ensure the forward command is also available for
    // testing purposes.
    content::TestNavigationObserver navigation_observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    navigation_observer.Wait();
    ASSERT_TRUE(chrome::CanGoBack(browser()));
    ASSERT_TRUE(chrome::CanGoForward(browser()));
  }

  void EnterLockedFullscreen(
      std::optional<chromeos::LockedState> target_state = std::nullopt) {
    auto* command_controller =
        chrome::BrowserCommandController::From(browser());
    if (is_unified_locked_state_controller_enabled_) {
      auto* controller = chromeos::LockedStateController::From(browser());
      ASSERT_TRUE(controller);
      chromeos::LockedState state = target_state.value_or(
          is_locked_for_on_task_ ? chromeos::LockedState::kOnTaskLocked
                                 : chromeos::LockedState::kExtensionLocked);
      controller->Lock(state);
    } else {
      ash::PinWindow(browser()->GetWindow()->GetNativeWindow(),
                     /*trusted=*/true);
      command_controller->LockedFullscreenStateChanged();
    }

    // Update the corresponding command controller state as well as other
    // states so we can verify what commands are enabled.
    command_controller->TabStateChanged();
    command_controller->FullscreenStateChanged();
    command_controller->PrintingStateChanged();
    command_controller->ExtensionStateChanged();
    command_controller->FindBarVisibilityChanged();
    command_controller->UpdateReloadStopState(
        /*is_loading=*/true,
        /*force=*/false);
  }

  void ExitLockedFullscreen() {
    if (is_unified_locked_state_controller_enabled_) {
      auto* controller = chromeos::LockedStateController::From(browser());
      if (controller) {
        controller->Unlock(controller->GetState());
      }
    } else {
      ash::UnpinWindow(browser()->GetWindow()->GetNativeWindow());
      chrome::BrowserCommandController::From(browser())
          ->LockedFullscreenStateChanged();
    }
  }

  CommandUpdater* GetCommandUpdater() {
    return chrome::BrowserCommandController::From(browser())
        ->command_updater_.get();
  }

 private:
  void OpenUrlWithDisposition(GURL url, WindowOpenDisposition disposition) {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, disposition,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(BrowserCommandControllerBrowserTestLockedFullscreen,
                       WhenNotLockedForOnTask) {
  SetLockedForOnTask(false);
  CommandUpdater* const command_updater = GetCommandUpdater();
  auto* const command_controller =
      chrome::BrowserCommandController::From(browser());

  // IDC_EXIT is always enabled in regular mode so it's a perfect candidate for
  // testing.
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_EXIT));
  EnterLockedFullscreen();

  // IDC_EXIT is not enabled in locked fullscreen.
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_EXIT));
  constexpr int kAllowlistedIds[] = {IDC_CUT, IDC_COPY, IDC_PASTE};

  // Go through all the command ids and ensure only allowlisted commands are
  // enabled.
  for (int id : command_updater->GetAllIds()) {
    bool is_command_allowlisted = std::ranges::contains(kAllowlistedIds, id);
    EXPECT_EQ(command_controller->IsCommandEnabled(id), is_command_allowlisted)
        << "Command " << id << " failed to meet enabled state expectation";
  }

  // Exit locked fullscreen and verify IDC_EXIT is enabled again.
  ExitLockedFullscreen();
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_EXIT));
}

IN_PROC_BROWSER_TEST_P(BrowserCommandControllerBrowserTestLockedFullscreen,
                       WhenLockedForOnTask) {
  SetLockedForOnTask(true);
  CommandUpdater* const command_updater = GetCommandUpdater();
  auto* const command_controller =
      chrome::BrowserCommandController::From(browser());

  // IDC_EXIT is always enabled in regular mode so it's a perfect candidate for
  // testing.
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_EXIT));
  EnterLockedFullscreen();

  // IDC_EXIT is not enabled in locked fullscreen.
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_EXIT));

  // NOTE: If new commands are being added, please disable them by default and
  // notify the ChromeOS team by filing a bug under this component --
  // b/?q=componentid:1389107.
  constexpr int kAllowlistedIds[] = {
      IDC_CUT, IDC_COPY, IDC_PASTE,
      // Page navigation commands.
      IDC_BACK, IDC_FORWARD, IDC_RELOAD, IDC_RELOAD_BYPASSING_CACHE,
      IDC_RELOAD_CLEARING_CACHE, IDC_STOP,
      // Tab navigation commands.
      IDC_SELECT_NEXT_TAB, IDC_SELECT_PREVIOUS_TAB, IDC_CYCLE_TO_NEXT_TAB,
      IDC_CYCLE_TO_PREV_TAB, IDC_SELECT_TAB_0, IDC_SELECT_TAB_1,
      IDC_SELECT_TAB_2, IDC_SELECT_TAB_3, IDC_SELECT_TAB_4, IDC_SELECT_TAB_5,
      IDC_SELECT_TAB_6, IDC_SELECT_TAB_7, IDC_SELECT_LAST_TAB,
      // Find content commands.
      IDC_FIND, IDC_FIND_NEXT, IDC_FIND_PREVIOUS, IDC_CLOSE_FIND_OR_STOP};

  std::vector<int> allowlisted_ids(std::begin(kAllowlistedIds),
                                   std::end(kAllowlistedIds));
  const bool use_unified_controller =
      is_unified_locked_state_controller_enabled_;
  if (use_unified_controller) {
    for (int id = IDC_SELECT_TAB_0; id <= IDC_SELECT_TAB_7; ++id) {
      allowlisted_ids.push_back(id);
    }
  }

  // Go through all the command ids and ensure only allowlisted commands are
  // enabled.
  for (int id : command_updater->GetAllIds()) {
    bool is_command_allowlisted = std::ranges::contains(allowlisted_ids, id);
    EXPECT_EQ(command_controller->IsCommandEnabled(id), is_command_allowlisted)
        << "Command " << id << " failed to meet enabled state expectation";
  }

  // Exit locked fullscreen and verify IDC_EXIT is enabled again.
  ExitLockedFullscreen();
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_EXIT));
}

IN_PROC_BROWSER_TEST_P(BrowserCommandControllerBrowserTestLockedFullscreen,
                       WhenLockedForOnTaskPaused) {
  if (!is_unified_locked_state_controller_enabled_) {
    // Paused state is only supported in unified LockedStateController.
    return;
  }
  CommandUpdater* const command_updater = GetCommandUpdater();
  auto* const command_controller =
      chrome::BrowserCommandController::From(browser());

  // IDC_EXIT is always enabled in regular mode so it's a perfect candidate for
  // testing.
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_EXIT));
  EnterLockedFullscreen(chromeos::LockedState::kOnTaskLockedPaused);

  // IDC_EXIT is not enabled in locked fullscreen.
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_EXIT));

  constexpr int kAllowlistedIds[] = {
      IDC_CUT, IDC_COPY, IDC_PASTE,
      // Page navigation commands.
      IDC_BACK, IDC_FORWARD, IDC_RELOAD, IDC_RELOAD_BYPASSING_CACHE,
      IDC_RELOAD_CLEARING_CACHE, IDC_STOP,
      // Tab navigation commands.
      IDC_SELECT_NEXT_TAB, IDC_SELECT_PREVIOUS_TAB, IDC_CYCLE_TO_NEXT_TAB,
      IDC_CYCLE_TO_PREV_TAB, IDC_SELECT_TAB_0, IDC_SELECT_TAB_1,
      IDC_SELECT_TAB_2, IDC_SELECT_TAB_3, IDC_SELECT_TAB_4, IDC_SELECT_TAB_5,
      IDC_SELECT_TAB_6, IDC_SELECT_TAB_7, IDC_SELECT_LAST_TAB,
      // Find content commands.
      IDC_FIND, IDC_FIND_NEXT, IDC_FIND_PREVIOUS, IDC_CLOSE_FIND_OR_STOP};

  std::vector<int> allowlisted_ids(std::begin(kAllowlistedIds),
                                   std::end(kAllowlistedIds));
  for (int id = IDC_SELECT_TAB_0; id <= IDC_SELECT_TAB_7; ++id) {
    allowlisted_ids.push_back(id);
  }

  // Go through all the command ids and ensure only allowlisted commands are
  // enabled.
  for (int id : command_updater->GetAllIds()) {
    bool is_command_allowlisted = std::ranges::contains(allowlisted_ids, id);
    EXPECT_EQ(command_controller->IsCommandEnabled(id), is_command_allowlisted)
        << "Command " << id << " failed to meet enabled state expectation";
  }

  // Exit locked fullscreen and verify IDC_EXIT is enabled again.
  ExitLockedFullscreen();
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_EXIT));
}

IN_PROC_BROWSER_TEST_P(BrowserCommandControllerBrowserTestLockedFullscreen,
                       WhenLockedForOnTaskPrepared) {
  if (!is_unified_locked_state_controller_enabled_) {
    // Prepared state is only supported in unified LockedStateController.
    return;
  }
  auto* controller = chromeos::LockedStateController::From(browser());
  ASSERT_TRUE(controller);
  controller->Lock(chromeos::LockedState::kOnTaskPrepared);
  EXPECT_FALSE(controller->IsLocked());
  EXPECT_TRUE(controller->IsLockedForOnTask());

  auto* const command_controller =
      chrome::BrowserCommandController::From(browser());
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_EXIT));

  controller->Unlock(chromeos::LockedState::kOnTaskLocked);
  EXPECT_FALSE(controller->IsLockedForOnTask());
}

INSTANTIATE_TEST_SUITE_P(All,
                         BrowserCommandControllerBrowserTestLockedFullscreen,
                         testing::Bool());
#endif

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       TestTabRestoreServiceInitialized) {
  // Note: The command should start out as enabled as the default.
  // All the initialization happens before any test code executes,
  // so we can't validate it.

  // The TabRestoreService should get initialized (Loaded)
  // automatically upon launch.
  // Wait for robustness because InProcessBrowserTest::PreRunTestOnMainThread
  // does not flush the task scheduler.
  TabRestoreServiceLoadWaiter waiter(
      TabRestoreServiceFactory::GetForProfile(browser()->GetProfile()));
  waiter.Wait();

  // After initialization, the command should become disabled because there's
  // nothing to restore.
  chrome::BrowserCommandController* commandController =
      chrome::BrowserCommandController::From(browser());
  ASSERT_EQ(false, commandController->IsCommandEnabled(IDC_RESTORE_TAB));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       PRE_TestTabRestoreCommandEnabled) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  content::WebContents* tab_to_close =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsDestroyedWatcher destroyed_watcher(tab_to_close);
  browser()->tab_strip_model()->CloseSelectedTabs();
  destroyed_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       TestTabRestoreCommandEnabled) {
  // The TabRestoreService should get initialized (Loaded)
  // automatically upon launch.
  // Wait for robustness because InProcessBrowserTest::PreRunTestOnMainThread
  // does not flush the task scheduler.
  TabRestoreServiceLoadWaiter waiter(
      TabRestoreServiceFactory::GetForProfile(browser()->GetProfile()));
  waiter.Wait();

  // After initialization, the command should remain enabled because there's
  // one tab to restore.
  chrome::BrowserCommandController* commandController =
      chrome::BrowserCommandController::From(browser());
  ASSERT_EQ(true, commandController->IsCommandEnabled(IDC_RESTORE_TAB));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       OpenDisabledForAppBrowser) {
  auto params = BrowserWindowCreateParams::CreateForApp(
      "abcdefghaghpphfffooibmlghaeopach", /*trusted_source=*/true,
      gfx::Rect(), /* window_bounds */
      browser()->GetProfile(), /*user_gesture=*/true);
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));

  chrome::BrowserCommandController* commandController =
      chrome::BrowserCommandController::From(browser);
  ASSERT_EQ(false, commandController->IsCommandEnabled(IDC_OPEN_FILE));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       NewTabEnabledForAppBrowser) {
  auto params = BrowserWindowCreateParams::CreateForApp(
      "abcdefghaghpphfffooibmlghaeopach", /*trusted_source=*/true,
      gfx::Rect(), /* window_bounds */
      browser()->GetProfile(), /*user_gesture=*/true);
  BrowserWindowInterface* app_browser = CreateBrowserWindow(std::move(params));

  chrome::BrowserCommandController* commandController =
      BrowserCommandController::From(app_browser);
  EXPECT_TRUE(commandController->IsCommandEnabled(IDC_NEW_TAB));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       OpenDisabledForAppPopupBrowser) {
  auto params = BrowserWindowCreateParams::CreateForAppPopup(
      "abcdefghaghpphfffooibmlghaeopach", /*trusted_source=*/true,
      gfx::Rect(), /* window_bounds */
      browser()->GetProfile(), /*user_gesture=*/true);
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));

  chrome::BrowserCommandController* commandController =
      chrome::BrowserCommandController::From(browser);
  ASSERT_EQ(false, commandController->IsCommandEnabled(IDC_OPEN_FILE));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       OpenDisabledForDevToolsBrowser) {
  auto params =
      BrowserWindowCreateParams::CreateForDevTools(browser()->GetProfile());
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));

  chrome::BrowserCommandController* commandController =
      chrome::BrowserCommandController::From(browser);
  ASSERT_EQ(false, commandController->IsCommandEnabled(IDC_OPEN_FILE));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ExecuteProfileMenuCustomizeChrome) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_CUSTOMIZE_CHROME));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(web_contents->GetURL().possibly_invalid_spec(),
            "chrome://settings/manageProfile");
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ExecuteProfileMenuManageGoogleAccount) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  CoreAccountInfo account_info = signin::SetPrimaryAccount(
      identity_manager, "user@example.com", signin::ConsentLevel::kSignin);
  chrome::UpdateCommandEnabled(browser(), IDC_MANAGE_GOOGLE_ACCOUNT, true);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_MANAGE_GOOGLE_ACCOUNT));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ExecuteProfileMenuCloseProfile) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_CLOSE_PROFILE));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_ExecuteShowSyncSettings DISABLED_ExecuteShowSyncSettings
#else
#define MAYBE_ExecuteShowSyncSettings ExecuteShowSyncSettings
#endif
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       MAYBE_ExecuteShowSyncSettings) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_SHOW_SYNC_SETTINGS));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(web_contents->GetURL().possibly_invalid_spec(),
            "chrome://settings/syncSetup");
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ExecuteShowCustomizeChrome) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  EXPECT_TRUE(chrome::ExecuteCommandWithContext(
      browser(), IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL,
      actions::ActionInvocationContext::Builder()
          .SetProperty(
              kSidePanelOpenTriggerKey,
              static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                  SidePanelOpenTrigger::kAppMenu))
          .Build()));
  EXPECT_TRUE(SidePanelUI::From(browser())->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kCustomizeChrome)));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ExecuteShowCustomizeChromeToolbar) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  EXPECT_TRUE(chrome::ExecuteCommandWithContext(
      browser(), IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR,
      actions::ActionInvocationContext::Builder()
          .SetProperty(
              kSidePanelOpenTriggerKey,
              static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                  SidePanelOpenTrigger::kAppMenu))
          .Build()));
  EXPECT_TRUE(SidePanelUI::From(browser())->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kCustomizeChrome)));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ExecuteProfileMenuOpenGuestProfile) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_OPEN_GUEST_PROFILE));
  BrowserWindowInterface* guest_browser = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(guest_browser);
  ASSERT_TRUE(guest_browser->GetProfile()->IsGuestSession());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ExecuteTurnOnSync) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_TURN_ON_SYNC));
}

class BrowserCommandControllerBrowserTestShowSigninWhenPaused
    : public BrowserCommandControllerBrowserTestRefreshOnly,
      public testing::WithParamInterface<bool> {
 public:
  BrowserCommandControllerBrowserTestShowSigninWhenPaused() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserCommandControllerBrowserTestShowSigninWhenPaused,
    testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "ReplaceSyncPromosEnabled"
                        : "ReplaceSyncPromosDisabled";
    });

IN_PROC_BROWSER_TEST_P(BrowserCommandControllerBrowserTestShowSigninWhenPaused,
                       ExecuteShowSigninWhenPaused) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakePrimaryAccountAvailable(
      identity_manager, "user@example.com",
      syncer::IsReplaceSyncPromosWithSignInPromosEnabled()
          ? signin::ConsentLevel::kSignin
          : signin::ConsentLevel::kSync);
  signin::SetRefreshTokenForPrimaryAccount(identity_manager);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          identity_manager->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin)));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_SHOW_SIGNIN_WHEN_PAUSED));
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_SHOW_SIGNIN_WHEN_PAUSED));

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(active_contents);
  ASSERT_TRUE(tab_helper);
  EXPECT_TRUE(tab_helper->IsChromeSigninPage());
  EXPECT_EQ(active_contents->GetVisibleURL().host(),
            GaiaUrls::GetInstance()->gaia_url().host());
  EXPECT_EQ(signin_metrics::AccessPoint::kMenu,
            tab_helper->signin_access_point());
  EXPECT_EQ(signin_metrics::Reason::kReauthentication,
            tab_helper->signin_reason());
#endif
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ExecuteProfileMenuAddNewProfile) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_ADD_NEW_PROFILE));
  profiles::testing::WaitForPickerLoadStop(
      GURL("chrome://profile-picker/new-profile"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ExecuteProfileMenuManageChromeProfiles) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_MANAGE_CHROME_PROFILES));
  profiles::testing::WaitForPickerWidgetCreated();
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

#endif
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ShowTranslateStatusChromePage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = GURL("chrome://new-tab-page/");
  translate::TranslateManager::SetIgnoreMissingKeyForTesting(true);
  net::NetworkChangeNotifier::CreateMockIfNeeded();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  chrome::BrowserCommandController::From(browser())->TabStateChanged();

  EXPECT_FALSE(
      chrome::BrowserCommandController::From(browser())->IsCommandEnabled(
          IDC_SHOW_TRANSLATE));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ShowTranslateStatusEnglishPage) {
  LoadAndWaitForLanguage("/english_page.html");
  EXPECT_TRUE(
      chrome::BrowserCommandController::From(browser())->IsCommandEnabled(
          IDC_SHOW_TRANSLATE));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ShowTranslateStatusFrenchPage) {
  LoadAndWaitForLanguage("/french_page.html");
  EXPECT_TRUE(
      chrome::BrowserCommandController::From(browser())->IsCommandEnabled(
          IDC_SHOW_TRANSLATE));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestRefreshOnly,
                       ExecuteShowTranslateBubble) {
  LoadAndWaitForLanguage("/french_page.html");
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_SHOW_TRANSLATE));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestToolbarPinningOnly,
                       ShowTranslateStatusChromePage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = GURL("chrome://new-tab-page/");
  translate::TranslateManager::SetIgnoreMissingKeyForTesting(true);
  net::NetworkChangeNotifier::CreateMockIfNeeded();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  chrome::BrowserCommandController::From(browser())->TabStateChanged();

  EXPECT_FALSE(actions::ActionManager::GetForTesting()
                   .FindAction(kActionShowTranslate)
                   ->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestToolbarPinningOnly,
                       ShowTranslateStatusEnglishPage) {
  LoadAndWaitForLanguage("/english_page.html");
  EXPECT_TRUE(actions::ActionManager::GetForTesting()
                  .FindAction(kActionShowTranslate)
                  ->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestToolbarPinningOnly,
                       ShowTranslateStatusFrenchPage) {
  LoadAndWaitForLanguage("/french_page.html");
  EXPECT_TRUE(actions::ActionManager::GetForTesting()
                  .FindAction(kActionShowTranslate)
                  ->GetEnabled());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
using CreateShortcutBrowserCommandControllerNavTest =
    BrowserCommandControllerBrowserTest;

IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserCommandControllerNavTest,
                       BrowserNoSiteNotEnabled) {
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));
}

IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserCommandControllerNavTest,
                       DisabledForOTRProfile) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito_browser);
  EXPECT_FALSE(
      chrome::IsCommandEnabled(incognito_browser, IDC_CREATE_SHORTCUT));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL valid_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, valid_url));
  EXPECT_FALSE(
      chrome::IsCommandEnabled(incognito_browser, IDC_CREATE_SHORTCUT));
}

IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserCommandControllerNavTest,
                       DisabledForGuestProfile) {
  BrowserWindowInterface* guest_browser = CreateGuestBrowser();
  ASSERT_TRUE(guest_browser);
  EXPECT_FALSE(chrome::IsCommandEnabled(guest_browser, IDC_CREATE_SHORTCUT));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL valid_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(guest_browser, valid_url));
  EXPECT_FALSE(chrome::IsCommandEnabled(guest_browser, IDC_CREATE_SHORTCUT));
}

IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserCommandControllerNavTest,
                       DisabledForSystemProfile) {
  // System profiles do not have a browser window, so desktop shortcuts cannot
  // be created.
  EXPECT_FALSE(browser()->GetProfile()->IsSystemProfile());
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));
}

IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserCommandControllerNavTest,
                       EnabledValidUrl) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL valid_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), valid_url));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));
}

IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserCommandControllerNavTest,
                       InvalidSchemeDisabled) {
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));
}

IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserCommandControllerNavTest,
                       ErrorUrlDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // This returns a 404 server error, and cannot be unit-tested, since a valid
  // request is not obtained for the navigation entry being committed in
  // unit-tests.
  GURL error_url(embedded_test_server()->GetURL("example.com", "/abcdef/"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), error_url));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_F(CreateShortcutBrowserCommandControllerNavTest,
                       ChromeExtensionSchemeEnabled) {
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(R"(
    {
      "name": "Test Extension",
      "version": "0.1",
      "manifest_version": 3
    }
  )");
  test_dir.WriteFile(FILE_PATH_LITERAL("resource.html"),
                     "<html><body>Test</body></html>");
  extensions::ChromeTestExtensionLoader loader(browser()->GetProfile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("resource.html")));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_CREATE_SHORTCUT));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

// Tests for Your saved info submenu.
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       ExecuteShowContactInfo) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_SHOW_CONTACT_INFO));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(web_contents->GetURL().possibly_invalid_spec(),
            "chrome://settings/contactInfo");
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       ExecuteShowIdentityDocs) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_SHOW_IDENTITY_DOCS));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(web_contents->GetURL().possibly_invalid_spec(),
            "chrome://settings/identityDocs");
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest, ExecuteShowTravel) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_SHOW_TRAVEL));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(web_contents->GetURL().possibly_invalid_spec(),
            "chrome://settings/travel");
}

// Adding and removing background tabs should update the bookmark all tab
// command.
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       BookmarkAllTabsUpdatesOnTabStripChanges) {
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile()));

  chrome::BrowserCommandController* command_controller =
      chrome::BrowserCommandController::From(browser());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_BOOKMARK_ALL_TABS));

  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_BOOKMARK_ALL_TABS));

  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/1,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_BOOKMARK_ALL_TABS));
}

// In browser tests, the BookmarkModel is already loaded during browser startup,
// so this test verifies IDC_BOOKMARK_THIS_TAB is enabled for a loaded model,
// consolidating the former BookmarkTabUpdateWhenBookmarkLoadingCompletes test.
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       BookmarkTabEnabledWhenBookmarkModelIsAlreadyLoaded) {
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile()));

  chrome::BrowserCommandController* command_controller =
      chrome::BrowserCommandController::From(browser());
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_BOOKMARK_THIS_TAB));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       BookmarkBarSubmenuCommandsExecuteCorrectly) {
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile()));

  chrome::BrowserCommandController* command_controller =
      chrome::BrowserCommandController::From(browser());
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_BOOKMARK_BAR_SUBMENU));
  EXPECT_TRUE(command_controller->IsCommandEnabled(
      IDC_BOOKMARK_BAR_SUBMENU_ALWAYS_SHOW));
  EXPECT_TRUE(command_controller->IsCommandEnabled(
      IDC_BOOKMARK_BAR_SUBMENU_ALWAYS_HIDE));
  EXPECT_TRUE(command_controller->IsCommandEnabled(
      IDC_BOOKMARK_BAR_SUBMENU_ONLY_ON_NTP));

  base::UserActionTester user_action_tester;

  // Test executing visibility commands updates the pref correctly.
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "WrenchMenu_Bookmarks_AlwaysShowBookmarkBar"));
  chrome::ExecuteCommand(browser(), IDC_BOOKMARK_BAR_SUBMENU_ALWAYS_SHOW);
  EXPECT_EQ(
      browser()->GetProfile()->GetPrefs()->GetInteger(
          bookmarks::prefs::kBookmarkBarVisibilityState),
      static_cast<int>(bookmarks::BookmarkBarVisibilityState::kAlwaysShow));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "WrenchMenu_Bookmarks_AlwaysShowBookmarkBar"));

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "WrenchMenu_Bookmarks_AlwaysHideBookmarkBar"));
  chrome::ExecuteCommand(browser(), IDC_BOOKMARK_BAR_SUBMENU_ALWAYS_HIDE);
  EXPECT_EQ(
      browser()->GetProfile()->GetPrefs()->GetInteger(
          bookmarks::prefs::kBookmarkBarVisibilityState),
      static_cast<int>(bookmarks::BookmarkBarVisibilityState::kAlwaysHide));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "WrenchMenu_Bookmarks_AlwaysHideBookmarkBar"));

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "WrenchMenu_Bookmarks_OnlyShowBookmarkBarOnNtp"));
  chrome::ExecuteCommand(browser(), IDC_BOOKMARK_BAR_SUBMENU_ONLY_ON_NTP);
  EXPECT_EQ(
      browser()->GetProfile()->GetPrefs()->GetInteger(
          bookmarks::prefs::kBookmarkBarVisibilityState),
      static_cast<int>(bookmarks::BookmarkBarVisibilityState::kOnlyShowOnNtp));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "WrenchMenu_Bookmarks_OnlyShowBookmarkBarOnNtp"));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       GroupAllUngroupedTabsUserMetricActionEmitted) {
  base::UserActionTester user_action_tester;
  chrome::BrowserCommandController* command_controller =
      chrome::BrowserCommandController::From(browser());

  ASSERT_TRUE(command_controller->IsCommandEnabled(IDC_GROUP_UNGROUPED_TABS));

  chrome::ExecuteCommand(browser(), IDC_GROUP_UNGROUPED_TABS);

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("TabGroups_GroupAllUngroupedTabs"));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       GroupAllUngroupedTabsDisabledWhenNoUngroupedTabs) {
  chrome::BrowserCommandController* command_controller =
      chrome::BrowserCommandController::From(browser());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model->SupportsTabGroups());

  // Ensure the service is initialized before making any changes to tab groups.
  tab_groups::TabGroupSyncServiceInitializedObserver observer(
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser()->GetProfile()));
  observer.Wait();

  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_GROUP_UNGROUPED_TABS));

  tab_strip_model->SetTabPinned(0, true);
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_GROUP_UNGROUPED_TABS));

  tab_strip_model->SetTabPinned(0, false);
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_GROUP_UNGROUPED_TABS));

  tab_strip_model->AddToNewGroup({0});
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_GROUP_UNGROUPED_TABS));

  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  tab_strip_model->SetTabPinned(1, true);
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_GROUP_UNGROUPED_TABS));

  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_GROUP_UNGROUPED_TABS));

  tab_strip_model->SetTabPinned(2, true);
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_GROUP_UNGROUPED_TABS));
}

class BrowserCommandControllerBrowserTestGlic
    : public BrowserCommandControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Bypass glic eligibility check.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kGlicDev);
  }

 private:
  glic::GlicTestEnvironment glic_test_environment_;
};

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestGlic,
                       ExecuteGlicTogglePin) {
  PrefService* profile_prefs = browser()->GetProfile()->GetPrefs();
  profile_prefs->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);

  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_GLIC_TOGGLE_PIN));
  EXPECT_TRUE(profile_prefs->GetBoolean(glic::prefs::kGlicPinnedToTabstrip));

  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_GLIC_TOGGLE_PIN));
  EXPECT_FALSE(profile_prefs->GetBoolean(glic::prefs::kGlicPinnedToTabstrip));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestGlic,
                       EnabledInRegularProfile) {
  ASSERT_TRUE(browser()->GetProfile()->IsRegularProfile());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_GLIC_TOGGLE_PIN));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestGlic,
                       DisabledInIncognitoProfile) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  EXPECT_TRUE(incognito_browser->GetProfile()->IsIncognitoProfile());
  EXPECT_FALSE(
      chrome::IsCommandEnabled(incognito_browser, IDC_GLIC_TOGGLE_PIN));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestGlic,
                       DisabledInGuestProfile) {
  BrowserWindowInterface* guest_browser = CreateGuestBrowser();
  EXPECT_TRUE(guest_browser->GetProfile()->IsGuestSession());
  EXPECT_FALSE(chrome::IsCommandEnabled(guest_browser, IDC_GLIC_TOGGLE_PIN));
}
#endif  // !BUILDFLAG(IS_CHROME)

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestGlic,
                       ThreeDotMenuItemEnabledInRegularProfile) {
  ASSERT_TRUE(browser()->GetProfile()->IsRegularProfile());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_GLIC));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestGlic,
                       ExecuteGlicThreeDotMenuItem) {
  // Bypass glic eligibility check.
  PrefService* profile_prefs = browser()->GetProfile()->GetPrefs();
  profile_prefs->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kEnabled));
  // Bypass fre.
  glic::GlicKeyedService::Get(browser()->GetProfile())
      ->enabling()
      .SetCompletedFre(glic::prefs::FreStatus::kCompleted);

  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_OPEN_GLIC));
  ASSERT_TRUE(glic::GlicKeyedServiceFactory::GetGlicKeyedService(
                  browser()->GetProfile())
                  ->instance_coordinator()
                  .IsAnyPanelShowing());
  // Open command is disabled because Glic is now open.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !chrome::IsCommandEnabled(browser(), IDC_OPEN_GLIC); }));
}

#if BUILDFLAG(IS_CHROMEOS)
class BrowserCommandControllerBrowserTestGlicChromeOSGuest
    : public MixinBasedInProcessBrowserTest {
 public:
  BrowserCommandControllerBrowserTestGlicChromeOSGuest() {
    scoped_feature_list_.InitWithFeatures({features::kGlic}, {});
  }

  BrowserCommandControllerBrowserTestGlicChromeOSGuest(
      const BrowserCommandControllerBrowserTestGlicChromeOSGuest&) = delete;
  BrowserCommandControllerBrowserTestGlicChromeOSGuest& operator=(
      const BrowserCommandControllerBrowserTestGlicChromeOSGuest&) = delete;

  ~BrowserCommandControllerBrowserTestGlicChromeOSGuest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    // Bypass glic eligibility check.
    command_line->AppendSwitch(::switches::kGlicDev);
  }

 protected:
  // Use a ChromeOS guest session mixin instead of a guest browser.
  ash::GuestSessionMixin guest_session_mixin_{&mixin_host_};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestGlicChromeOSGuest,
                       DisabledInGuestProfile) {
  EXPECT_TRUE(browser()->GetProfile()->IsGuestSession());
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_GLIC_TOGGLE_PIN));
}

class BrowserCommandControllerBrowserTestChromeOSGuest
    : public MixinBasedInProcessBrowserTest {
 public:
  BrowserCommandControllerBrowserTestChromeOSGuest() = default;
  BrowserCommandControllerBrowserTestChromeOSGuest(
      const BrowserCommandControllerBrowserTestChromeOSGuest&) = delete;
  BrowserCommandControllerBrowserTestChromeOSGuest& operator=(
      const BrowserCommandControllerBrowserTestChromeOSGuest&) = delete;
  ~BrowserCommandControllerBrowserTestChromeOSGuest() override = default;

 private:
  // Use a ChromeOS guest session mixin instead of a guest browser.
  ash::GuestSessionMixin guest_session_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestChromeOSGuest,
                       UpdateCommandsForFullscreenMode) {
  EXPECT_TRUE(browser()->GetProfile()->IsGuestSession());
  VerifyFullscreenCommandStates(browser());
}

// Guest Profiles disallow some options and respond to forced/disabled incognito
// availability.
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestChromeOSGuest,
                       IncognitoCommands) {
  EXPECT_TRUE(browser()->GetProfile()->IsGuestSession());

  // 1. Guest Profiles disallow some options by default.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_PERFORMANCE));

  // 2. Forced incognito mode in guest profile still allows options but
  // disallows others.
  IncognitoModePrefs::SetAvailability(
      browser()->GetProfile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_PERFORMANCE));

  // 3. Disabled incognito mode in guest profile.
  IncognitoModePrefs::SetAvailability(
      browser()->GetProfile()->GetPrefs(),
      policy::IncognitoModeAvailability::kDisabled);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_PERFORMANCE));
}

// Ensure that the logic for enabling IDC_OPTIONS is consistent in guest mode,
// regardless of the order of entering fullscreen and forced incognito modes.
// See http://crbug.com/40507396.
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTestChromeOSGuest,
                       OptionsConsistency) {
  EXPECT_TRUE(browser()->GetProfile()->IsGuestSession());
  // Setup forced incognito mode.
  IncognitoModePrefs::SetAvailability(
      browser()->GetProfile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));

  // Enter fullscreen.
  chrome::BrowserCommandController::From(browser())->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));

  // Exit fullscreen.
  chrome::BrowserCommandController::From(browser())->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));

  // Reenter incognito mode, this should trigger
  // UpdateSharedCommandsForIncognitoAvailability() again.
  IncognitoModePrefs::SetAvailability(
      browser()->GetProfile()->GetPrefs(),
      policy::IncognitoModeAvailability::kDisabled);
  IncognitoModePrefs::SetAvailability(
      browser()->GetProfile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace chrome
