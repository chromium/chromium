// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/debugging_buildflags.h"
#include "base/debug/profiler.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/ui/webui/inspect_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/profiling.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_urls.h"
#include "printing/buildflags/buildflags.h"
#include "ui/actions/actions.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/browser_commands_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "content/public/browser/gpu_data_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"
#include "chrome/browser/ui/browser_commands_chromeos.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/base/ime/text_input_flags.h"
#include "ui/linux/linux_ui.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/shortcuts/desktop_shortcuts_utils.h"
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

using WebExposedIsolationLevel = content::WebExposedIsolationLevel;

namespace chrome {

namespace {

// Ensures that - if we have not popped up an infobar to prompt the user to e.g.
// reload the current page - that the content pane of the browser is refocused.
void AppInfoDialogClosedCallback(SessionID session_id,
                                 views::Widget::ClosedReason closed_reason,
                                 bool reload_prompt) {
  if (reload_prompt)
    return;

  // If the user clicked on something specific or focus was changed, don't
  // override the focus.
  if (closed_reason != views::Widget::ClosedReason::kEscKeyPressed &&
      closed_reason != views::Widget::ClosedReason::kCloseButtonClicked) {
    return;
  }

  // Ensure that the session id we have is still valid. It's possible
  // (though unlikely) that either the browser or session has been pulled
  // out from underneath us.
  Browser* const browser = chrome::FindBrowserWithID(session_id);
  if (!browser)
    return;

  // We want to focus the active web contents, which again, might not be the
  // original web contents (though it should be the vast majority of the time).
  content::WebContents* const active_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (active_contents)
    active_contents->Focus();
}

bool CanOpenFile(Browser* browser) {
  if (browser->is_type_devtools() || browser->is_type_app() ||
      browser->is_type_app_popup()) {
    return false;
  }

  PrefService* local_state = g_browser_process->local_state();
  // May be null in unit tests.
  if (local_state)
    return local_state->GetBoolean(prefs::kAllowFileSelectionDialogs);

  return true;
}

void InvokeAction(actions::ActionId id, actions::ActionItem* scope) {
  actions::ActionManager::Get().FindAction(id, scope)->InvokeAction();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, public:

BrowserCommandController::BrowserCommandController(Browser* browser)
    : browser_(browser), command_updater_(nullptr) {
  browser_->tab_strip_model()->AddObserver(this);
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    local_pref_registrar_.Init(local_state);
    local_pref_registrar_.Add(
        prefs::kAllowFileSelectionDialogs,
        base::BindRepeating(
            &BrowserCommandController::UpdateCommandsForFileSelectionDialogs,
            base::Unretained(this)));
  }

  profile_pref_registrar_.Init(profile()->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kDevToolsAvailability,
      base::BindRepeating(&BrowserCommandController::UpdateCommandsForDevTools,
                          base::Unretained(this)));
  profile_pref_registrar_.Add(
      bookmarks::prefs::kEditBookmarksEnabled,
      base::BindRepeating(
          &BrowserCommandController::UpdateCommandsForBookmarkEditing,
          base::Unretained(this)));
  profile_pref_registrar_.Add(
      bookmarks::prefs::kShowBookmarkBar,
      base::BindRepeating(
          &BrowserCommandController::UpdateCommandsForBookmarkBar,
          base::Unretained(this)));
  profile_pref_registrar_.Add(
      policy::policy_prefs::kIncognitoModeAvailability,
      base::BindRepeating(
          &BrowserCommandController::UpdateCommandsForIncognitoAvailability,
          base::Unretained(this)));
#if BUILDFLAG(ENABLE_PRINTING)
  profile_pref_registrar_.Add(
      prefs::kPrintingEnabled,
      base::BindRepeating(&BrowserCommandController::UpdatePrintingState,
                          base::Unretained(this)));
#endif  // BUILDFLAG(ENABLE_PRINTING)
  profile_pref_registrar_.Add(
      prefs::kDownloadRestrictions,
      base::BindRepeating(&BrowserCommandController::UpdateSaveAsState,
                          base::Unretained(this)));
#if !BUILDFLAG(IS_MAC)
  profile_pref_registrar_.Add(
      prefs::kFullscreenAllowed,
      base::BindRepeating(
          &BrowserCommandController::UpdateCommandsForFullscreenMode,
          base::Unretained(this)));
#endif

  InitCommandState();

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  if (tab_restore_service) {
    tab_restore_service->AddObserver(this);
    if (!tab_restore_service->IsLoaded()) {
      tab_restore_service->LoadTabsFromLastSession();
    }
  }
}

BrowserCommandController::~BrowserCommandController() {
  // TabRestoreService may have been shutdown by the time we get here. Don't
  // trigger creating it.
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfileIfExisting(profile());
  if (tab_restore_service)
    tab_restore_service->RemoveObserver(this);
  profile_pref_registrar_.RemoveAll();
  local_pref_registrar_.RemoveAll();
  browser_->tab_strip_model()->RemoveObserver(this);
}

bool BrowserCommandController::IsReservedCommandOrKey(
    int command_id,
    const input::NativeWebKeyboardEvent& event) {
  // In Apps mode, no keys are reserved.
  if (browser_->is_type_app() || browser_->is_type_app_popup())
    return false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, the top row of keys are mapped to browser actions like
  // back/forward or refresh. We don't want web pages to be able to change the
  // behavior of these keys.  Ash handles F4 and up; this leaves us needing to
  // reserve browser back/forward and refresh here.
  ui::KeyboardCode key_code =
      static_cast<ui::KeyboardCode>(event.windows_key_code);
  if ((key_code == ui::VKEY_BROWSER_BACK && command_id == IDC_BACK) ||
      (key_code == ui::VKEY_BROWSER_FORWARD && command_id == IDC_FORWARD) ||
      (key_code == ui::VKEY_BROWSER_REFRESH && command_id == IDC_RELOAD)) {
    return true;
  }
#endif

  if (window()->IsFullscreen()) {
    // In fullscreen, all commands except for IDC_FULLSCREEN and IDC_EXIT should
    // be delivered to the web page. The intent to implement and ship can be
    // found in http://crbug.com/680809.
    const bool is_exit_fullscreen =
        (command_id == IDC_EXIT || command_id == IDC_FULLSCREEN);
#if BUILDFLAG(IS_MAC)
    // This behavior is different on Mac OS, which has a unique user-initiated
    // full-screen mode. According to the discussion in http://crbug.com/702251,
    // the commands should be reserved for browser-side handling if the browser
    // window's toolbar is visible.
    if (window()->IsToolbarShowing()) {
      if (command_id == IDC_FULLSCREEN)
        return true;
    } else {
      return is_exit_fullscreen;
    }
#else
    return is_exit_fullscreen;
#endif
  }

#if BUILDFLAG(IS_LINUX)
  // If this key was registered by the user as a content editing hotkey, then
  // it is not reserved.
  auto* linux_ui = ui::LinuxUi::instance();
  if (linux_ui && event.os_event &&
      linux_ui->GetTextEditCommandsForEvent(
          *event.os_event, ui::TEXT_INPUT_FLAG_NONE, nullptr)) {
    return false;
  }
#endif

  return command_id == IDC_CLOSE_TAB || command_id == IDC_CLOSE_WINDOW ||
         command_id == IDC_NEW_INCOGNITO_WINDOW || command_id == IDC_NEW_TAB ||
         command_id == IDC_NEW_WINDOW || command_id == IDC_RESTORE_TAB ||
         command_id == IDC_SELECT_NEXT_TAB ||
         command_id == IDC_SELECT_PREVIOUS_TAB || command_id == IDC_EXIT;
}

void BrowserCommandController::TabStateChanged() {
  UpdateCommandsForTabState();
  UpdateCommandsForWebContentsFocus();
}

void BrowserCommandController::ZoomStateChanged() {
  UpdateCommandsForZoomState();
}

void BrowserCommandController::ContentRestrictionsChanged() {
  UpdateCommandsForContentRestrictionState();
}

void BrowserCommandController::FullscreenStateChanged() {
  UpdateCommandsForFullscreenMode();
}

#if BUILDFLAG(IS_CHROMEOS)
void BrowserCommandController::LockedFullscreenStateChanged() {
  UpdateCommandsForLockedFullscreenMode();
}
#endif

void BrowserCommandController::PrintingStateChanged() {
  UpdatePrintingState();
}

void BrowserCommandController::LoadingStateChanged(bool is_loading,
                                                   bool force) {
  UpdateReloadStopState(is_loading, force);
}

void BrowserCommandController::FindBarVisibilityChanged() {
  if (is_locked_fullscreen_)
    return;
  UpdateCloseFindOrStop();
}

void BrowserCommandController::ExtensionStateChanged() {
  // Extensions may disable the bookmark editing commands.
  UpdateCommandsForBookmarkEditing();
}

void BrowserCommandController::TabKeyboardFocusChangedTo(
    std::optional<int> index) {
  UpdateCommandsForTabKeyboardFocus(index);
}

void BrowserCommandController::WebContentsFocusChanged() {
  UpdateCommandsForWebContentsFocus();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, CommandUpdater implementation:

bool BrowserCommandController::SupportsCommand(int id) const {
  return command_updater_.SupportsCommand(id);
}

bool BrowserCommandController::IsCommandEnabled(int id) const {
  return command_updater_.IsCommandEnabled(id);
}

bool BrowserCommandController::ExecuteCommand(int id,
                                              base::TimeTicks time_stamp) {
  return ExecuteCommandWithDisposition(id, WindowOpenDisposition::CURRENT_TAB,
                                       time_stamp);
}

bool BrowserCommandController::ExecuteCommandWithDisposition(
    int id,
    WindowOpenDisposition disposition,
    base::TimeTicks time_stamp) {
  // Doesn't go through the command_updater_ to avoid dealing with having a
  // naming collision for ExecuteCommandWithDisposition (both
  // CommandUpdaterDelegate and CommandUpdater declare this function so we
  // choose to not implement CommandUpdaterDelegate inside this class and
  // therefore command_updater_ doesn't have the delegate set).
  if (!SupportsCommand(id) || !IsCommandEnabled(id)) {
    return false;
  }

  // No commands are enabled if there is not yet any selected tab.
  // TODO(pkasting): It seems like we should not need this, because either
  // most/all commands should not have been enabled yet anyway or the ones that
  // are enabled should be global, or safe themselves against having no selected
  // tab.  However, Ben says he tried removing this before and got lots of
  // crashes, e.g. from Windows sending WM_COMMANDs at random times during
  // window construction.  This probably could use closer examination someday.
  if (browser_->tab_strip_model()->active_index() == TabStripModel::kNoTab)
    return true;

  DCHECK(command_updater_.IsCommandEnabled(id))
      << "Invalid/disabled command " << id;

  // The order of commands in this switch statement must match the function
  // declaration order in browser.h!
  switch (id) {
    // Navigation commands
    case IDC_BACK:
      GoBack(browser_, disposition);
      break;
    case IDC_FORWARD:
      GoForward(browser_, disposition);
      break;
    case IDC_RELOAD:
      Reload(browser_, disposition);
      break;
    case IDC_RELOAD_CLEARING_CACHE:
      ClearCache(browser_);
      [[fallthrough]];
    case IDC_RELOAD_BYPASSING_CACHE:
      ReloadBypassingCache(browser_, disposition);
      break;
    case IDC_HOME:
      Home(browser_, disposition);
      break;
    case IDC_OPEN_CURRENT_URL:
      OpenCurrentURL(browser_);
      break;
    case IDC_STOP:
      Stop(browser_);
      break;
    case IDC_TAB_SEARCH:
      ShowTabSearch(browser_);
      break;
    case IDC_TAB_SEARCH_CLOSE:
      CloseTabSearch(browser_);
      break;

    // Window management commands
    case IDC_NEW_WINDOW:
      NewWindow(browser_);
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      NewIncognitoWindow(profile());
      break;
    case IDC_CLOSE_WINDOW:
      base::RecordAction(base::UserMetricsAction("CloseWindowByKey"));
      CloseWindow(browser_);
      break;
    case IDC_NEW_TAB: {
      NewTab(browser_);
      break;
    }
    case IDC_NEW_TAB_TO_RIGHT: {
      NewTabToRight(browser_);
      break;
    }
    case IDC_CLOSE_TAB:
      base::RecordAction(base::UserMetricsAction("CloseTabByKey"));
      CloseTab(browser_);
      break;
    case IDC_SELECT_NEXT_TAB:
      base::RecordAction(base::UserMetricsAction("Accel_SelectNextTab"));
      SelectNextTab(
          browser_,
          TabStripUserGestureDetails(
              TabStripUserGestureDetails::GestureType::kKeyboard, time_stamp));
      break;
    case IDC_SELECT_PREVIOUS_TAB:
      base::RecordAction(base::UserMetricsAction("Accel_SelectPreviousTab"));
      SelectPreviousTab(
          browser_,
          TabStripUserGestureDetails(
              TabStripUserGestureDetails::GestureType::kKeyboard, time_stamp));
      break;
    case IDC_MOVE_TAB_NEXT:
      MoveTabNext(browser_);
      break;
    case IDC_MOVE_TAB_PREVIOUS:
      MoveTabPrevious(browser_);
      break;
    case IDC_SELECT_TAB_0:
    case IDC_SELECT_TAB_1:
    case IDC_SELECT_TAB_2:
    case IDC_SELECT_TAB_3:
    case IDC_SELECT_TAB_4:
    case IDC_SELECT_TAB_5:
    case IDC_SELECT_TAB_6:
    case IDC_SELECT_TAB_7:
      base::RecordAction(base::UserMetricsAction("Accel_SelectNumberedTab"));
      SelectNumberedTab(
          browser_, id - IDC_SELECT_TAB_0,
          TabStripUserGestureDetails(
              TabStripUserGestureDetails::GestureType::kKeyboard, time_stamp));
      break;
    case IDC_SELECT_LAST_TAB:
      base::RecordAction(base::UserMetricsAction("Accel_SelectNumberedTab"));
      SelectLastTab(
          browser_,
          TabStripUserGestureDetails(
              TabStripUserGestureDetails::GestureType::kKeyboard, time_stamp));
      break;
    case IDC_DUPLICATE_TAB:
      DuplicateTab(browser_);
      break;
    case IDC_RESTORE_TAB:
      RestoreTab(browser_);
      break;
    case IDC_SHOW_AS_TAB:
      ConvertPopupToTabbedBrowser(browser_);
      break;
    case IDC_FULLSCREEN:
      chrome::ToggleFullscreenMode(browser_);
      break;
    case IDC_OPEN_IN_PWA_WINDOW:
      base::RecordAction(base::UserMetricsAction("OpenActiveTabInPwaWindow"));
      web_app::ReparentWebAppForActiveTab(browser_);
      break;
    case IDC_MOVE_TAB_TO_NEW_WINDOW:
      MoveActiveTabToNewWindow(browser_);
      break;
    case IDC_NAME_WINDOW:
      PromptToNameWindow(browser_);
      break;
    case IDC_COMPACT_MODE:
      ToggleCompactMode(browser_);
      break;

#if BUILDFLAG(IS_CHROMEOS)
    case IDC_TOGGLE_MULTITASK_MENU:
      ToggleMultitaskMenu(browser_);
      break;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    case IDC_VISIT_DESKTOP_OF_LRU_USER_2:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_3:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_4:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_5:
      ExecuteVisitDesktopCommand(id, window()->GetNativeWindow());
      break;
#endif

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
    case IDC_MINIMIZE_WINDOW:
      browser_->window()->Minimize();
      break;
    case IDC_MAXIMIZE_WINDOW:
      browser_->window()->Maximize();
      break;
    case IDC_RESTORE_WINDOW:
      browser_->window()->Restore();
      break;
#endif

#if BUILDFLAG(IS_LINUX)
    case IDC_USE_SYSTEM_TITLE_BAR: {
      PrefService* prefs = profile()->GetPrefs();
      prefs->SetBoolean(prefs::kUseCustomChromeFrame,
                        !prefs->GetBoolean(prefs::kUseCustomChromeFrame));
      break;
    }
#endif

#if BUILDFLAG(IS_MAC)
    case IDC_TOGGLE_FULLSCREEN_TOOLBAR:
      chrome::ToggleAlwaysShowToolbarInFullscreen(browser_);
      break;
    case IDC_TOGGLE_JAVASCRIPT_APPLE_EVENTS: {
      chrome::ToggleJavaScriptFromAppleEventsAllowed(browser_);
      break;
    }
#endif
    case IDC_EXIT:
      Exit();
      break;

    // Page-related commands
    case IDC_SAVE_PAGE:
      SavePage(browser_);
      break;
    case IDC_BOOKMARK_THIS_TAB:
      BookmarkCurrentTab(browser_);
      break;
    case IDC_BOOKMARK_ALL_TABS:
      BookmarkAllTabs(browser_);
      break;
    case IDC_VIEW_SOURCE:
      browser_->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame()
          ->ViewSource();
      break;
    case IDC_PRINT:
      Print(browser_);
      break;

#if BUILDFLAG(ENABLE_PRINTING)
    case IDC_BASIC_PRINT:
      base::RecordAction(base::UserMetricsAction("Accel_Advanced_Print"));
      BasicPrint(browser_);
      break;
#endif  // ENABLE_PRINTING
    case IDC_OFFERS_AND_REWARDS_FOR_PAGE:
      ShowOffersAndRewardsForPage(browser_);
      break;
    case IDC_SAVE_CREDIT_CARD_FOR_PAGE:
      SaveCreditCard(browser_);
      break;
    case IDC_SAVE_IBAN_FOR_PAGE:
      SaveIban(browser_);
      break;
    case IDC_AUTOFILL_MANDATORY_REAUTH:
      ShowMandatoryReauthOptInPrompt(browser_);
      break;
    case IDC_MIGRATE_LOCAL_CREDIT_CARD_FOR_PAGE:
      MigrateLocalCards(browser_);
      break;
    case IDC_SAVE_AUTOFILL_ADDRESS:
      SaveAutofillAddress(browser_);
      break;
    case IDC_SHOW_SYNC_SETTINGS:
      chrome::ShowSettingsSubPage(browser_, chrome::kSyncSetupSubPage);
      break;
    case IDC_TURN_ON_SYNC:
      signin_ui_util::EnableSyncFromSingleAccountPromo(
          browser_->profile(), GetAccountInfoFromProfile(browser_->profile()),
          signin_metrics::AccessPoint::ACCESS_POINT_MENU);
      break;
    case IDC_SHOW_SIGNIN_WHEN_PAUSED:
      signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
          browser_->profile(), signin_metrics::AccessPoint::ACCESS_POINT_MENU);
      break;
    case IDC_SHOW_PASSWORD_MANAGER:
      ShowPasswordManager(browser_);
      break;
    case IDC_SAFETY_HUB_SHOW_PASSWORD_CHECKUP:
      ShowPasswordCheck(browser_);
      break;
    case IDC_SHOW_PAYMENT_METHODS:
      ShowPaymentMethods(browser_);
      break;
    case IDC_SHOW_ADDRESSES:
      ShowAddresses(browser_);
      break;
    case IDC_VIRTUAL_CARD_MANUAL_FALLBACK:
      ShowVirtualCardManualFallbackBubble(browser_);
      break;
    case IDC_VIRTUAL_CARD_ENROLL:
      ShowVirtualCardEnrollBubble(browser_);
      break;
    case IDC_ORGANIZE_TABS:
      StartTabOrganizationRequest(browser_);
      break;
    case IDC_DECLUTTER_TABS:
      ShowTabDeclutter(browser_);
      break;
    case IDC_SHOW_TRANSLATE:
      ShowTranslateBubble(browser_);
      break;
    case IDC_MANAGE_PASSWORDS_FOR_PAGE:
      ManagePasswordsForPage(browser_);
      break;
    case IDC_SEND_TAB_TO_SELF:
      SendTabToSelf(browser_);
      break;
    case IDC_QRCODE_GENERATOR:
      GenerateQRCode(browser_);
      break;
    case IDC_SHARING_HUB:
      SharingHub(browser_);
      break;
    case IDC_SHARING_HUB_SCREENSHOT:
      ScreenshotCapture(browser_);
      break;

    // Clipboard commands
    case IDC_CUT:
      InvokeAction(actions::kActionCut,
                   browser_->GetActions()->root_action_item());
      break;
    case IDC_COPY:
      InvokeAction(actions::kActionCopy,
                   browser_->GetActions()->root_action_item());
      break;
    case IDC_PASTE:
      InvokeAction(actions::kActionPaste,
                   browser_->GetActions()->root_action_item());
      break;

    // Find-in-page
    case IDC_FIND:
      Find(browser_);
      break;
    case IDC_FIND_NEXT:
      FindNext(browser_);
      break;
    case IDC_FIND_PREVIOUS:
      FindPrevious(browser_);
      break;
    case IDC_CLOSE_FIND_OR_STOP:
      if (CanCloseFind(browser_))
        CloseFind(browser_);
      else if (IsCommandEnabled(IDC_STOP))
        ExecuteCommand(IDC_STOP);
      break;

    // Zoom
    case IDC_ZOOM_PLUS:
      Zoom(browser_, content::PAGE_ZOOM_IN);
      break;
    case IDC_ZOOM_NORMAL:
      Zoom(browser_, content::PAGE_ZOOM_RESET);
      break;
    case IDC_ZOOM_MINUS:
      Zoom(browser_, content::PAGE_ZOOM_OUT);
      break;

    // Focus various bits of UI
    case IDC_FOCUS_TOOLBAR:
      base::RecordAction(base::UserMetricsAction("Accel_Focus_Toolbar"));
      FocusToolbar(browser_);
      break;
    case IDC_FOCUS_LOCATION:
      if (!window()->IsLocationBarVisible())
        break;
      base::RecordAction(base::UserMetricsAction("Accel_Focus_Location"));
      FocusLocationBar(browser_);
      break;
    case IDC_FOCUS_SEARCH:
      base::RecordAction(base::UserMetricsAction("Accel_Focus_Search"));
      FocusSearch(browser_);
      break;
    case IDC_FOCUS_MENU_BAR:
      FocusAppMenu(browser_);
      break;
    case IDC_FOCUS_BOOKMARKS:
      base::RecordAction(base::UserMetricsAction("Accel_Focus_Bookmarks"));
      FocusBookmarksToolbar(browser_);
      break;
    case IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY:
      FocusInactivePopupForAccessibility(browser_);
      break;
    case IDC_FOCUS_NEXT_PANE:
      FocusNextPane(browser_);
      break;
    case IDC_FOCUS_PREVIOUS_PANE:
      FocusPreviousPane(browser_);
      break;
    case IDC_FOCUS_WEB_CONTENTS_PANE:
      FocusWebContentsPane(browser_);
      break;

    // Show various bits of UI
    case IDC_OPEN_FILE:
      browser_->OpenFile();
      break;
    case IDC_CREATE_SHORTCUT:
      base::RecordAction(base::UserMetricsAction("CreateShortcut"));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      if (base::FeatureList::IsEnabled(features::kShortcutsNotApps)) {
        chrome::CreateDesktopShortcutForActiveWebContents(browser_);
      } else {
        web_app::CreateWebAppFromCurrentWebContents(
            browser_, web_app::WebAppInstallFlow::kCreateShortcut);
      }
#else
      web_app::CreateWebAppFromCurrentWebContents(
          browser_, web_app::WebAppInstallFlow::kCreateShortcut);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      break;
    case IDC_INSTALL_PWA:
      base::RecordAction(base::UserMetricsAction("InstallWebAppFromMenu"));
      web_app::CreateWebAppFromCurrentWebContents(
          browser_, web_app::WebAppInstallFlow::kInstallSite);
      break;
    case IDC_DEV_TOOLS:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::Show(),
                           DevToolsOpenedByAction::kMainMenuOrMainShortcut);
      break;
    case IDC_DEV_TOOLS_CONSOLE:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::ShowConsolePanel(),
                           DevToolsOpenedByAction::kConsoleShortcut);
      break;
    case IDC_DEV_TOOLS_DEVICES:
      InspectUI::InspectDevices(browser_);
      break;
    case IDC_DEV_TOOLS_INSPECT:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::Inspect(),
                           DevToolsOpenedByAction::kInspectorModeShortcut);
      break;
    case IDC_DEV_TOOLS_TOGGLE:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::Toggle(),
                           DevToolsOpenedByAction::kToggleShortcut);
      break;
    case IDC_TASK_MANAGER:
      OpenTaskManager(browser_);
      break;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case IDC_TAKE_SCREENSHOT:
      TakeScreenshot();
      break;
