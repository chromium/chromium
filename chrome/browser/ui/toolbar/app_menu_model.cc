// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_model.h"

#include <algorithm>
#include <cmath>

#include "base/command_line.h"
#include "base/debug/debugging_buildflags.h"
#include "base/debug/profiler.h"
#include "base/i18n/number_formatting.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiling.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "components/zoom/zoom_controller.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/models/button_menu_item_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_elider.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "base/feature_list.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

#if defined(OS_WIN)
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "content/public/browser/gpu_data_manager.h"
#endif

using base::UserMetricsAction;
using content::WebContents;

namespace {

constexpr size_t kMaxAppNameLength = 30;

#if defined(OS_MACOSX)
// An empty command used because of a bug in AppKit menus.
// See comment in CreateActionToolbarOverflowMenu().
const int kEmptyMenuItemCommand = 0;
#endif

// Conditionally return the update app menu item title based on upgrade detector
// state.
base::string16 GetUpgradeDialogMenuItemName() {
  if (UpgradeDetector::GetInstance()->is_outdated_install() ||
      UpgradeDetector::GetInstance()->is_outdated_install_no_au()) {
    return l10n_util::GetStringUTF16(IDS_UPGRADE_BUBBLE_MENU_ITEM);
  } else {
    return l10n_util::GetStringUTF16(IDS_UPDATE_NOW);
  }
}

// Returns the appropriate menu label for the IDC_INSTALL_PWA command if
// available.
base::Optional<base::string16> GetInstallPWAAppMenuItemName(Browser* browser) {
  if (!browser->tab_strip_model())
    return base::nullopt;
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return base::nullopt;
  base::string16 app_name =
      banners::AppBannerManager::GetInstallableAppName(web_contents);
  if (app_name.empty())
    return base::nullopt;
  return l10n_util::GetStringFUTF16(IDS_INSTALL_TO_OS_LAUNCH_SURFACE, app_name);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ZoomMenuModel

ZoomMenuModel::ZoomMenuModel(ui::SimpleMenuModel::Delegate* delegate)
    : SimpleMenuModel(delegate) {
  Build();
}

ZoomMenuModel::~ZoomMenuModel() {
}

void ZoomMenuModel::Build() {
  AddItemWithStringId(IDC_ZOOM_PLUS, IDS_ZOOM_PLUS);
  AddItemWithStringId(IDC_ZOOM_NORMAL, IDS_ZOOM_NORMAL);
  AddItemWithStringId(IDC_ZOOM_MINUS, IDS_ZOOM_MINUS);
}

////////////////////////////////////////////////////////////////////////////////
// HelpMenuModel

#if defined(GOOGLE_CHROME_BUILD)

const base::Feature kIncludeBetaForumMenuItem{
    "IncludeBetaForumMenuItem", base::FEATURE_DISABLED_BY_DEFAULT};

class AppMenuModel::HelpMenuModel : public ui::SimpleMenuModel {
 public:
  HelpMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                Browser* browser)
      : SimpleMenuModel(delegate) {
    Build(browser);
  }

 private:
  void Build(Browser* browser) {
#if defined(OS_CHROMEOS) && defined(OFFICIAL_BUILD)
    int help_string_id = IDS_GET_HELP;
#else
    int help_string_id = IDS_HELP_PAGE;
#endif
    AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ABOUT));
    AddItemWithStringId(IDC_HELP_PAGE_VIA_MENU, help_string_id);
    if (base::FeatureList::IsEnabled(kIncludeBetaForumMenuItem))
      AddItem(IDC_SHOW_BETA_FORUM, l10n_util::GetStringUTF16(IDS_BETA_FORUM));
    if (browser_defaults::kShowHelpMenuItemIcon) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      SetIcon(GetIndexOfCommandId(IDC_HELP_PAGE_VIA_MENU),
              rb.GetNativeImageNamed(IDR_HELP_MENU));
    }
    AddItemWithStringId(IDC_FEEDBACK, IDS_FEEDBACK);
  }

  DISALLOW_COPY_AND_ASSIGN(HelpMenuModel);
};

