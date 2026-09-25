// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "ui/actions/actions.h"

namespace {

constexpr char kMenuActionHistogram[] = "WrenchMenu.MenuAction";
constexpr char kTimeToActionHistogram[] = "WrenchMenu.TimeToAction";
constexpr char kTimeToActionPrefix[] = "WrenchMenu.TimeToAction.";

void LogMenuActionHistogram(AppMenuAction action_id) {
  UMA_HISTOGRAM_ENUMERATION(kMenuActionHistogram, action_id, LIMIT_MENU_ACTION);
}

}  // namespace

ActionAppMenuMetrics::ActionAppMenuMetrics() = default;

ActionAppMenuMetrics::~ActionAppMenuMetrics() = default;

void ActionAppMenuMetrics::OnMenuOpened() {
  menu_opened_timer_ = base::ElapsedTimer();
  uma_action_recorded_ = false;
  base::RecordAction(base::UserMetricsAction("ShowAppMenu"));
  LogMenuActionHistogram(MENU_ACTION_MENU_OPENED);
}

void ActionAppMenuMetrics::OnWillShowSubMenu(int command_id) {
  if (command_id == kActionSavedTabGroupsSubmenu) {
    base::UmaHistogramMediumTimes(
        base::StrCat({kTimeToActionPrefix, "ShowSavedTabGroups"}),
        menu_opened_timer_.Elapsed());
    LogMenuActionHistogram(MENU_ACTION_SHOW_SAVED_TAB_GROUPS);
  }
}

void ActionAppMenuMetrics::LogMenuAction(actions::BaseAction* base_action) {
  actions::ActionItem* action_ptr = base_action->GetActionItem();
  CHECK(action_ptr);

  if (action_ptr->GetActionId().has_value()) {
    LogMenuActionWithId(action_ptr->GetActionId().value());
    return;
  }

  for (actions::BaseAction* parent = base_action->GetParent(); parent;
       parent = parent->GetParent()) {
    std::optional<actions::ActionId> parent_id =
        parent->GetActionItem()->GetActionId();
    if (parent_id == kActionBookmarksSubmenu) {
      RecordAction(MENU_ACTION_BOOKMARK_OPEN, "OpenBookmark");
      return;
    }
    if (parent_id == kActionRecentTabsSubmenu) {
      LogMenuActionWithId(kActionOpenRecentTab);
      return;
    }
    if (parent_id == kActionProfileSubmenu) {
      RecordAction(MENU_ACTION_SWITCH_TO_ANOTHER_PROFILE,
                   "SwitchToAnotherProfile");
      return;
    }
  }
}

void ActionAppMenuMetrics::RecordAction(
    int action_id,
    std::string_view time_to_action_suffix) {
  CHECK(!time_to_action_suffix.empty());
  if (!uma_action_recorded_) {
    base::UmaHistogramMediumTimes(
        base::StrCat({kTimeToActionPrefix, time_to_action_suffix}),
        menu_opened_timer_.Elapsed());
    RecordTimeToAction();
  }
  LogMenuActionHistogram(static_cast<AppMenuAction>(action_id));
}

void ActionAppMenuMetrics::RecordTimeToAction() {
  if (!uma_action_recorded_) {
    base::UmaHistogramMediumTimes(kTimeToActionHistogram,
                                  menu_opened_timer_.Elapsed());
    uma_action_recorded_ = true;
  }
}