#endif
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case IDC_FEEDBACK:
      OpenFeedbackDialog(browser_, feedback::kFeedbackSourceBrowserCommand);
      break;
    case IDC_SHOW_SEARCH_COMPANION:
      browser_->GetFeatures().side_panel_ui()->Show(
          SidePanelEntryId::kSearchCompanion, SidePanelOpenTrigger::kAppMenu);
      break;
#endif
    case IDC_SHOW_CHROME_LABS:
      window()->ShowChromeLabs();
      break;
    case IDC_SHOW_BOOKMARK_BAR:
      ToggleBookmarkBar(browser_);
      break;
    case IDC_SHOW_FULL_URLS:
      ToggleShowFullURLs(browser_);
      break;
    case IDC_SHOW_GOOGLE_LENS_SHORTCUT:
      ToggleShowGoogleLensShortcut(browser_);
      break;
    case IDC_PROFILING_ENABLED:
      content::Profiling::Toggle();
      break;
    case IDC_CARET_BROWSING_TOGGLE:
      ToggleCaretBrowsing(browser_);
      break;
    case IDC_RECENT_TABS_LOGIN_FOR_DEVICE_TABS:
      ShowSettingsSubPage(browser_->GetBrowserForOpeningWebUi(),
                          chrome::kPeopleSubPage);
      break;
    case IDC_SHOW_BOOKMARK_MANAGER:
      ShowBookmarkManager(browser_->GetBrowserForOpeningWebUi());
      break;
    case IDC_SHOW_BOOKMARK_SIDE_PANEL:
      browser_->GetFeatures().side_panel_ui()->Show(
          SidePanelEntryId::kBookmarks, SidePanelOpenTrigger::kAppMenu);
      break;
    case IDC_SHOW_APP_MENU:
      base::RecordAction(base::UserMetricsAction("Accel_Show_App_Menu"));
      ShowAppMenu(browser_);
      break;
    case IDC_SHOW_AVATAR_MENU:
      ShowAvatarMenu(browser_);
      break;
    case IDC_SHOW_HISTORY:
      ShowHistory(browser_->GetBrowserForOpeningWebUi());
      break;
    case IDC_SHOW_HISTORY_CLUSTERS_SIDE_PANEL:
      browser_->GetFeatures().side_panel_ui()->Show(
          SidePanelEntryId::kHistoryClusters, SidePanelOpenTrigger::kAppMenu);
      break;
    case IDC_SHOW_DOWNLOADS:
      ShowDownloads(browser_->GetBrowserForOpeningWebUi());
      break;
    case IDC_MANAGE_EXTENSIONS:
    case IDC_SAFETY_HUB_MANAGE_EXTENSIONS:
      ShowExtensions(browser_->GetBrowserForOpeningWebUi());
      break;
    case IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS:
      CHECK(features::IsExtensionMenuInRootAppMenu());
      ShowExtensions(browser_->GetBrowserForOpeningWebUi());
      break;
    case IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE:
      CHECK(features::IsExtensionMenuInRootAppMenu());
      ShowWebStore(browser_, extension_urls::kAppMenuUtmSource);
      break;
    case IDC_PERFORMANCE:
      ShowSettingsSubPage(browser_->GetBrowserForOpeningWebUi(),
                          chrome::kPerformanceSubPage);
      break;
    case IDC_OPTIONS:
      ShowSettings(browser_->GetBrowserForOpeningWebUi());
      break;
    case IDC_EDIT_SEARCH_ENGINES:
      ShowSearchEngineSettings(browser_->GetBrowserForOpeningWebUi());
      break;
    case IDC_VIEW_PASSWORDS:
      NavigateToManagePasswordsPage(
          browser_->GetBrowserForOpeningWebUi(),
          password_manager::ManagePasswordsReferrer::kChromeMenuItem);
      break;
    case IDC_CLEAR_BROWSING_DATA: {
      if (profile()->IsIncognitoProfile()) {
        ShowIncognitoClearBrowsingDataDialog(
            browser_->GetBrowserForOpeningWebUi());
      } else {
        ShowClearBrowsingDataDialog(browser_->GetBrowserForOpeningWebUi());
      }
      break;
    }
    case IDC_IMPORT_SETTINGS:
      ShowImportDialog(browser_);
      break;
    case IDC_TOGGLE_REQUEST_TABLET_SITE:
      ToggleRequestTabletSite(browser_);
      break;
    case IDC_ABOUT:
      ShowAboutChrome(browser_->GetBrowserForOpeningWebUi());
      break;
    case IDC_UPGRADE_DIALOG:
      OpenUpdateChromeDialog(browser_);
      break;
    case IDC_OPEN_SAFETY_HUB:
      ShowSettingsSubPage(browser_->GetBrowserForOpeningWebUi(),
                          chrome::kSafetyHubSubPage);
      break;
    case IDC_HELP_PAGE_VIA_KEYBOARD:
      ShowHelp(browser_, HELP_SOURCE_KEYBOARD);
      break;
    case IDC_HELP_PAGE_VIA_MENU:
      ShowHelp(browser_, HELP_SOURCE_MENU);
      break;
    case IDC_CHROME_TIPS:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      ShowChromeTips(browser_);