#endif  // defined(GOOGLE_CHROME_BUILD)

////////////////////////////////////////////////////////////////////////////////
// ToolsMenuModel

ToolsMenuModel::ToolsMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                               Browser* browser)
    : SimpleMenuModel(delegate) {
  Build(browser);
}

ToolsMenuModel::~ToolsMenuModel() {}

// More tools submenu is constructed as follows:
// - Page specific actions overflow (save page, adding to desktop).
// - Browser / OS level tools (extensions, task manager).
// - Developer tools.
// - Option to enable profiling.
void ToolsMenuModel::Build(Browser* browser) {
  AddItemWithStringId(IDC_SAVE_PAGE, IDS_SAVE_PAGE);

  if (extensions::util::IsNewBookmarkAppsEnabled())
    AddItemWithStringId(IDC_CREATE_SHORTCUT, IDS_ADD_TO_OS_LAUNCH_SURFACE);

  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(IDC_CLEAR_BROWSING_DATA, IDS_CLEAR_BROWSING_DATA);
  AddItemWithStringId(IDC_MANAGE_EXTENSIONS, IDS_SHOW_EXTENSIONS);
  if (chrome::CanOpenTaskManager())
    AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
#if defined(OS_CHROMEOS)
  AddItemWithStringId(IDC_TAKE_SCREENSHOT, IDS_TAKE_SCREENSHOT);
#endif
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(IDC_DEV_TOOLS, IDS_DEV_TOOLS);

  if (base::debug::IsProfilingSupported()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddCheckItemWithStringId(IDC_PROFILING_ENABLED, IDS_PROFILING_ENABLED);
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppMenuModel

AppMenuModel::AppMenuModel(ui::AcceleratorProvider* provider, Browser* browser)
    : ui::SimpleMenuModel(this),
      uma_action_recorded_(false),
      provider_(provider),
      browser_(browser) {}

AppMenuModel::~AppMenuModel() {
  if (browser_)  // Null in Cocoa tests.
    browser_->tab_strip_model()->RemoveObserver(this);
}

void AppMenuModel::Init() {
  Build();
  UpdateZoomControls();

  browser_zoom_subscription_ =
      zoom::ZoomEventManager::GetForBrowserContext(browser_->profile())
          ->AddZoomLevelChangedCallback(base::Bind(
              &AppMenuModel::OnZoomLevelChanged, base::Unretained(this)));

  browser_->tab_strip_model()->AddObserver(this);

  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
}

bool AppMenuModel::DoesCommandIdDismissMenu(int command_id) const {
  return command_id != IDC_ZOOM_MINUS && command_id != IDC_ZOOM_PLUS;
}

bool AppMenuModel::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_ZOOM_PERCENT_DISPLAY ||
#if defined(OS_MACOSX)
         command_id == IDC_FULLSCREEN ||
#elif defined(OS_WIN)
         command_id == IDC_PIN_TO_START_SCREEN ||
#endif
         command_id == IDC_INSTALL_PWA || command_id == IDC_UPGRADE_DIALOG;
}

base::string16 AppMenuModel::GetLabelForCommandId(int command_id) const {
  switch (command_id) {
    case IDC_ZOOM_PERCENT_DISPLAY:
      return zoom_label_;
#if defined(OS_MACOSX)
    case IDC_FULLSCREEN: {
      int string_id = IDS_ENTER_FULLSCREEN_MAC;  // Default to Enter.
      // Note: On startup, |window()| may be NULL.
      if (browser_->window() && browser_->window()->IsFullscreen())
        string_id = IDS_EXIT_FULLSCREEN_MAC;
      return l10n_util::GetStringUTF16(string_id);
    }
#elif defined(OS_WIN)
    case IDC_PIN_TO_START_SCREEN: {
      int string_id = IDS_PIN_TO_START_SCREEN;
      // TODO(scottmg): Remove http://crbug.com/558054.
      return l10n_util::GetStringUTF16(string_id);
    }
#endif
    case IDC_INSTALL_PWA:
      return GetInstallPWAAppMenuItemName(browser_).value();
    case IDC_UPGRADE_DIALOG:
      return GetUpgradeDialogMenuItemName();
    default:
      NOTREACHED();
      return base::string16();
  }
}

bool AppMenuModel::GetIconForCommandId(int command_id, gfx::Image* icon) const {
  if (command_id == IDC_UPGRADE_DIALOG &&
      UpgradeDetector::GetInstance()->notify_upgrade()) {
    *icon = UpgradeDetector::GetInstance()->GetIcon();
    return true;
  }
  return false;
}

void AppMenuModel::ExecuteCommand(int command_id, int event_flags) {
  GlobalError* error = GlobalErrorServiceFactory::GetForProfile(
      browser_->profile())->GetGlobalErrorByMenuItemCommandID(command_id);
  if (error) {
    error->ExecuteMenuItem(browser_);
    return;
  }

  LogMenuMetrics(command_id);
  chrome::ExecuteCommand(browser_, command_id);
}

void AppMenuModel::LogMenuMetrics(int command_id) {
  base::TimeDelta delta = timer_.Elapsed();

  switch (command_id) {
    case IDC_UPGRADE_DIALOG:
      LogMenuAction(MENU_ACTION_UPGRADE_DIALOG);
      break;
    case IDC_NEW_TAB:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.NewTab", delta);
      LogMenuAction(MENU_ACTION_NEW_TAB);
      break;
    case IDC_NEW_WINDOW:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.NewWindow", delta);
      LogMenuAction(MENU_ACTION_NEW_WINDOW);
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.NewIncognitoWindow",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_NEW_INCOGNITO_WINDOW);
      break;

    // Bookmarks sub menu.
    case IDC_SHOW_BOOKMARK_BAR:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ShowBookmarkBar",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_BOOKMARK_BAR);
      break;
    case IDC_SHOW_BOOKMARK_MANAGER:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ShowBookmarkMgr",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_BOOKMARK_MANAGER);
      break;
    case IDC_IMPORT_SETTINGS:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ImportSettings",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_IMPORT_SETTINGS);
      break;
    case IDC_BOOKMARK_PAGE:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.BookmarkPage",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_BOOKMARK_PAGE);
      break;
    case IDC_BOOKMARK_ALL_TABS:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.BookmarkAllTabs",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_BOOKMARK_ALL_TABS);
      break;
    case IDC_PIN_TO_START_SCREEN:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.PinToStartScreen",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_PIN_TO_START_SCREEN);
      break;

    // Recent tabs menu.
    case IDC_RESTORE_TAB:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.RestoreTab", delta);
      LogMenuAction(MENU_ACTION_RESTORE_TAB);
      break;

    case IDC_DISTILL_PAGE:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.DistillPage",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_DISTILL_PAGE);
      break;
    case IDC_SAVE_PAGE:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.SavePage", delta);
      LogMenuAction(MENU_ACTION_SAVE_PAGE);
      break;
    case IDC_FIND:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.Find", delta);
      LogMenuAction(MENU_ACTION_FIND);
      break;
    case IDC_PRINT:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.Print", delta);
      LogMenuAction(MENU_ACTION_PRINT);
      break;

    case IDC_ROUTE_MEDIA:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.Cast", delta);
      LogMenuAction(MENU_ACTION_CAST);
      break;

    // Edit menu.
    case IDC_CUT:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.Cut", delta);
      LogMenuAction(MENU_ACTION_CUT);
      break;
    case IDC_COPY:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.Copy", delta);
      LogMenuAction(MENU_ACTION_COPY);
      break;
    case IDC_PASTE:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.Paste", delta);
      LogMenuAction(MENU_ACTION_PASTE);
      break;

    // Tools menu.
    case IDC_CREATE_SHORTCUT:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.CreateHostedApp",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_CREATE_HOSTED_APP);
      break;
    case IDC_MANAGE_EXTENSIONS:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ManageExtensions",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_MANAGE_EXTENSIONS);
      break;
    case IDC_TASK_MANAGER:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.TaskManager",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_TASK_MANAGER);
      break;
    case IDC_CLEAR_BROWSING_DATA:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ClearBrowsingData",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_CLEAR_BROWSING_DATA);
      break;
    case IDC_VIEW_SOURCE:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ViewSource", delta);
      LogMenuAction(MENU_ACTION_VIEW_SOURCE);
      break;
    case IDC_DEV_TOOLS:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.DevTools", delta);
      LogMenuAction(MENU_ACTION_DEV_TOOLS);
      break;
    case IDC_DEV_TOOLS_CONSOLE:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.DevToolsConsole",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_DEV_TOOLS_CONSOLE);
      break;
    case IDC_DEV_TOOLS_DEVICES:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.DevToolsDevices",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_DEV_TOOLS_DEVICES);
      break;
    case IDC_PROFILING_ENABLED:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ProfilingEnabled",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_PROFILING_ENABLED);
      break;

    // Zoom menu
    case IDC_ZOOM_MINUS:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ZoomMinus", delta);
        LogMenuAction(MENU_ACTION_ZOOM_MINUS);
      }
      break;
    case IDC_ZOOM_PLUS:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ZoomPlus", delta);
        LogMenuAction(MENU_ACTION_ZOOM_PLUS);
      }
      break;
    case IDC_FULLSCREEN:
      base::RecordAction(UserMetricsAction("EnterFullScreenWithWrenchMenu"));

      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.EnterFullScreen",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_FULLSCREEN);
      break;

    case IDC_SHOW_HISTORY:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ShowHistory",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_HISTORY);
      break;
    case IDC_SHOW_DOWNLOADS:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ShowDownloads",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_DOWNLOADS);
      break;
    case IDC_SHOW_SIGNIN:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ShowSyncSetup",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_SHOW_SYNC_SETUP);
      break;
    case IDC_OPTIONS:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.Settings", delta);
      LogMenuAction(MENU_ACTION_OPTIONS);
      break;
    case IDC_ABOUT:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.About", delta);
      LogMenuAction(MENU_ACTION_ABOUT);
      break;

    // Help menu.
    case IDC_HELP_PAGE_VIA_MENU:
      base::RecordAction(UserMetricsAction("ShowHelpTabViaWrenchMenu"));

      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.HelpPage", delta);
      LogMenuAction(MENU_ACTION_HELP_PAGE_VIA_MENU);
      break;
  #if defined(GOOGLE_CHROME_BUILD)
    case IDC_SHOW_BETA_FORUM:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.BetaForum", delta);
      LogMenuAction(MENU_ACTION_BETA_FORUM);
      break;
    case IDC_FEEDBACK:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.Feedback", delta);
      LogMenuAction(MENU_ACTION_FEEDBACK);
      break;
  #endif

    case IDC_TOGGLE_REQUEST_TABLET_SITE:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.RequestTabletSite",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_TOGGLE_REQUEST_TABLET_SITE);
      break;
    case IDC_EXIT:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.Exit", delta);
      LogMenuAction(MENU_ACTION_EXIT);
      break;

    // Hosted App menu.
    case IDC_COPY_URL:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.CopyUrl", delta);
      LogMenuAction(MENU_ACTION_COPY_URL);
      break;
    case IDC_OPEN_IN_CHROME:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.OpenInChrome",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_OPEN_IN_CHROME);
      break;
    case IDC_SITE_SETTINGS:
      if (!uma_action_recorded_) {
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.SiteSettings",
                                   delta);
      }
      LogMenuAction(MENU_ACTION_SITE_SETTINGS);
      break;
    case IDC_HOSTED_APP_MENU_APP_INFO:
      if (!uma_action_recorded_)
        UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.AppInfo", delta);
      LogMenuAction(MENU_ACTION_APP_INFO);
      break;
  }

  if (!uma_action_recorded_) {
    UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction", delta);
    uma_action_recorded_ = true;
  }
}