void ActionAppMenuMetrics::LogMenuActionWithId(actions::ActionId action_id) {
  switch (action_id) {
    case kActionNewTab:
      RecordAction(MENU_ACTION_NEW_TAB, "NewTab");
      break;
    case kActionNewWindow:
      RecordAction(MENU_ACTION_NEW_WINDOW, "NewWindow");
      break;
    case kActionNewIncognitoWindow:
      RecordAction(MENU_ACTION_NEW_INCOGNITO_WINDOW, "NewIncognitoWindow");
      break;

    // Bookmarks sub menu.
    case kActionShowBookmarkBar:
      RecordAction(MENU_ACTION_SHOW_BOOKMARK_BAR, "ShowBookmarkBar");
      break;
    case kActionSidePanelShowBookmarks:
      RecordAction(MENU_ACTION_SHOW_BOOKMARK_SIDE_PANEL,
                   "ShowBookmarkSidePanel");
      break;
    case kActionShowBookmarkManager:
      RecordAction(MENU_ACTION_SHOW_BOOKMARK_MANAGER, "ShowBookmarkMgr");
      break;
    case kActionImportSettings:
      RecordAction(MENU_ACTION_IMPORT_SETTINGS, "ImportSettings");
      break;
    case kActionBookmarkThisTab:
      RecordAction(MENU_ACTION_BOOKMARK_THIS_TAB, "BookmarkPage");
      break;
    case kActionBookmarkAllTabs:
      RecordAction(MENU_ACTION_BOOKMARK_ALL_TABS, "BookmarkAllTabs");
      break;

    // Lens overlay.
    case kActionShowLensOverlayFromAppMenu:
      RecordAction(MENU_ACTION_SHOW_LENS_OVERLAY, "ShowLensOverlay");
      break;

    // Extensions menu.
    case kActionExtensionsSubmenuManageExtensions:
      RecordAction(MENU_ACTION_MANAGE_EXTENSIONS, "ManageExtensions");
      break;
    case kActionExtensionsSubmenuVisitChromeWebStore:
      RecordAction(MENU_ACTION_VISIT_CHROME_WEB_STORE, "VisitChromeWebStore");
      break;
    case kActionFindExtensions:
      RecordAction(MENU_ACTION_FIND_EXTENSIONS, "FindExtensions");
      break;

    // Recent tabs menu.
    case kActionRestoreTab:
      RecordAction(MENU_ACTION_RESTORE_TAB, "RestoreTab");
      break;
    case kActionOpenRecentTab:
      RecordAction(MENU_ACTION_RECENT_TAB, "OpenRecentTab");
      break;
    case kActionRecentTabsLoginForDeviceTabs:
      RecordAction(MENU_ACTION_RECENT_TABS_LOGIN_FOR_DEVICE_TABS,
                   "LoginForDeviceTabs");
      break;
    case kActionRecentTabsSeeDeviceTabs:
      RecordAction(MENU_ACTION_RECENT_TABS_SEE_DEVICE_TABS, "SeeDeviceTabs");
      break;

    case kActionFind:
      RecordAction(MENU_ACTION_FIND, "Find");
      break;
    case kActionPrint:
      RecordAction(MENU_ACTION_PRINT, "Print");
      break;
    case kActionOpenGlic:
      RecordAction(MENU_ACTION_OPEN_GLIC, "OpenGlic");
      break;
    case kActionShowTtcMenu:
      RecordAction(MENU_ACTION_TTC_APP_MENU, "TtcAppMenu");
      break;
    case kActionShowTranslate:
      RecordAction(MENU_ACTION_SHOW_TRANSLATE, "ShowTranslate");
      break;

    // Edit menu.
    case actions::kActionCut:
      RecordAction(MENU_ACTION_CUT, "Cut");
      break;
    case actions::kActionCopy:
      RecordAction(MENU_ACTION_COPY, "Copy");
      break;
    case actions::kActionPaste:
      RecordAction(MENU_ACTION_PASTE, "Paste");
      break;

    // Save and share menu.
    case kActionSavePage:
      RecordAction(MENU_ACTION_SAVE_PAGE, "SavePage");
      break;
    case kActionInstallPwa:
      RecordAction(MENU_ACTION_INSTALL_PWA, "InstallPwa");
      break;
    case kActionOpenInPwaWindow:
      RecordAction(MENU_ACTION_OPEN_IN_PWA_WINDOW, "OpenInPwaWindow");
      break;
    case kActionCreateShortcut:
      RecordAction(MENU_ACTION_CREATE_HOSTED_APP, "CreateHostedApp");
      break;
    case kActionCopyUrl:
      RecordAction(MENU_ACTION_COPY_URL, "CopyUrl");
      break;
    case kActionSendTabToSelf:
      RecordAction(MENU_ACTION_SEND_TO_DEVICES, "SendToDevices");
      break;
    case kActionQrCodeGenerator:
      RecordAction(MENU_ACTION_CREATE_QR_CODE, "CreateQrCode");
      break;
    case kActionRouteMedia:
      RecordAction(MENU_ACTION_CAST, "Cast");
      break;

    // Tools menu.
    case kActionManageExtensions:
      RecordAction(MENU_ACTION_MANAGE_EXTENSIONS, "ManageExtensions");
      break;
    case kActionTaskManager:
    case kActionTaskManagerAppMenu:
      RecordAction(MENU_ACTION_TASK_MANAGER, "TaskManager");
      break;
    case kActionClearBrowsingData:
      RecordAction(MENU_ACTION_CLEAR_BROWSING_DATA, "ClearBrowsingData");
      break;
    case kActionViewSource:
      RecordAction(MENU_ACTION_VIEW_SOURCE, "ViewSource");
      break;
    case kActionDevTools:
      RecordAction(MENU_ACTION_DEV_TOOLS, "DevTools");
      break;
    case kActionDevToolsConsole:
      RecordAction(MENU_ACTION_DEV_TOOLS_CONSOLE, "DevToolsConsole");
      break;
    case kActionDevToolsDevices:
      RecordAction(MENU_ACTION_DEV_TOOLS_DEVICES, "DevToolsDevices");
      break;
    case kActionProfilingEnabled:
      RecordAction(MENU_ACTION_PROFILING_ENABLED, "ProfilingEnabled");
      break;
    case kActionShowChromeLabs:
      RecordAction(MENU_ACTION_SHOW_CHROME_LABS, "ShowChromeLabs");
      break;
    case kActionSidePanelShowHistoryCluster:
      RecordAction(MENU_ACTION_SHOW_HISTORY_CLUSTER_SIDE_PANEL,
                   "ShowHistoryClustersSidePanel");
      break;
    case kActionShowReadingModeSidePanel:
      RecordAction(MENU_ACTION_SHOW_READING_MODE_SIDE_PANEL,
                   "ShowReadingModeSidePanel");
      break;
    case kActionSidePanelShowCustomizeChrome:
      RecordAction(MENU_ACTION_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL,
                   "ShowCustomizeChromeSidePanel");
      break;

    // Zoom menu: ZoomMinus and ZoomPlus keep the menu open, so only log
    // MENU_ACTION_ZOOM_* if no action has been recorded yet.
    case kActionZoomMinus:
      if (!uma_action_recorded_) {
        RecordAction(MENU_ACTION_ZOOM_MINUS, "ZoomMinus");
      }
      break;
    case kActionZoomPlus:
      if (!uma_action_recorded_) {
        RecordAction(MENU_ACTION_ZOOM_PLUS, "ZoomPlus");
      }
      break;
    case kActionFullscreen:
      base::RecordAction(
          base::UserMetricsAction("EnterFullScreenWithWrenchMenu"));
      RecordAction(MENU_ACTION_FULLSCREEN, "EnterFullScreen");
      break;

    case kActionShowHistory:
      RecordAction(MENU_ACTION_SHOW_HISTORY, "ShowHistory");
      break;
    case kActionShowDownloadsPage:
      RecordAction(MENU_ACTION_SHOW_DOWNLOADS, "ShowDownloads");
      break;
    case kActionOptions:
      RecordAction(MENU_ACTION_OPTIONS, "Settings");
      break;
    case kActionAbout:
      RecordAction(MENU_ACTION_ABOUT, "About");
      break;

    // Help menu.
    case kActionHelpPageViaMenu:
      base::RecordAction(base::UserMetricsAction("ShowHelpTabViaWrenchMenu"));
      RecordAction(MENU_ACTION_HELP_PAGE_VIA_MENU, "HelpPage");
      break;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case kActionShowBetaForum:
      RecordAction(MENU_ACTION_BETA_FORUM, "BetaForum");
      break;
    case kActionFeedback:
      RecordAction(MENU_ACTION_FEEDBACK, "Feedback");
      break;
    case kActionChromeTips:
      RecordAction(MENU_ACTION_CHROME_TIPS, "ChromeTips");
      break;
    case kActionChromeWhatsNew:
      RecordAction(MENU_ACTION_CHROME_WHATS_NEW, "ChromeWhatsNew");
      break;
#endif

    case kActionToggleRequestTabletSite:
      RecordAction(MENU_ACTION_TOGGLE_REQUEST_TABLET_SITE, "RequestTabletSite");
      break;
    case kActionExit:
      RecordAction(MENU_ACTION_EXIT, "Exit");
      break;

    // Hosted App menu.
    case kActionOpenInChrome:
      RecordAction(MENU_ACTION_OPEN_IN_CHROME, "OpenInChrome");
      break;
    case kActionWebAppMenuAppInfo:
      RecordAction(MENU_ACTION_APP_INFO, "AppInfo");
      break;
    case kActionViewPasswords:
      RecordAction(MENU_ACTION_PASSWORD_MANAGER, "PasswordManager");
      break;

      // Profile submenu.
#if !BUILDFLAG(IS_CHROMEOS)
    case kActionCustomizeChrome:
      RecordAction(MENU_ACTION_CUSTOMIZE_CHROME, "CustomizeChrome");
      break;
    case kActionCloseProfile:
      RecordAction(MENU_ACTION_CLOSE_PROFILE, "CloseProfile");
      break;
    case kActionManageGoogleAccount:
      RecordAction(MENU_ACTION_MANAGE_GOOGLE_ACCOUNT, "ManageGoogleAccount");
      break;
    case kActionShowSyncSettings:
      RecordAction(MENU_SHOW_SYNC_SETTINGS, "ShowSyncSettings");
      break;
    case kActionShowSignin:
      RecordAction(MENU_SHOW_SIGNIN, "ShowSignin");
      break;
    case kActionTurnOnSync:
      RecordAction(MENU_TURN_ON_SYNC, "ShowTurnOnSync");
      break;
    case kActionShowSigninWhenPaused:
      RecordAction(MENU_SHOW_SIGNIN_WHEN_PAUSED, "ShowSigninWhenPaused");
      break;
    case kActionOpenGuestProfile:
      RecordAction(MENU_ACTION_OPEN_GUEST_PROFILE, "OpenGuestProfile");
      break;
    case kActionAddNewProfile:
      RecordAction(MENU_ACTION_ADD_NEW_PROFILE, "AddNewProfile");
      break;
    case kActionManageChromeProfiles:
      RecordAction(MENU_ACTION_MANAGE_CHROME_PROFILES, "ManageChromeProfiles");
      break;
#endif

    // Reading list submenu.
    case kActionReadingListMenuAddTab:
      RecordAction(MENU_ACTION_READING_LIST_ADD_TAB, "ReadingListAddTab");
      break;
    case kActionSidePanelShowReadingList:
      RecordAction(MENU_ACTION_READING_LIST_SHOW_UI, "ReadingListShowUi");
      break;

    // Password autofill submenu.
    case kActionShowPasswordManager:
      RecordAction(MENU_ACTION_SHOW_PASSWORD_MANAGER, "ShowPasswordManager");
      break;
    case kActionShowPaymentMethods:
      RecordAction(MENU_ACTION_SHOW_PAYMENT_METHODS, "ShowPaymentMethods");
      break;
    case kActionShowContactInfo:
      RecordAction(MENU_ACTION_SHOW_CONTACT_INFO, "ShowContactInfo");
      break;
    case kActionShowIdentityDocs:
      RecordAction(MENU_ACTION_SHOW_IDENTITY_DOCS, "ShowIdentityDocs");
      break;
    case kActionShowTravel:
      RecordAction(MENU_ACTION_SHOW_TRAVEL, "ShowTravel");
      break;

    case kActionPerformance:
      RecordAction(MENU_ACTION_SHOW_PERFORMANCE_SETTINGS,
                   "ShowPerformanceSettings");
      break;
    case kActionSetBrowserAsDefault:
      RecordAction(MENU_ACTION_SET_BROWSER_AS_DEFAULT, "SetBrowserAsDefault");
      break;
    case kActionSafetyHubShowPasswordCheckup:
      RecordAction(MENU_ACTION_SAFETY_HUB_SHOW_PASSWORD_CHECKUP,
                   "SafetyHubNotificationPasswordCheck");
      break;
    case kActionOpenSafetyHub:
      RecordAction(MENU_ACTION_SHOW_SAFETY_HUB,
                   "SafetyHubNotificationOpenSafetyHub");
      break;
    case kActionSafetyHubManageExtensions:
      RecordAction(MENU_ACTION_SAFETY_HUB_MANAGE_EXTENSIONS,
                   "SafetyHubNotificationManageExtensions");
      break;

    // Actions present in the menu that do not have a per-action TimeToAction
    // variant in histograms.xml, but still record the overall
    // WrenchMenu.TimeToAction histogram (and WrenchMenu.MenuAction if defined).
    // TODO(crbug.com/565832018): Add TimeToAction and MenuAction entries for
    // each of these.
    case kActionUpgradeDialog:
      RecordTimeToAction();
      LogMenuActionHistogram(MENU_ACTION_UPGRADE_DIALOG);
      break;
    case kActionNewIsolatedWindow:
      RecordTimeToAction();
      LogMenuActionHistogram(MENU_ACTION_NEW_ISOLATED_WINDOW);
      break;
    case kActionBookmarkBarSubmenuAlwaysHide:
    case kActionBookmarkBarSubmenuAlwaysShow:
    case kActionBookmarkBarSubmenuOnlyOnNtp:
    case kActionCreateNewTabGroup:
    case kActionGlobalError:
    case kActionNameWindow:
    case kActionReportUnsafeSite:
    case kActionSharingHubScreenshot:
    case kActionShowManagementPage:
    case kActionShowSyncPassphraseDialog:
    case kActionTabGroupDelete:
    case kActionTabGroupOpenInBrowser:
    case kActionTabGroupOpenInNewWindow:
    case kActionTabGroupPin:
    case kActionTabSearch:
    case kActionTakeScreenshot:
    case kActionToggleVerticalTabs:
      RecordTimeToAction();
      break;

    default:
      NOTREACHED();
  }
}