#else
      NOTREACHED_IN_MIGRATION();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      break;
    case IDC_CHROME_WHATS_NEW:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
      ShowChromeWhatsNew(browser_);
#else
      NOTREACHED_IN_MIGRATION();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
        // (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
      break;
    case IDC_SHOW_BETA_FORUM:
      ShowBetaForum(browser_);
      break;
    case IDC_ROUTE_MEDIA:
      RouteMediaInvokedFromAppMenu(browser_);
      break;
    case IDC_WINDOW_MUTE_SITE:
      MuteSite(browser_);
      break;
    case IDC_WINDOW_PIN_TAB:
      PinTab(browser_);
      break;
    case IDC_WINDOW_GROUP_TAB:
      GroupTab(browser_);
      break;
    case IDC_CREATE_NEW_TAB_GROUP:
      CreateNewTabGroup(browser_);
      break;
    case IDC_WINDOW_CLOSE_TABS_TO_RIGHT:
      CloseTabsToRight(browser_);
      break;
    case IDC_WINDOW_CLOSE_OTHER_TABS:
      CloseOtherTabs(browser_);
      break;
    case IDC_SHOW_MANAGEMENT_PAGE: {
      ShowSingletonTab(browser_, GetManagedUiUrl(profile()));
      break;
    }
    case IDC_MUTE_TARGET_SITE:
      MuteSiteForKeyboardFocusedTab(browser_);
      break;
    case IDC_PIN_TARGET_TAB:
      PinKeyboardFocusedTab(browser_);
      break;
    case IDC_GROUP_TARGET_TAB:
      GroupKeyboardFocusedTab(browser_);
      break;
    case IDC_DUPLICATE_TARGET_TAB:
      DuplicateKeyboardFocusedTab(browser_);
      break;
    // Hosted App commands
    case IDC_COPY_URL:
      CopyURL(browser_, browser_->tab_strip_model()->GetActiveWebContents());
      break;
    case IDC_OPEN_IN_CHROME:
      OpenInChrome(browser_);
      break;
    case IDC_WEB_APP_SETTINGS:
#if !BUILDFLAG(IS_CHROMEOS)
      CHECK(browser_->app_controller());
      ShowWebAppSettings(browser_, browser_->app_controller()->app_id(),
                         web_app::AppSettingsPageEntryPoint::kBrowserCommand);
#endif
      break;
    case IDC_WEB_APP_MENU_APP_INFO: {
      content::WebContents* const web_contents =
          browser_->tab_strip_model()->GetActiveWebContents();
      if (web_contents) {
        ShowPageInfoDialog(
            web_contents,
            base::BindOnce(&AppInfoDialogClosedCallback,
                           sessions::SessionTabHelper::IdForWindowContainingTab(
                               web_contents)),
            bubble_anchor_util::kAppMenuButton);
      }
      break;
    }

    // UI debug commands
    case IDC_DEBUG_TOGGLE_TABLET_MODE:
    case IDC_DEBUG_PRINT_VIEW_TREE:
    case IDC_DEBUG_PRINT_VIEW_TREE_DETAILS:
      ExecuteUIDebugCommand(id, browser_);
      break;

    case IDC_CONTENT_CONTEXT_LENS_OVERLAY:
      ExecLensOverlay(browser_);
      break;

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
    case IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH:
      ExecLensRegionSearch(browser_);
      break;
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
    case IDC_READING_LIST_MENU_ADD_TAB:
      chrome::MoveCurrentTabToReadLater(browser_);
      break;

    case IDC_READING_LIST_MENU_SHOW_UI:
      browser_->GetFeatures().side_panel_ui()->Show(
          SidePanelEntryId::kReadingList, SidePanelOpenTrigger::kAppMenu);
      break;

    case IDC_SHOW_READING_MODE_SIDE_PANEL: {
      // Yes. This is a separate feature from the reading list.
      browser_->GetFeatures().side_panel_ui()->Show(
          SidePanelEntryId::kReadAnything, SidePanelOpenTrigger::kAppMenu);
      break;
    }

    case IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL: {
      ShowCustomizeChromeSidePanel(CustomizeChromeSection::kAppearance);
      break;
    }

    case IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR: {
      ShowCustomizeChromeSidePanel(CustomizeChromeSection::kToolbar);
      break;
    }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // Profile submenu commands
    // This menu item is not enabled on ChromeOS and certain capabilities such
    // as the profile picker are not available.
    case IDC_CUSTOMIZE_CHROME:
      chrome::ShowSettingsSubPage(browser_, chrome::kManageProfileSubPage);
      break;
    case IDC_CLOSE_PROFILE: {
      if (browser_->profile()->IsIncognitoProfile()) {
        BrowserList::CloseAllBrowsersWithIncognitoProfile(
            browser_->profile(), base::DoNothing(), base::DoNothing(), true);
      } else {
        profiles::CloseProfileWindows(browser_->profile());
      }
      break;
    }
    case IDC_MANAGE_GOOGLE_ACCOUNT: {
      Profile* profile = browser_->profile();
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile);
      DCHECK(
          identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
      NavigateToGoogleAccountPage(
          profile,
          identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
              .email);
      break;
    }
    case IDC_OPEN_GUEST_PROFILE:
      profiles::SwitchToGuestProfile();
      break;
    case IDC_ADD_NEW_PROFILE:
      ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
          ProfilePicker::EntryPoint::kAppMenuProfileSubMenuAddNewProfile));
      break;
    case IDC_MANAGE_CHROME_PROFILES:
      ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
          ProfilePicker::EntryPoint::kAppMenuProfileSubMenuManageProfiles));
      break;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
    case IDC_SET_BROWSER_AS_DEFAULT:
      base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
          ->StartSetAsDefault(base::DoNothing());

      // Log metrics before clearing prefs and closing prompts.
      if (g_browser_process->local_state()->HasPrefPath(
              prefs::kDefaultBrowserFirstShownTime)) {
        base::UmaHistogramCounts100(
            "DefaultBrowser.AppMenu.TimesShownBeforeAccept",
            g_browser_process->local_state()->GetInteger(
                prefs::kDefaultBrowserDeclinedCount) +
                1);
        base::UmaHistogramCustomTimes(
            "DefaultBrowser.AppMenu.TimeToSetDefault",
            base::Time::Now() - g_browser_process->local_state()->GetTime(
                                    prefs::kDefaultBrowserFirstShownTime),
            base::Milliseconds(1), base::Days(7), 50);
      }
      chrome::startup::default_prompt::UpdatePrefsForDismissedPrompt(
          browser_->profile());
      DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
          DefaultBrowserPromptManager::CloseReason::kAccept);
      break;
#endif
    default:
      LOG(WARNING) << "Received Unimplemented Command: " << id;
      break;
  }

  return true;
}

void BrowserCommandController::AddCommandObserver(int id,
                                                  CommandObserver* observer) {
  command_updater_.AddCommandObserver(id, observer);
}