bool AppMenuModel::IsCommandIdChecked(int command_id) const {
  if (command_id == IDC_SHOW_BOOKMARK_BAR) {
    return browser_->profile()->GetPrefs()->GetBoolean(
        bookmarks::prefs::kShowBookmarkBar);
  }
  if (command_id == IDC_PROFILING_ENABLED)
    return Profiling::BeingProfiled();
  if (command_id == IDC_TOGGLE_REQUEST_TABLET_SITE)
    return chrome::IsRequestingTabletSite(browser_);

  return false;
}

bool AppMenuModel::IsCommandIdEnabled(int command_id) const {
  GlobalError* error = GlobalErrorServiceFactory::GetForProfile(
      browser_->profile())->GetGlobalErrorByMenuItemCommandID(command_id);
  if (error)
    return true;

  return chrome::IsCommandEnabled(browser_, command_id);
}

bool AppMenuModel::IsCommandIdVisible(int command_id) const {
  switch (command_id) {
#if defined(OS_MACOSX)
    case kEmptyMenuItemCommand:
      return false;  // Always hidden (see CreateActionToolbarOverflowMenu).
#endif
    case IDC_PIN_TO_START_SCREEN:
      return false;
    case IDC_UPGRADE_DIALOG:
      return browser_defaults::kShowUpgradeMenuItem &&
          UpgradeDetector::GetInstance()->notify_upgrade();
#if !defined(OS_LINUX) || defined(USE_AURA)
    case IDC_BOOKMARK_PAGE:
      return !chrome::ShouldRemoveBookmarkThisPageUI(browser_->profile());
    case IDC_BOOKMARK_ALL_TABS:
      return !chrome::ShouldRemoveBookmarkOpenPagesUI(browser_->profile());
#endif
    default:
      return true;
  }
}

bool AppMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return provider_->GetAcceleratorForCommandId(command_id, accelerator);
}

void AppMenuModel::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->empty())
    return;

  if (selection.active_tab_changed()) {
    // The user has switched between tabs and the new tab may have a different
    // zoom setting. Or web contents for a tab has been replaced.
    UpdateZoomControls();
  }
}

void AppMenuModel::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
  UpdateZoomControls();
}

void AppMenuModel::LogMenuAction(AppMenuAction action_id) {
  UMA_HISTOGRAM_ENUMERATION("WrenchMenu.MenuAction", action_id,
                            LIMIT_MENU_ACTION);
}

// Note: When adding new menu items please place under an appropriate section.
// Menu is organised as follows:
// - Extension toolbar overflow.
// - Global browser errors and warnings.
// - Tabs and windows.
// - Places previously been e.g. History, bookmarks, recent tabs.
// - Page actions e.g. zoom, edit, find, print.
// - Learn about the browser and global customisation e.g. settings, help.
// - Browser relaunch, quit.
void AppMenuModel::Build() {
  // Build (and, by extension, Init) should only be called once.
  DCHECK_EQ(0, GetItemCount());

  if (CreateActionToolbarOverflowMenu())
    AddSeparator(ui::UPPER_SEPARATOR);

  if (IsCommandIdVisible(IDC_UPGRADE_DIALOG))
    AddItem(IDC_UPGRADE_DIALOG, GetUpgradeDialogMenuItemName());
  if (AddGlobalErrorMenuItems() ||
      IsCommandIdVisible(IDC_UPGRADE_DIALOG))
    AddSeparator(ui::NORMAL_SEPARATOR);

  AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  AddItemWithStringId(IDC_NEW_WINDOW, IDS_NEW_WINDOW);
  if (ShouldShowNewIncognitoWindowMenuItem())
    AddItemWithStringId(IDC_NEW_INCOGNITO_WINDOW, IDS_NEW_INCOGNITO_WINDOW);
  AddSeparator(ui::NORMAL_SEPARATOR);

  if (!browser_->profile()->IsOffTheRecord()) {
    recent_tabs_sub_menu_model_ =
        std::make_unique<RecentTabsSubMenuModel>(provider_, browser_);
    AddSubMenuWithStringId(IDC_RECENT_TABS_MENU, IDS_HISTORY_MENU,
                           recent_tabs_sub_menu_model_.get());
  }
  AddItemWithStringId(IDC_SHOW_DOWNLOADS, IDS_SHOW_DOWNLOADS);
  if (!browser_->profile()->IsGuestSession()) {
    bookmark_sub_menu_model_.reset(new BookmarkSubMenuModel(this, browser_));
    AddSubMenuWithStringId(IDC_BOOKMARKS_MENU, IDS_BOOKMARKS_MENU,
                           bookmark_sub_menu_model_.get());
  }

  AddSeparator(ui::LOWER_SEPARATOR);
  CreateZoomMenu();
  AddSeparator(ui::UPPER_SEPARATOR);
  AddItemWithStringId(IDC_PRINT, IDS_PRINT);

  if (media_router::MediaRouterEnabled(browser()->profile()))
    AddItemWithStringId(IDC_ROUTE_MEDIA, IDS_MEDIA_ROUTER_MENU_ITEM_TITLE);

  AddItemWithStringId(IDC_FIND, IDS_FIND);
  if (extensions::util::IsNewBookmarkAppsEnabled() &&
      banners::AppBannerManager::IsExperimentalAppBannersEnabled()) {
    const extensions::Extension* pwa =
        base::FeatureList::IsEnabled(features::kDesktopPWAWindowing)
            ? extensions::util::GetPwaForSecureActiveTab(browser_)
            : nullptr;
    if (pwa) {
      AddItem(
          IDC_OPEN_IN_PWA_WINDOW,
          l10n_util::GetStringFUTF16(
              IDS_OPEN_IN_APP_WINDOW,
              gfx::TruncateString(base::UTF8ToUTF16(pwa->name()),
                                  kMaxAppNameLength, gfx::CHARACTER_BREAK)));
    } else {
      base::Optional<base::string16> install_pwa_item_name =
          GetInstallPWAAppMenuItemName(browser_);
      if (install_pwa_item_name)
        AddItem(IDC_INSTALL_PWA, *install_pwa_item_name);
    }
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDomDistiller))
    AddItemWithStringId(IDC_DISTILL_PAGE, IDS_DISTILL_PAGE);
  tools_menu_model_.reset(new ToolsMenuModel(this, browser_));
  AddSubMenuWithStringId(
      IDC_MORE_TOOLS_MENU, IDS_MORE_TOOLS_MENU, tools_menu_model_.get());
  AddSeparator(ui::LOWER_SEPARATOR);
  CreateCutCopyPasteMenu();
  AddSeparator(ui::UPPER_SEPARATOR);

  AddItemWithStringId(IDC_OPTIONS, IDS_SETTINGS);