void BrowserCommandController::RemoveCommandObserver(
    int id,
    CommandObserver* observer) {
  command_updater_.RemoveCommandObserver(id, observer);
}

void BrowserCommandController::RemoveCommandObserver(
    CommandObserver* observer) {
  command_updater_.RemoveCommandObserver(observer);
}

bool BrowserCommandController::UpdateCommandEnabled(int id, bool state) {
  if (is_locked_fullscreen_)
    return false;

  return command_updater_.UpdateCommandEnabled(id, state);
}

// BrowserCommandController, TabStripModelObserver implementation:

void BrowserCommandController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  UpdateCommandsForTabStripStateChanged();
}

void BrowserCommandController::TabBlockedStateChanged(
    content::WebContents* contents,
    int index) {
  PrintingStateChanged();
  FullscreenStateChanged();
  UpdateCommandsForFind();
  UpdateCommandsForMediaRouter();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, TabRestoreServiceObserver implementation:

void BrowserCommandController::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  UpdateTabRestoreCommandState();
}

void BrowserCommandController::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  service->RemoveObserver(this);
}

void BrowserCommandController::TabRestoreServiceLoaded(
    sessions::TabRestoreService* service) {
  UpdateTabRestoreCommandState();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, private:

bool BrowserCommandController::IsShowingMainUI() {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
}

bool BrowserCommandController::IsShowingLocationBar() {
  return browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
}

void BrowserCommandController::InitCommandState() {
  // All browser commands whose state isn't set automagically some other way
  // (like Back & Forward with initial page load) must have their state
  // initialized here, otherwise they will be forever disabled.

  if (is_locked_fullscreen_)
    return;

  // Navigation commands
  const bool can_reload = CanReload(browser_);
  command_updater_.UpdateCommandEnabled(IDC_RELOAD, can_reload);
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_BYPASSING_CACHE, can_reload);
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_CLEARING_CACHE, can_reload);

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_WINDOW, true);
  command_updater_.UpdateCommandEnabled(
      IDC_NEW_TAB, !browser_->app_controller() ||
                       !browser_->app_controller()->ShouldHideNewTabButton());
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_TAB, true);
  command_updater_.UpdateCommandEnabled(
      IDC_DUPLICATE_TAB, !browser_->is_type_picture_in_picture());
  UpdateTabRestoreCommandState();
  command_updater_.UpdateCommandEnabled(IDC_EXIT, true);
  command_updater_.UpdateCommandEnabled(IDC_NAME_WINDOW, true);
  if (base::FeatureList::IsEnabled(features::kCompactMode)) {
    command_updater_.UpdateCommandEnabled(IDC_COMPACT_MODE, true);
  }

  command_updater_.UpdateCommandEnabled(IDC_ORGANIZE_TABS, true);
  command_updater_.UpdateCommandEnabled(IDC_CREATE_NEW_TAB_GROUP, true);
  command_updater_.UpdateCommandEnabled(IDC_DECLUTTER_TABS, true);
#if BUILDFLAG(IS_CHROMEOS)
  command_updater_.UpdateCommandEnabled(IDC_TOGGLE_MULTITASK_MENU, true);
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  command_updater_.UpdateCommandEnabled(IDC_MINIMIZE_WINDOW, true);
  // The VisitDesktop command is only supported for up to 5 logged in users
  // because that's the max number of user sessions. If that number is increased
  // the IDC_VISIT_DESKTOP_OF_LRU_USER_ command ids should be updated as well.
  // crbug.com/940461
  static_assert(
      session_manager::kMaximumNumberOfUserSessions <=
          IDC_VISIT_DESKTOP_OF_LRU_USER_LAST -
              IDC_VISIT_DESKTOP_OF_LRU_USER_NEXT + 2,
      "The max number of user sessions exceeds the number of users supported.");
  command_updater_.UpdateCommandEnabled(IDC_VISIT_DESKTOP_OF_LRU_USER_2, true);
  command_updater_.UpdateCommandEnabled(IDC_VISIT_DESKTOP_OF_LRU_USER_3, true);
  command_updater_.UpdateCommandEnabled(IDC_VISIT_DESKTOP_OF_LRU_USER_4, true);
  command_updater_.UpdateCommandEnabled(IDC_VISIT_DESKTOP_OF_LRU_USER_5, true);
#endif
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  command_updater_.UpdateCommandEnabled(IDC_MINIMIZE_WINDOW, true);
  command_updater_.UpdateCommandEnabled(IDC_MAXIMIZE_WINDOW, true);
  command_updater_.UpdateCommandEnabled(IDC_RESTORE_WINDOW, true);
#endif
#if BUILDFLAG(IS_LINUX)
  bool use_system_title_bar = true;
#if BUILDFLAG(IS_OZONE)
  use_system_title_bar = ui::OzonePlatform::GetInstance()
                             ->GetPlatformRuntimeProperties()
                             .supports_server_side_window_decorations;
#endif
  command_updater_.UpdateCommandEnabled(IDC_USE_SYSTEM_TITLE_BAR,
                                        use_system_title_bar);
#endif
  command_updater_.UpdateCommandEnabled(IDC_OPEN_IN_PWA_WINDOW,
                                        web_app::CanPopOutWebApp(profile()));

  // Page-related commands
  command_updater_.UpdateCommandEnabled(IDC_MANAGE_PASSWORDS_FOR_PAGE, true);

  // Zoom
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_PLUS, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_NORMAL, false);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MINUS, true);

  // Show various bits of UI
  DCHECK(!profile()->IsSystemProfile())
      << "Ought to never have browser for the system profile.";
  const bool normal_window = browser_->is_type_normal();
  const bool guest_session = profile()->IsGuestSession();
  command_updater_.UpdateCommandEnabled(IDC_OPEN_FILE, CanOpenFile(browser_));
  UpdateCommandsForDevTools();
  command_updater_.UpdateCommandEnabled(IDC_TASK_MANAGER, CanOpenTaskManager());
  command_updater_.UpdateCommandEnabled(IDC_PROFILE_MENU_IN_APP_MENU, true);
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_HISTORY, (!guest_session && !profile()->IsSystemProfile()));
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_HISTORY_CLUSTERS_SIDE_PANEL,
      (!guest_session && !profile()->IsSystemProfile()));
  command_updater_.UpdateCommandEnabled(IDC_SHOW_DOWNLOADS, true);
  command_updater_.UpdateCommandEnabled(IDC_FIND_AND_EDIT_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_SAVE_AND_SHARE_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_READING_MODE_SIDE_PANEL, true);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL,
                                        true);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR,
                                        true);
  command_updater_.UpdateCommandEnabled(IDC_SEND_TAB_TO_SELF, false);
  command_updater_.UpdateCommandEnabled(IDC_QRCODE_GENERATOR, false);
  command_updater_.UpdateCommandEnabled(IDC_PASSWORDS_AND_AUTOFILL_MENU,
                                        !guest_session);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_PASSWORD_MANAGER,
                                        !guest_session);
  command_updater_.UpdateCommandEnabled(IDC_SAFETY_HUB_SHOW_PASSWORD_CHECKUP,
                                        !guest_session);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_PAYMENT_METHODS,
                                        !guest_session);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_SYNC_SETTINGS, true);
  command_updater_.UpdateCommandEnabled(IDC_TURN_ON_SYNC, true);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_SIGNIN_WHEN_PAUSED, true);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_ADDRESSES, !guest_session);
  command_updater_.UpdateCommandEnabled(IDC_HELP_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_HELP_PAGE_VIA_KEYBOARD, true);
  command_updater_.UpdateCommandEnabled(IDC_HELP_PAGE_VIA_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_BETA_FORUM, true);
  command_updater_.UpdateCommandEnabled(
      IDC_BOOKMARKS_MENU, (!guest_session && !profile()->IsSystemProfile()));
  command_updater_.UpdateCommandEnabled(IDC_SAVED_TAB_GROUPS_MENU, true);
  command_updater_.UpdateCommandEnabled(
      IDC_RECENT_TABS_MENU, (!guest_session && !profile()->IsSystemProfile() &&
                             !profile()->IsIncognitoProfile()));
  command_updater_.UpdateCommandEnabled(
      IDC_RECENT_TABS_LOGIN_FOR_DEVICE_TABS,
      (!guest_session && !profile()->IsSystemProfile() &&
       !profile()->IsIncognitoProfile()));
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  command_updater_.UpdateCommandEnabled(IDC_CUSTOMIZE_CHROME, true);
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_PROFILE, true);
  command_updater_.UpdateCommandEnabled(IDC_MANAGE_GOOGLE_ACCOUNT, true);
  command_updater_.UpdateCommandEnabled(IDC_OPEN_GUEST_PROFILE, true);
  command_updater_.UpdateCommandEnabled(IDC_ADD_NEW_PROFILE, true);
  command_updater_.UpdateCommandEnabled(IDC_MANAGE_CHROME_PROFILES, true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (profile()->IsIncognitoProfile()) {
    command_updater_.UpdateCommandEnabled(IDC_CLEAR_BROWSING_DATA, true);
  } else {
    command_updater_.UpdateCommandEnabled(
        IDC_CLEAR_BROWSING_DATA,
        (!guest_session && !profile()->IsSystemProfile()));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  command_updater_.UpdateCommandEnabled(IDC_TAKE_SCREENSHOT, true);
  // Chrome OS uses the system tray menu to handle multi-profiles. Avatar menu
  // is only required in incognito mode.
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_AVATAR_MENU, /*state=*/profile()->IsIncognitoProfile());
#else
  command_updater_.UpdateCommandEnabled(IDC_SHOW_AVATAR_MENU,
                                        /*state=*/normal_window);
#endif
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_SAVE_LOCAL_CARD_SIGN_IN_PROMO_IF_APPLICABLE, true);
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_SIGN_IN_PROMO, true);
  command_updater_.UpdateCommandEnabled(IDC_CARET_BROWSING_TOGGLE, true);
  // Navigation commands
  command_updater_.UpdateCommandEnabled(
      IDC_HOME, normal_window || browser_->is_type_app() ||
                    browser_->is_type_app_popup());

  // Hosted app browser commands.
  const bool is_web_app_or_custom_tab = IsWebAppOrCustomTab(browser_);
  const bool enable_copy_url =
      is_web_app_or_custom_tab ||
      !sharing_hub::SharingIsDisabledByPolicy(browser_->profile());
  command_updater_.UpdateCommandEnabled(IDC_COPY_URL, enable_copy_url);
  command_updater_.UpdateCommandEnabled(IDC_WEB_APP_SETTINGS,
                                        is_web_app_or_custom_tab);
  command_updater_.UpdateCommandEnabled(IDC_WEB_APP_MENU_APP_INFO,
                                        is_web_app_or_custom_tab);

  // Tab management commands
  const bool supports_tabs =
      browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_NEXT_TAB, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_PREVIOUS_TAB, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_MOVE_TAB_NEXT, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_MOVE_TAB_PREVIOUS, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_0, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_1, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_2, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_3, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_4, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_5, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_6, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_7, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_LAST_TAB, supports_tabs);
  command_updater_.UpdateCommandEnabled(IDC_NEW_TAB_TO_RIGHT, supports_tabs);

  // These are always enabled; the menu determines their menu item visibility.
  command_updater_.UpdateCommandEnabled(IDC_UPGRADE_DIALOG, true);
  command_updater_.UpdateCommandEnabled(IDC_SET_BROWSER_AS_DEFAULT, true);

  // Safety Hub commands.
  command_updater_.UpdateCommandEnabled(
      IDC_OPEN_SAFETY_HUB, base::FeatureList::IsEnabled(features::kSafetyHub));

  command_updater_.UpdateCommandEnabled(IDC_WINDOW_MUTE_SITE, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_WINDOW_PIN_TAB, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_WINDOW_GROUP_TAB, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_WINDOW_CLOSE_TABS_TO_RIGHT,
                                        normal_window);
  command_updater_.UpdateCommandEnabled(IDC_WINDOW_CLOSE_OTHER_TABS,
                                        normal_window);

  const bool enable_tab_search_commands = browser_->is_type_normal();
  command_updater_.UpdateCommandEnabled(IDC_TAB_SEARCH,
                                        enable_tab_search_commands);
  command_updater_.UpdateCommandEnabled(IDC_TAB_SEARCH_CLOSE,
                                        enable_tab_search_commands);

  if (base::FeatureList::IsEnabled(features::kUIDebugTools)) {
    command_updater_.UpdateCommandEnabled(IDC_DEBUG_TOGGLE_TABLET_MODE, true);
    command_updater_.UpdateCommandEnabled(IDC_DEBUG_PRINT_VIEW_TREE, true);
    command_updater_.UpdateCommandEnabled(IDC_DEBUG_PRINT_VIEW_TREE_DETAILS,
                                          true);
  }

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  if (base::FeatureList::IsEnabled(
          lens::features::kEnableRegionSearchKeyboardShortcut)) {
    command_updater_.UpdateCommandEnabled(
        IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH, true);
  }