// The help submenu is only displayed on official Chrome builds. As the
// 'About' item has been moved to this submenu, it's reinstated here for
// Chromium builds.
#if defined(GOOGLE_CHROME_BUILD)
  help_menu_model_.reset(new HelpMenuModel(this, browser_));
  AddSubMenuWithStringId(IDC_HELP_MENU, IDS_HELP_MENU,
                         help_menu_model_.get());
#else
  AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ABOUT));
#endif
#if defined(OS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableRequestTabletSite))
    AddCheckItemWithStringId(IDC_TOGGLE_REQUEST_TABLET_SITE,
                             IDS_TOGGLE_REQUEST_TABLET_SITE);
#endif

  if (browser_defaults::kShowExitMenuItem) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(IDC_EXIT, IDS_EXIT);
  }
  uma_action_recorded_ = false;
}

bool AppMenuModel::CreateActionToolbarOverflowMenu() {
  // We only add the extensions overflow container if there are any icons that
  // aren't shown in the main container.
  // browser_->window() can return null during startup, and
  // GetToolbarActionsBar() can be null in testing.
  if (browser_->window() && browser_->window()->GetToolbarActionsBar() &&
      browser_->window()->GetToolbarActionsBar()->NeedsOverflow()) {
#if defined(OS_MACOSX)
    // There's a bug in AppKit menus, where if a menu item with a custom view
    // (like the extensions overflow menu) is the first menu item, it is not
    // highlightable or keyboard-selectable.
    // Adding any menu item before it (even one which is never visible) prevents
    // it, so add a bogus item here that will always be hidden.
    AddItem(kEmptyMenuItemCommand, base::string16());
#endif
    AddItem(IDC_EXTENSIONS_OVERFLOW_MENU, base::string16());
    return true;
  }
  return false;
}

void AppMenuModel::CreateCutCopyPasteMenu() {
  // WARNING: Mac does not use the ButtonMenuItemModel, but instead defines the
  // layout for this menu item in AppMenu.xib. It does, however, use the
  // command_id value from AddButtonItem() to identify this special item.
  edit_menu_item_model_.reset(new ui::ButtonMenuItemModel(IDS_EDIT, this));
  edit_menu_item_model_->AddGroupItemWithStringId(IDC_CUT, IDS_CUT);
  edit_menu_item_model_->AddGroupItemWithStringId(IDC_COPY, IDS_COPY);
  edit_menu_item_model_->AddGroupItemWithStringId(IDC_PASTE, IDS_PASTE);
  AddButtonItem(IDC_EDIT_MENU, edit_menu_item_model_.get());
}

void AppMenuModel::CreateZoomMenu() {
  // WARNING: Mac does not use the ButtonMenuItemModel, but instead defines the
  // layout for this menu item in AppMenu.xib. It does, however, use the
  // command_id value from AddButtonItem() to identify this special item.
  zoom_menu_item_model_.reset(
      new ui::ButtonMenuItemModel(IDS_ZOOM_MENU, this));
  zoom_menu_item_model_->AddGroupItemWithStringId(IDC_ZOOM_MINUS,
                                                  IDS_ZOOM_MINUS2);
  zoom_menu_item_model_->AddGroupItemWithStringId(IDC_ZOOM_PLUS,
                                                  IDS_ZOOM_PLUS2);
  zoom_menu_item_model_->AddItemWithImage(IDC_FULLSCREEN,
                                          IDR_FULLSCREEN_MENU_BUTTON);
  AddButtonItem(IDC_ZOOM_MENU, zoom_menu_item_model_.get());
}