#endif

  command_updater_.UpdateCommandEnabled(IDC_SHOW_BOOKMARK_SIDE_PANEL, true);

  if (browser_->is_type_normal()) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    command_updater_.UpdateCommandEnabled(IDC_SHOW_SEARCH_COMPANION, true);
#endif
    // Reading list commands.
    command_updater_.UpdateCommandEnabled(IDC_READING_LIST_MENU, true);
    command_updater_.UpdateCommandEnabled(IDC_READING_LIST_MENU_ADD_TAB, true);
    command_updater_.UpdateCommandEnabled(IDC_READING_LIST_MENU_SHOW_UI, true);
  }
  if (IsChromeLabsEnabled()) {
    command_updater_.UpdateCommandEnabled(IDC_SHOW_CHROME_LABS, true);
  }

  // Initialize other commands whose state changes based on various conditions.
  UpdateCommandsForFullscreenMode();
  UpdateCommandsForContentRestrictionState();
  UpdateCommandsForBookmarkEditing();
  UpdateCommandsForIncognitoAvailability();
  UpdateCommandsForExtensionsMenu();
  UpdateCommandsForTabKeyboardFocus(GetKeyboardFocusedTabIndex(browser_));
  UpdateCommandsForWebContentsFocus();
}

// static
void BrowserCommandController::UpdateSharedCommandsForIncognitoAvailability(
    CommandUpdater* command_updater,
    Profile* profile) {
  policy::IncognitoModeAvailability incognito_availability =
      IncognitoModePrefs::GetAvailability(profile->GetPrefs());
  command_updater->UpdateCommandEnabled(
      IDC_NEW_WINDOW,
      incognito_availability != policy::IncognitoModeAvailability::kForced);
  command_updater->UpdateCommandEnabled(
      IDC_NEW_INCOGNITO_WINDOW,
      incognito_availability != policy::IncognitoModeAvailability::kDisabled &&
          !profile->IsGuestSession());

  const bool forced_incognito =
      incognito_availability == policy::IncognitoModeAvailability::kForced;
  const bool is_guest = profile->IsGuestSession();

  command_updater->UpdateCommandEnabled(
      IDC_SHOW_BOOKMARK_MANAGER,
      browser_defaults::bookmarks_enabled && !forced_incognito && !is_guest);
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const bool enable_extensions =
      extension_service && extension_service->extensions_enabled();

  command_updater->UpdateCommandEnabled(IDC_SHOW_FULL_URLS, true);
  command_updater->UpdateCommandEnabled(IDC_SHOW_GOOGLE_LENS_SHORTCUT, true);

  // Bookmark manager and settings page/subpages are forced to open in normal
  // mode. For this reason we disable these commands when incognito is forced.
  command_updater->UpdateCommandEnabled(
      IDC_MANAGE_EXTENSIONS,
      enable_extensions && !forced_incognito && !is_guest);
  command_updater->UpdateCommandEnabled(
      IDC_SAFETY_HUB_MANAGE_EXTENSIONS,
      enable_extensions && !forced_incognito && !is_guest);

  command_updater->UpdateCommandEnabled(IDC_IMPORT_SETTINGS,
                                        !forced_incognito && !is_guest);
  command_updater->UpdateCommandEnabled(IDC_OPTIONS,
                                        !forced_incognito || is_guest);
  command_updater->UpdateCommandEnabled(IDC_PERFORMANCE,
                                        !forced_incognito && !is_guest);
}

void BrowserCommandController::UpdateCommandsForIncognitoAvailability() {
  if (is_locked_fullscreen_)
    return;

  UpdateSharedCommandsForIncognitoAvailability(&command_updater_, profile());

  if (!IsShowingMainUI()) {
    command_updater_.UpdateCommandEnabled(IDC_IMPORT_SETTINGS, false);
    command_updater_.UpdateCommandEnabled(IDC_OPTIONS, false);
  }
}

void BrowserCommandController::UpdateCommandsForExtensionsMenu() {
  // TODO(crbug.com/41124423): Talk with isandrk@chromium.org about whether this
  // is necessary for the experiment or not.
  if (is_locked_fullscreen_) {
    return;
  }

  if (features::IsExtensionMenuInRootAppMenu()) {
    command_updater_.UpdateCommandEnabled(
        IDC_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS,
        /*state=*/true);
    command_updater_.UpdateCommandEnabled(
        IDC_EXTENSIONS_SUBMENU_VISIT_CHROME_WEB_STORE, /*state=*/true);
  }
}