void AppMenuModel::UpdateZoomControls() {
  int zoom_percent = 100;
  if (browser_->tab_strip_model() &&
      browser_->tab_strip_model()->GetActiveWebContents()) {
    zoom_percent = zoom::ZoomController::FromWebContents(
                       browser_->tab_strip_model()->GetActiveWebContents())
                       ->GetZoomPercent();
  }
  zoom_label_ = base::FormatPercent(zoom_percent);
}

bool AppMenuModel::ShouldShowNewIncognitoWindowMenuItem() {
  if (browser_->profile()->IsGuestSession())
    return false;

  return IncognitoModePrefs::GetAvailability(browser_->profile()->GetPrefs()) !=
         IncognitoModePrefs::DISABLED;
}

bool AppMenuModel::AddGlobalErrorMenuItems() {
  // TODO(sail): Currently we only build the app menu once per browser
  // window. This means that if a new error is added after the menu is built
  // it won't show in the existing app menu. To fix this we need to some
  // how update the menu if new errors are added.
  const GlobalErrorService::GlobalErrorList& errors =
      GlobalErrorServiceFactory::GetForProfile(browser_->profile())->errors();
  bool menu_items_added = false;
  for (auto it = errors.begin(); it != errors.end(); ++it) {
    GlobalError* error = *it;
    DCHECK(error);
    if (error->HasMenuItem()) {
      AddItem(error->MenuItemCommandID(), error->MenuItemLabel());
      SetIcon(GetIndexOfCommandId(error->MenuItemCommandID()),
              error->MenuItemIcon());
      menu_items_added = true;
      if (IDC_SHOW_SIGNIN_ERROR == error->MenuItemCommandID()) {
        signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
            signin_metrics::AccessPoint::ACCESS_POINT_MENU);
      }
    }
  }
  return menu_items_added;
}

void AppMenuModel::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  UpdateZoomControls();
}