void BrowserCommandController::UpdateCommandsForTabState() {
  // Keep commands disabled when in locked fullscreen so users cannot exit this
  // mode. Only update navigation ones when the webapp is locked for OnTask
  // (only relevant for non-web browser scenarios).
  // TODO(b/365146870): Remove once we consolidate locked fullscreen with
  // OnTask.
  bool skip_all_command_updates = is_locked_fullscreen_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (browser_->IsLockedForOnTask()) {
    skip_all_command_updates = false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  if (skip_all_command_updates) {
    return;
  }

  content::WebContents* current_web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!current_web_contents)  // May be NULL during tab restore.
    return;

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_BACK, CanGoBack(browser_));
  command_updater_.UpdateCommandEnabled(IDC_FORWARD, CanGoForward(browser_));
  const bool can_reload = CanReload(browser_);
  command_updater_.UpdateCommandEnabled(IDC_RELOAD, can_reload);
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_BYPASSING_CACHE, can_reload);
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_CLEARING_CACHE, can_reload);
  if (is_locked_fullscreen_) {
    // Skip other command updates.
    // NOTE: If new commands are being added, please add them after this
    // conditional and notify the ChromeOS team by filing a bug under this
    // component -- b/?q=componentid:1389107.
    return;
  }

  // Window management commands
  bool is_app = browser_->is_type_app() || browser_->is_type_app_popup();
  bool is_normal = browser_->is_type_normal();

  command_updater_.UpdateCommandEnabled(IDC_DUPLICATE_TAB,
                                        !is_app && CanDuplicateTab(browser_));
  command_updater_.UpdateCommandEnabled(IDC_WINDOW_MUTE_SITE, !is_app);
  command_updater_.UpdateCommandEnabled(IDC_WINDOW_PIN_TAB, is_normal);
  command_updater_.UpdateCommandEnabled(IDC_WINDOW_GROUP_TAB, is_normal);

  // Page-related commands
  window()->SetStarredState(
      BookmarkTabHelper::FromWebContents(current_web_contents)->is_starred());
  window()->ZoomChangedForActiveTab(false);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_SOURCE,
                                        CanViewSource(browser_));

  command_updater_.UpdateCommandEnabled(IDC_OPEN_FILE, CanOpenFile(browser_));

  bool can_create_web_app = web_app::CanCreateWebApp(browser_);
  command_updater_.UpdateCommandEnabled(IDC_INSTALL_PWA, can_create_web_app);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(features::kShortcutsNotApps)) {
    command_updater_.UpdateCommandEnabled(
        IDC_CREATE_SHORTCUT, shortcuts::CanCreateDesktopShortcut(browser_));
  } else {
    command_updater_.UpdateCommandEnabled(IDC_CREATE_SHORTCUT,
                                          can_create_web_app);
  }
#else
  command_updater_.UpdateCommandEnabled(IDC_CREATE_SHORTCUT,
                                        can_create_web_app);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

  UpdateCommandAndActionEnabled(IDC_SEND_TAB_TO_SELF, kActionSendTabToSelf,
                                CanSendTabToSelf(browser_));

  UpdateCommandAndActionEnabled(IDC_QRCODE_GENERATOR, kActionQrCodeGenerator,
                                CanGenerateQrCode(browser_));

  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(current_web_contents);
  const bool can_translate =
      chrome_translate_client &&
      chrome_translate_client->GetTranslateManager()->CanManuallyTranslate();
  UpdateCommandAndActionEnabled(IDC_SHOW_TRANSLATE, kActionShowTranslate,
                                can_translate);

  bool is_isolated_app = browser_->app_controller() &&
                         browser_->app_controller()->IsIsolatedWebApp();
  bool is_pinned_home_tab = web_app::IsPinnedHomeTab(
      browser_->tab_strip_model(), browser_->tab_strip_model()->active_index());
  command_updater_.UpdateCommandEnabled(
      IDC_OPEN_IN_CHROME,
      IsWebAppOrCustomTab(browser_) && !is_isolated_app && !is_pinned_home_tab);

  command_updater_.UpdateCommandEnabled(
      IDC_TOGGLE_REQUEST_TABLET_SITE,
      CanRequestTabletSite(current_web_contents));

  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_SEARCH_COMPANION,
      companion::IsCompanionAvailableForCurrentActiveTab(browser_));

  UpdateCommandsForContentRestrictionState();
  UpdateCommandsForBookmarkEditing();
  UpdateCommandsForFind();
  UpdateCommandsForMediaRouter();
  // Update the zoom commands when an active tab is selected.
  UpdateCommandsForZoomState();
  UpdateCommandsForTabKeyboardFocus(GetKeyboardFocusedTabIndex(browser_));
}

void BrowserCommandController::UpdateCommandsForZoomState() {
  content::WebContents* contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!contents)
    return;
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_PLUS, CanZoomIn(contents));
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_NORMAL,
                                        CanResetZoom(contents));
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MINUS, CanZoomOut(contents));
}

void BrowserCommandController::UpdateCommandsForContentRestrictionState() {
  int restrictions = GetContentRestrictions(browser_);

  command_updater_.UpdateCommandEnabled(
      IDC_COPY, !(restrictions & CONTENT_RESTRICTION_COPY));
  command_updater_.UpdateCommandEnabled(
      IDC_CUT, !(restrictions & CONTENT_RESTRICTION_CUT));
  command_updater_.UpdateCommandEnabled(
      IDC_PASTE, !(restrictions & CONTENT_RESTRICTION_PASTE));
  UpdateSaveAsState();
  UpdatePrintingState();
}

void BrowserCommandController::UpdateCommandsForDevTools() {
  if (is_locked_fullscreen_)
    return;

  bool dev_tools_enabled = DevToolsWindow::AllowDevToolsFor(
      profile(), browser_->tab_strip_model()->GetActiveWebContents());
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS, dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_CONSOLE,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_DEVICES,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_INSPECT,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_TOGGLE,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_SOURCE, dev_tools_enabled);
#if BUILDFLAG(IS_MAC)
  command_updater_.UpdateCommandEnabled(IDC_TOGGLE_JAVASCRIPT_APPLE_EVENTS,
                                        dev_tools_enabled);
#endif
}

void BrowserCommandController::UpdateCommandsForBookmarkEditing() {
  if (is_locked_fullscreen_)
    return;

  command_updater_.UpdateCommandEnabled(IDC_BOOKMARK_THIS_TAB,
                                        CanBookmarkCurrentTab(browser_));
  command_updater_.UpdateCommandEnabled(IDC_BOOKMARK_ALL_TABS,
                                        CanBookmarkAllTabs(browser_));
}

void BrowserCommandController::UpdateCommandsForBookmarkBar() {
  if (is_locked_fullscreen_)
    return;

  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_BOOKMARK_BAR, browser_defaults::bookmarks_enabled &&
                                 !profile()->IsGuestSession() &&
                                 !profile()->IsSystemProfile() &&
                                 !profile()->GetPrefs()->IsManagedPreference(
                                     bookmarks::prefs::kShowBookmarkBar) &&
                                 IsShowingMainUI());
}

void BrowserCommandController::UpdateCommandsForFileSelectionDialogs() {
  if (is_locked_fullscreen_)
    return;

  UpdateSaveAsState();
  command_updater_.UpdateCommandEnabled(IDC_OPEN_FILE, CanOpenFile(browser_));
}

void BrowserCommandController::UpdateCommandsForFullscreenMode() {
  if (is_locked_fullscreen_)
    return;

  const bool is_fullscreen = window() && window()->IsFullscreen();
  const bool show_main_ui = IsShowingMainUI();
  const bool show_location_bar = IsShowingLocationBar();

  const bool main_not_fullscreen = show_main_ui && !is_fullscreen;

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_OPEN_CURRENT_URL, show_main_ui);

  // Window management commands
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_AS_TAB, !browser_->is_type_normal() && !is_fullscreen &&
                           !browser_->is_type_devtools() &&
                           !browser_->is_type_picture_in_picture());

  // Focus various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_TOOLBAR, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_LOCATION, show_location_bar);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_SEARCH, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_MENU_BAR,
                                        main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_NEXT_PANE,
                                        main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_PREVIOUS_PANE,
                                        main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_WEB_CONTENTS_PANE,
                                        main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_BOOKMARKS,
                                        main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY, main_not_fullscreen);

  // Show various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_DEVELOPER_MENU, show_main_ui);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  command_updater_.UpdateCommandEnabled(
      IDC_FEEDBACK, show_main_ui || browser_->is_type_devtools());
#endif

  command_updater_.UpdateCommandEnabled(IDC_EDIT_SEARCH_ENGINES, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_PASSWORDS, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_ABOUT, show_main_ui);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  command_updater_.UpdateCommandEnabled(IDC_CHROME_TIPS, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_CHROME_WHATS_NEW, show_main_ui);
#endif
  command_updater_.UpdateCommandEnabled(IDC_CONTENT_CONTEXT_SHARING_SUBMENU,
                                        show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SHARING_HUB, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SHARING_HUB_SCREENSHOT,
                                        show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_APP_MENU, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_MANAGEMENT_PAGE, true);

  if (base::debug::IsProfilingSupported())
    command_updater_.UpdateCommandEnabled(IDC_PROFILING_ENABLED, show_main_ui);

#if !BUILDFLAG(IS_MAC)
  // Disable toggling into fullscreen mode if disallowed by pref.
  const bool fullscreen_enabled =
      is_fullscreen ||
      profile()->GetPrefs()->GetBoolean(prefs::kFullscreenAllowed);
#else
  const bool fullscreen_enabled = true;
#endif

  command_updater_.UpdateCommandEnabled(IDC_FULLSCREEN, fullscreen_enabled);
  command_updater_.UpdateCommandEnabled(IDC_TOGGLE_FULLSCREEN_TOOLBAR,
                                        fullscreen_enabled);

  UpdateCommandsForBookmarkBar();
  UpdateCommandsForIncognitoAvailability();
  UpdateCommandsForHostedAppAvailability();
}

void BrowserCommandController::UpdateCommandsForHostedAppAvailability() {
  bool has_toolbar = browser_->is_type_normal() ||
                     web_app::AppBrowserController::IsWebApp(browser_);
  if (window() && window()->ShouldHideUIForFullscreen())
    has_toolbar = false;
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_TOOLBAR, has_toolbar);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_NEXT_PANE, has_toolbar);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_PREVIOUS_PANE, has_toolbar);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_APP_MENU, has_toolbar);
}

#if BUILDFLAG(IS_CHROMEOS)
namespace {

#if DCHECK_IS_ON()
// Makes sure that all commands that are not allowlisted are disabled. DCHECKs
// otherwise. Compiled only in debug mode.
void NonAllowlistedCommandsAreDisabled(CommandUpdaterImpl* command_updater) {
  constexpr int kAllowlistedIds[] = {IDC_CUT, IDC_COPY, IDC_PASTE};

  // Go through all the command ids, skip the allowlisted ones.
  for (int id : command_updater->GetAllIds()) {
    if (base::Contains(kAllowlistedIds, id)) {
      continue;
    }
    DCHECK(!command_updater->IsCommandEnabled(id));
  }
}
#endif

}  // namespace

void BrowserCommandController::UpdateCommandsForLockedFullscreenMode() {
  bool is_locked_fullscreen =
      platform_util::IsBrowserLockedFullscreen(browser_);
  // Sanity check to make sure this function is called only on state change.
  DCHECK_NE(is_locked_fullscreen, is_locked_fullscreen_);
  if (is_locked_fullscreen == is_locked_fullscreen_)
    return;
  is_locked_fullscreen_ = is_locked_fullscreen;

  if (is_locked_fullscreen_) {
    command_updater_.DisableAllCommands();
    // Update the state of allowlisted commands:
    // IDC_CUT/IDC_COPY/IDC_PASTE,
    UpdateCommandsForContentRestrictionState();
    // TODO(crbug.com/41426009): Re-enable Find and Zoom in locked fullscreen.
    // All other commands will be disabled (there is an early return in their
    // corresponding UpdateCommandsFor* functions).
#if DCHECK_IS_ON()
    NonAllowlistedCommandsAreDisabled(&command_updater_);
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Enable commands that allow users to switch between tabs if the webapp is
    // locked for OnTask (only relevant for non-web browser scenarios).
    if (browser_->IsLockedForOnTask()) {
      bool supports_tabs =
          browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
      command_updater_.UpdateCommandEnabled(IDC_SELECT_NEXT_TAB, supports_tabs);
      command_updater_.UpdateCommandEnabled(IDC_SELECT_PREVIOUS_TAB,
                                            supports_tabs);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else {
    // Do an init call to re-initialize command state after the
    // DisableAllCommands.
    InitCommandState();
  }
}
#endif

void BrowserCommandController::UpdatePrintingState() {
  if (is_locked_fullscreen_)
    return;

  UpdateCommandAndActionEnabled(IDC_PRINT, kActionPrint, CanPrint(browser_));
#if BUILDFLAG(ENABLE_PRINTING)
  command_updater_.UpdateCommandEnabled(IDC_BASIC_PRINT,
                                        CanBasicPrint(browser_));
#endif
}

void BrowserCommandController::UpdateSaveAsState() {
  if (is_locked_fullscreen_)
    return;

  command_updater_.UpdateCommandEnabled(IDC_SAVE_PAGE, CanSavePage(browser_));
}

void BrowserCommandController::UpdateReloadStopState(bool is_loading,
                                                     bool force) {
  if (is_locked_fullscreen_)
    return;

  window()->UpdateReloadStopState(is_loading, force);
  command_updater_.UpdateCommandEnabled(IDC_STOP, is_loading);
  UpdateCloseFindOrStop();
}

void BrowserCommandController::UpdateTabRestoreCommandState() {
  if (is_locked_fullscreen_)
    return;

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  // The command is enabled if the service hasn't loaded yet to trigger loading.
  // The command is updated once the load completes.
  command_updater_.UpdateCommandEnabled(
      IDC_RESTORE_TAB,
      tab_restore_service && (!tab_restore_service->IsLoaded() ||
                              !tab_restore_service->entries().empty()));
}

void BrowserCommandController::UpdateCommandsForFind() {
  TabStripModel* model = browser_->tab_strip_model();
  int active_index = model->active_index();

  bool enabled = active_index != TabStripModel::kNoTab &&
                 !model->IsTabBlocked(active_index) &&
                 !browser_->is_type_devtools();

  command_updater_.UpdateCommandEnabled(IDC_FIND, enabled);
  command_updater_.UpdateCommandEnabled(IDC_FIND_NEXT, enabled);
  command_updater_.UpdateCommandEnabled(IDC_FIND_PREVIOUS, enabled);
}

void BrowserCommandController::UpdateCloseFindOrStop() {
  bool enabled = CanCloseFind(browser_) || IsCommandEnabled(IDC_STOP);
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_FIND_OR_STOP, enabled);
}

void BrowserCommandController::UpdateCommandsForMediaRouter() {
  if (is_locked_fullscreen_)
    return;

  UpdateCommandAndActionEnabled(IDC_ROUTE_MEDIA, kActionRouteMedia,
                                CanRouteMedia(browser_));
}

void BrowserCommandController::UpdateCommandsForTabKeyboardFocus(
    std::optional<int> target_index) {
  command_updater_.UpdateCommandEnabled(
      IDC_DUPLICATE_TARGET_TAB, !browser_->is_type_app() &&
                                    !browser_->is_type_app_popup() &&
                                    target_index.has_value() &&
                                    CanDuplicateTabAt(browser_, *target_index));
  const bool normal_window = browser_->is_type_normal();
  command_updater_.UpdateCommandEnabled(
      IDC_MUTE_TARGET_SITE, normal_window && target_index.has_value());
  command_updater_.UpdateCommandEnabled(
      IDC_PIN_TARGET_TAB, normal_window && target_index.has_value());
  command_updater_.UpdateCommandEnabled(
      IDC_GROUP_TARGET_TAB, normal_window && target_index.has_value());
}

void BrowserCommandController::UpdateCommandsForWebContentsFocus() {
#if BUILDFLAG(IS_MAC)
  // On Mac, toggling caret browsing changes whether it's enabled or not
  // based on web contents focus.
  command_updater_.UpdateCommandEnabled(IDC_CARET_BROWSING_TOGGLE,
                                        CanToggleCaretBrowsing(browser_));
#endif  // BUILDFLAG(IS_MAC)
}

void BrowserCommandController::UpdateCommandsForTabStripStateChanged() {
  if (is_locked_fullscreen_) {
    // Keep tab management commands disabled when in locked fullscreen so users
    // cannot exit this mode. Only relevant for non-web browser scenarios.
    return;
  }

  int tab_index = browser_->tab_strip_model()->active_index();
  // No commands are updated if there is not yet any selected tab.
  if (tab_index == TabStripModel::kNoTab) {
    return;
  }
  command_updater_.UpdateCommandEnabled(
      IDC_CLOSE_TAB,
      web_app::IsTabClosable(browser_->tab_strip_model(), tab_index));
  command_updater_.UpdateCommandEnabled(IDC_WINDOW_CLOSE_TABS_TO_RIGHT,
                                        CanCloseTabsToRight(browser_));
  command_updater_.UpdateCommandEnabled(IDC_WINDOW_CLOSE_OTHER_TABS,
                                        CanCloseOtherTabs(browser_));
  command_updater_.UpdateCommandEnabled(IDC_MOVE_TAB_TO_NEW_WINDOW,
                                        CanMoveActiveTabToNewWindow(browser_));
  UpdateCommandsForBookmarkEditing();
}

actions::ActionItem* BrowserCommandController::FindAction(
    actions::ActionId action_id) {
  BrowserActions* browser_actions = browser_->browser_actions();

  // If there is no root action item then ActionManager falls back to the
  // root_action_parent_ which might contain actions from other browser windows.
  if (!browser_actions->root_action_item()) {
    return nullptr;
  }

  return actions::ActionManager::Get().FindAction(
      action_id, browser_actions->root_action_item());
}

void BrowserCommandController::UpdateCommandAndActionEnabled(
    int command_id,
    actions::ActionId action_id,
    bool enabled) {
  command_updater_.UpdateCommandEnabled(command_id, enabled);
  if (features::IsToolbarPinningEnabled()) {
    if (auto* const action = FindAction(action_id)) {
      action->SetEnabled(enabled);
    }
  }
}

BrowserWindow* BrowserCommandController::window() {
  return browser_->window();
}

Profile* BrowserCommandController::profile() {
  return browser_->profile();
}

void BrowserCommandController::ShowCustomizeChromeSidePanel(
    std::optional<CustomizeChromeSection> section) {
  tabs::TabModel* tab = browser_->tab_strip_model()->GetActiveTab();
  if (!tab || !tab->tab_features() ||
      !tab->tab_features()->customize_chrome_side_panel_controller()) {
    return;
  }

  customize_chrome::SidePanelController* side_panel_controller =
      tab->tab_features()->customize_chrome_side_panel_controller();

  if (!side_panel_controller ||
      !side_panel_controller->IsCustomizeChromeEntryAvailable()) {
    return;
  }

  side_panel_controller->OpenSidePanel(SidePanelOpenTrigger::kAppMenu, section);
}

}  // namespace chrome
