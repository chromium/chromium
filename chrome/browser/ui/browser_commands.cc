// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"  // nogncheck
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_types.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help_factory.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/feature_engagement/buildflags.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/translate/core/browser/language_state.h"
#include "components/version_info/version_info.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/user_agent.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/escape.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/print_view_manager_common.h"
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_LEGACY_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/feature_engagement/incognito_window/incognito_window_tracker.h"
#include "chrome/browser/feature_engagement/incognito_window/incognito_window_tracker_factory.h"
#endif

namespace {

const char kOsOverrideForTabletSite[] = "Linux; Android 9; Chrome tablet";

translate::TranslateBubbleUiEvent TranslateBubbleResultToUiEvent(
    ShowTranslateBubbleResult result) {
  switch (result) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case ShowTranslateBubbleResult::SUCCESS:
      return translate::TranslateBubbleUiEvent::BUBBLE_SHOWN;
    case ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_VALID:
      return translate::TranslateBubbleUiEvent::
          BUBBLE_NOT_SHOWN_WINDOW_NOT_VALID;
    case ShowTranslateBubbleResult::BROWSER_WINDOW_MINIMIZED:
      return translate::TranslateBubbleUiEvent::
          BUBBLE_NOT_SHOWN_WINDOW_MINIMIZED;
    case ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_ACTIVE:
      return translate::TranslateBubbleUiEvent::
          BUBBLE_NOT_SHOWN_WINDOW_NOT_ACTIVE;
    case ShowTranslateBubbleResult::WEB_CONTENTS_NOT_ACTIVE:
      return translate::TranslateBubbleUiEvent::
          BUBBLE_NOT_SHOWN_WEB_CONTENTS_NOT_ACTIVE;
    case ShowTranslateBubbleResult::EDITABLE_FIELD_IS_ACTIVE:
      return translate::TranslateBubbleUiEvent::
          BUBBLE_NOT_SHOWN_EDITABLE_FIELD_IS_ACTIVE;
  }
}

}  // namespace

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace chrome {
namespace {

bool CanBookmarkCurrentTabInternal(const Browser* browser,
                                   bool check_remove_bookmark_ui) {
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  return browser_defaults::bookmarks_enabled &&
         browser->profile()->GetPrefs()->GetBoolean(
             bookmarks::prefs::kEditBookmarksEnabled) &&
         model && model->loaded() && browser->is_type_normal() &&
         (!check_remove_bookmark_ui ||
          !chrome::ShouldRemoveBookmarkThisTabUI(browser->profile()));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
const extensions::Extension* GetExtensionForBrowser(Browser* browser) {
  return extensions::ExtensionRegistry::Get(browser->profile())
      ->GetExtensionById(
          web_app::GetAppIdFromApplicationName(browser->app_name()),
          extensions::ExtensionRegistry::EVERYTHING);
}

bool GetBookmarkOverrideCommand(Profile* profile,
                                const extensions::Extension** extension,
                                extensions::Command* command) {
  DCHECK(extension);
  DCHECK(command);

  ui::Accelerator bookmark_tab_accelerator =
      chrome::GetPrimaryChromeAcceleratorForBookmarkTab();
  if (bookmark_tab_accelerator.key_code() == ui::VKEY_UNKNOWN)
    return false;

  extensions::CommandService* command_service =
      extensions::CommandService::Get(profile);
  const extensions::ExtensionSet& extension_set =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator i = extension_set.begin();
       i != extension_set.end(); ++i) {
    extensions::Command prospective_command;
    if (command_service->GetSuggestedExtensionCommand(
            (*i)->id(), bookmark_tab_accelerator, &prospective_command)) {
      *extension = i->get();
      *command = prospective_command;
      return true;
    }
  }
  return false;
}
#endif

// Based on |disposition|, creates a new tab as necessary, and returns the
// appropriate tab to navigate.  If that tab is the |current_tab|, reverts the
// location bar contents, since all browser-UI-triggered navigations should
// revert any omnibox edits in the |current_tab|.
WebContents* GetTabAndRevertIfNecessaryHelper(Browser* browser,
                                              WindowOpenDisposition disposition,
                                              WebContents* current_tab) {
  switch (disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB: {
      std::unique_ptr<WebContents> new_tab = current_tab->Clone();
      WebContents* raw_new_tab = new_tab.get();
      if (disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB)
        new_tab->WasHidden();
      const int index =
          browser->tab_strip_model()->GetIndexOfWebContents(current_tab);
      const auto group = browser->tab_strip_model()->GetTabGroupForTab(index);
      browser->tab_strip_model()->AddWebContents(
          std::move(new_tab), -1, ui::PAGE_TRANSITION_LINK,
          (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB)
              ? TabStripModel::ADD_ACTIVE
              : TabStripModel::ADD_NONE,
          group);
      return raw_new_tab;
    }
    case WindowOpenDisposition::NEW_WINDOW: {
      std::unique_ptr<WebContents> new_tab = current_tab->Clone();
      WebContents* raw_new_tab = new_tab.get();
      Browser* new_browser =
          new Browser(Browser::CreateParams(browser->profile(), true));
      new_browser->tab_strip_model()->AddWebContents(std::move(new_tab), -1,
                                                     ui::PAGE_TRANSITION_LINK,
                                                     TabStripModel::ADD_ACTIVE);
      new_browser->window()->Show();
      return raw_new_tab;
    }
    default:
      browser->window()->GetLocationBar()->Revert();
      return current_tab;
  }
}

// Like the above, but auto-computes the current tab
WebContents* GetTabAndRevertIfNecessary(Browser* browser,
                                        WindowOpenDisposition disposition) {
  WebContents* activate_tab =
      browser->tab_strip_model()->GetActiveWebContents();
  return GetTabAndRevertIfNecessaryHelper(browser, disposition, activate_tab);
}

void ReloadInternal(Browser* browser,
                    WindowOpenDisposition disposition,
                    bool bypass_cache) {
  const WebContents* active_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  const auto& selected_indices =
      browser->tab_strip_model()->selection_model().selected_indices();
  for (int index : selected_indices) {
    WebContents* selected_tab =
        browser->tab_strip_model()->GetWebContentsAt(index);
    WebContents* new_tab =
        GetTabAndRevertIfNecessaryHelper(browser, disposition, selected_tab);

    // If the selected_tab is the activated page, give the focus to it, as this
    // is caused by a user action
    if (selected_tab == active_contents &&
        !new_tab->FocusLocationBarByDefault()) {
      new_tab->Focus();
    }

    DevToolsWindow* devtools =
        DevToolsWindow::GetInstanceForInspectedWebContents(new_tab);
    constexpr content::ReloadType kBypassingType =
        content::ReloadType::BYPASSING_CACHE;
    constexpr content::ReloadType kNormalType = content::ReloadType::NORMAL;
    if (!devtools || !devtools->ReloadInspectedWebContents(bypass_cache)) {
      new_tab->GetController().Reload(
          bypass_cache ? kBypassingType : kNormalType, true);
    }
  }
}

bool IsShowingWebContentsModalDialog(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;

  // TODO(gbillock): This is currently called in production by the CanPrint
  // method, and may be too restrictive if we allow print preview to overlap.
  // Re-assess how to queue print preview after we know more about popup
  // management policy.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  return manager && manager->IsDialogActive();
}

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
bool PrintPreviewShowing(const Browser* browser) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  printing::PrintPreviewDialogController* controller =
      printing::PrintPreviewDialogController::GetInstance();
  return controller && (controller->GetPrintPreviewForContents(contents) ||
                        controller->is_creating_print_preview_dialog());
#else
  return false;
#endif
}
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

}  // namespace

bool IsCommandEnabled(Browser* browser, int command) {
  return browser->command_controller()->IsCommandEnabled(command);
}

bool SupportsCommand(Browser* browser, int command) {
  return browser->command_controller()->SupportsCommand(command);
}

bool ExecuteCommand(Browser* browser, int command, base::TimeTicks time_stamp) {
  return browser->command_controller()->ExecuteCommand(command, time_stamp);
}

bool ExecuteCommandWithDisposition(Browser* browser,
                                   int command,
                                   WindowOpenDisposition disposition) {
  return browser->command_controller()->ExecuteCommandWithDisposition(
      command, disposition);
}

void UpdateCommandEnabled(Browser* browser, int command, bool enabled) {
  browser->command_controller()->UpdateCommandEnabled(command, enabled);
}

void AddCommandObserver(Browser* browser,
                        int command,
                        CommandObserver* observer) {
  browser->command_controller()->AddCommandObserver(command, observer);
}

void RemoveCommandObserver(Browser* browser,
                           int command,
                           CommandObserver* observer) {
  browser->command_controller()->RemoveCommandObserver(command, observer);
}

int GetContentRestrictions(const Browser* browser) {
  int content_restrictions = 0;
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (current_tab) {
    CoreTabHelper* core_tab_helper =
        CoreTabHelper::FromWebContents(current_tab);
    content_restrictions = core_tab_helper->content_restrictions();
    NavigationEntry* last_committed_entry =
        current_tab->GetController().GetLastCommittedEntry();
    if (!content::IsSavableURL(
            last_committed_entry ? last_committed_entry->GetURL() : GURL()) ||
        current_tab->ShowingInterstitialPage())
      content_restrictions |= CONTENT_RESTRICTION_SAVE;
    if (current_tab->ShowingInterstitialPage())
      content_restrictions |= CONTENT_RESTRICTION_PRINT;
  }
  return content_restrictions;
}

void NewEmptyWindow(Profile* profile) {
  bool incognito = profile->IsOffTheRecord();
  PrefService* prefs = profile->GetPrefs();
  if (incognito) {
    if (IncognitoModePrefs::GetAvailability(prefs) ==
        IncognitoModePrefs::DISABLED) {
      incognito = false;
    }
  } else if (profile->IsGuestSession() ||
             (browser_defaults::kAlwaysOpenIncognitoWindow &&
              IncognitoModePrefs::ShouldLaunchIncognito(
                  *base::CommandLine::ForCurrentProcess(), prefs))) {
    incognito = true;
  }

  if (incognito) {
    base::RecordAction(UserMetricsAction("NewIncognitoWindow"));
    OpenEmptyWindow(profile->GetOffTheRecordProfile());
  } else {
    base::RecordAction(UserMetricsAction("NewWindow"));
    SessionService* session_service =
        SessionServiceFactory::GetForProfileForSessionRestore(
            profile->GetOriginalProfile());
    if (!session_service ||
        !session_service->RestoreIfNecessary(std::vector<GURL>())) {
      OpenEmptyWindow(profile->GetOriginalProfile());
    }
  }
}

Browser* OpenEmptyWindow(Profile* profile) {
  Browser* browser =
      new Browser(Browser::CreateParams(Browser::TYPE_NORMAL, profile, true));
  AddTabAt(browser, GURL(), -1, true);
  browser->window()->Show();
  return browser;
}

void OpenWindowWithRestoredTabs(Profile* profile) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  if (service)
    service->RestoreMostRecentEntry(nullptr);
}

void OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  ScopedTabbedBrowserDisplayer displayer(profile->GetOffTheRecordProfile());
  AddSelectedTabWithURL(displayer.browser(), url, ui::PAGE_TRANSITION_LINK);
}

bool CanGoBack(const Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .CanGoBack();
}

void GoBack(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Back"));

  if (CanGoBack(browser)) {
    WebContents* current_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    WebContents* new_tab = GetTabAndRevertIfNecessary(browser, disposition);
    // If we are on an interstitial page and clone the tab, it won't be copied
    // to the new tab, so we don't need to go back.
    if ((new_tab == current_tab) || !current_tab->ShowingInterstitialPage())
      new_tab->GetController().GoBack();
  }
}

bool CanGoForward(const Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .CanGoForward();
}

void GoForward(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Forward"));
  if (CanGoForward(browser)) {
    GetTabAndRevertIfNecessary(browser, disposition)
        ->GetController()
        .GoForward();
  }
}

void NavigateToIndexWithDisposition(Browser* browser,
                                    int index,
                                    WindowOpenDisposition disposition) {
  NavigationController* controller =
      &GetTabAndRevertIfNecessary(browser, disposition)->GetController();
  DCHECK_GE(index, 0);
  DCHECK_LT(index, controller->GetEntryCount());
  controller->GoToIndex(index);
}

void Reload(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Reload"));
  ReloadInternal(browser, disposition, false);
}

void ReloadBypassingCache(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("ReloadBypassingCache"));
  ReloadInternal(browser, disposition, true);
}

bool CanReload(const Browser* browser) {
  return !browser->is_type_devtools();
}

void Home(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Home"));

  std::string extra_headers;
#if BUILDFLAG(ENABLE_RLZ)
  // If the home page is a Google home page, add the RLZ header to the request.
  PrefService* pref_service = browser->profile()->GetPrefs();
  if (pref_service) {
    if (google_util::IsGoogleHomePageUrl(
            GURL(pref_service->GetString(prefs::kHomePage)))) {
      extra_headers = rlz::RLZTracker::GetAccessPointHttpHeader(
          rlz::RLZTracker::ChromeHomePage());
    }
  }
#endif  // BUILDFLAG(ENABLE_RLZ)

  GURL url = browser->profile()->GetHomePage();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // With bookmark apps enabled, hosted apps should return to their launch page
  // when the home button is pressed.
  if (browser->deprecated_is_app()) {
    const extensions::Extension* extension = GetExtensionForBrowser(browser);
    if (!extension)
      return;
    url = extensions::AppLaunchInfo::GetLaunchWebURL(extension);
  }

  if (disposition == WindowOpenDisposition::CURRENT_TAB ||
      disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB)
    extensions::MaybeShowExtensionControlledHomeNotification(browser);
#endif

  bool is_chrome_internal = url.SchemeIs(url::kAboutScheme) ||
                            url.SchemeIs(content::kChromeUIScheme) ||
                            url.SchemeIs(chrome::kChromeNativeScheme);
  base::UmaHistogramBoolean("Navigation.Home.IsChromeInternal",
                            is_chrome_internal);
  OpenURLParams params(
      url, Referrer(), disposition,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_BOOKMARK |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false);
  params.extra_headers = extra_headers;
  browser->OpenURL(params);
}

void OpenCurrentURL(Browser* browser) {
  base::RecordAction(UserMetricsAction("LoadURL"));
  LocationBar* location_bar = browser->window()->GetLocationBar();
  if (!location_bar)
    return;

  GURL url(location_bar->GetDestinationURL());

  NavigateParams params(browser, url, location_bar->GetPageTransition());
  params.disposition = location_bar->GetWindowOpenDisposition();
  // Use ADD_INHERIT_OPENER so that all pages opened by the omnibox at least
  // inherit the opener. In some cases the tabstrip will determine the group
  // should be inherited, in which case the group is inherited instead of the
  // opener.
  params.tabstrip_add_types =
      TabStripModel::ADD_FORCE_INDEX | TabStripModel::ADD_INHERIT_OPENER;
  params.input_start = location_bar->GetMatchSelectionTimestamp();
  Navigate(&params);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK(extensions::ExtensionSystem::Get(browser->profile())
             ->extension_service());
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser->profile())
          ->enabled_extensions()
          .GetAppByURL(url);
  if (extension) {
    extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_OMNIBOX_LOCATION,
                                    extension->GetType());
  }
#endif
}

void Stop(Browser* browser) {
  base::RecordAction(UserMetricsAction("Stop"));
  browser->tab_strip_model()->GetActiveWebContents()->Stop();
}

void NewWindow(Browser* browser) {
#if BUILDFLAG(ENABLE_EXTENSIONS) && defined(OS_MACOSX)
  // With bookmark apps enabled, hosted apps should open a window to their
  // launch page.
  const extensions::Extension* extension = GetExtensionForBrowser(browser);
  if (extension && extension->is_hosted_app()) {
    const auto app_launch_params = CreateAppLaunchParamsUserContainer(
        browser->profile(), extension, WindowOpenDisposition::NEW_WINDOW,
        extensions::AppLaunchSource::kSourceKeyboard);
    OpenApplicationWindow(
        browser->profile(), app_launch_params,
        extensions::AppLaunchInfo::GetLaunchWebURL(extension));
    return;
  }
#endif
  NewEmptyWindow(browser->profile()->GetOriginalProfile());
}

void NewIncognitoWindow(Profile* profile) {
#if BUILDFLAG(ENABLE_LEGACY_DESKTOP_IN_PRODUCT_HELP)
  feature_engagement::IncognitoWindowTrackerFactory::GetInstance()
      ->GetForProfile(profile)
      ->OnIncognitoWindowOpened();
#endif
  NewEmptyWindow(profile->GetOffTheRecordProfile());
}

void CloseWindow(Browser* browser) {
  base::RecordAction(UserMetricsAction("CloseWindow"));
  browser->window()->Close();
}

void NewTab(Browser* browser) {
  base::RecordAction(UserMetricsAction("NewTab"));
  // TODO(asvitkine): This is invoked programmatically from several places.
  // Audit the code and change it so that the histogram only gets collected for
  // user-initiated commands.
  UMA_HISTOGRAM_ENUMERATION("Tab.NewTab", TabStripModel::NEW_TAB_COMMAND,
                            TabStripModel::NEW_TAB_ENUM_COUNT);

  // Notify IPH that new tab was opened.
  auto* reopen_tab_iph =
      ReopenTabInProductHelpFactory::GetForProfile(browser->profile());
  reopen_tab_iph->NewTabOpened();

  if (browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP)) {
    TabStripModel* const model = browser->tab_strip_model();
    const auto group_id = model->GetTabGroupForTab(model->active_index());
    AddTabAt(browser, GURL(), -1, true, group_id);
    model->GetActiveWebContents()->RestoreFocus();
  } else {
    ScopedTabbedBrowserDisplayer displayer(browser->profile());
    Browser* b = displayer.browser();
    AddTabAt(b, GURL(), -1, true);
    b->window()->Show();
    // The call to AddBlankTabAt above did not set the focus to the tab as its
    // window was not active, so we have to do it explicitly.
    // See http://crbug.com/6380.
    b->tab_strip_model()->GetActiveWebContents()->RestoreFocus();
  }
}

void CloseTab(Browser* browser) {
  base::RecordAction(UserMetricsAction("CloseTab_Accelerator"));
  browser->tab_strip_model()->CloseSelectedTabs();
}

bool CanZoomIn(content::WebContents* contents) {
  return contents && !contents->IsCrashed() &&
         zoom::ZoomController::FromWebContents(contents)->GetZoomPercent() !=
             contents->GetMaximumZoomPercent();
}

bool CanZoomOut(content::WebContents* contents) {
  return contents && !contents->IsCrashed() &&
         zoom::ZoomController::FromWebContents(contents)->GetZoomPercent() !=
             contents->GetMinimumZoomPercent();
}

bool CanResetZoom(content::WebContents* contents) {
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(contents);
  return !zoom_controller->IsAtDefaultZoom() ||
         !zoom_controller->PageScaleFactorIsOne();
}

void SelectNextTab(Browser* browser,
                   TabStripModel::UserGestureDetails gesture_detail) {
  base::RecordAction(UserMetricsAction("SelectNextTab"));
  browser->tab_strip_model()->SelectNextTab(gesture_detail);
}

void SelectPreviousTab(Browser* browser,
                       TabStripModel::UserGestureDetails gesture_detail) {
  base::RecordAction(UserMetricsAction("SelectPrevTab"));
  browser->tab_strip_model()->SelectPreviousTab(gesture_detail);
}

void MoveTabNext(Browser* browser) {
  base::RecordAction(UserMetricsAction("MoveTabNext"));
  browser->tab_strip_model()->MoveTabNext();
}

void MoveTabPrevious(Browser* browser) {
  base::RecordAction(UserMetricsAction("MoveTabPrevious"));
  browser->tab_strip_model()->MoveTabPrevious();
}

void SelectNumberedTab(Browser* browser,
                       int index,
                       TabStripModel::UserGestureDetails gesture_detail) {
  if (index < browser->tab_strip_model()->count()) {
    base::RecordAction(UserMetricsAction("SelectNumberedTab"));
    browser->tab_strip_model()->ActivateTabAt(index, gesture_detail);
  }
}

void SelectLastTab(Browser* browser,
                   TabStripModel::UserGestureDetails gesture_detail) {
  base::RecordAction(UserMetricsAction("SelectLastTab"));
  browser->tab_strip_model()->SelectLastTab(gesture_detail);
}

void DuplicateTab(Browser* browser) {
  base::RecordAction(UserMetricsAction("Duplicate"));
  DuplicateTabAt(browser, browser->tab_strip_model()->active_index());
}

bool CanDuplicateTab(const Browser* browser) {
  return CanDuplicateTabAt(browser, browser->tab_strip_model()->active_index());
}

bool CanDuplicateKeyboardFocusedTab(const Browser* browser) {
  if (!HasKeyboardFocusedTab(browser))
    return false;
  return CanDuplicateTabAt(browser, *GetKeyboardFocusedTabIndex(browser));
}

bool CanCloseTabsToRight(const Browser* browser) {
  return browser->tab_strip_model()->IsContextMenuCommandEnabled(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandCloseTabsToRight);
}

bool CanCloseOtherTabs(const Browser* browser) {
  return browser->tab_strip_model()->IsContextMenuCommandEnabled(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandCloseOtherTabs);
}

WebContents* DuplicateTabAt(Browser* browser, int index) {
  WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(index);
  CHECK(contents);
  std::unique_ptr<WebContents> contents_dupe = contents->Clone();
  WebContents* raw_contents_dupe = contents_dupe.get();

  bool pinned = false;
  if (browser->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP)) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    const int index =
        browser->tab_strip_model()->GetIndexOfWebContents(contents);
    pinned = browser->tab_strip_model()->IsTabPinned(index);
    int add_types = TabStripModel::ADD_ACTIVE |
                    TabStripModel::ADD_INHERIT_OPENER |
                    (pinned ? TabStripModel::ADD_PINNED : 0);
    const auto old_group = browser->tab_strip_model()->GetTabGroupForTab(index);
    browser->tab_strip_model()->InsertWebContentsAt(
        index + 1, std::move(contents_dupe), add_types, old_group);
  } else {
    Browser* new_browser = NULL;
    if (browser->deprecated_is_app()) {
      new_browser = new Browser(Browser::CreateParams::CreateForApp(
          browser->app_name(), browser->is_trusted_source(), gfx::Rect(),
          browser->profile(), true));
    } else {
      new_browser = new Browser(
          Browser::CreateParams(browser->type(), browser->profile(), true));
    }
    // Preserve the size of the original window. The new window has already
    // been given an offset by the OS, so we shouldn't copy the old bounds.
    BrowserWindow* new_window = new_browser->window();
    new_window->SetBounds(
        gfx::Rect(new_window->GetRestoredBounds().origin(),
                  browser->window()->GetRestoredBounds().size()));

    // We need to show the browser now.  Otherwise ContainerWin assumes the
    // WebContents is invisible and won't size it.
    new_browser->window()->Show();

    // The page transition below is only for the purpose of inserting the tab.
    new_browser->tab_strip_model()->AddWebContents(std::move(contents_dupe), -1,
                                                   ui::PAGE_TRANSITION_LINK,
                                                   TabStripModel::ADD_ACTIVE);
  }

  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(browser->profile());
  if (session_service)
    session_service->TabRestored(raw_contents_dupe, pinned);
  return raw_contents_dupe;
}

bool CanDuplicateTabAt(const Browser* browser, int index) {
  WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(index);
  // If an interstitial is showing, do not allow tab duplication, since
  // the last committed entry is what would get duplicated and is not
  // what the user expects to duplicate.
  return contents && !contents->ShowingInterstitialPage() &&
         contents->GetController().GetLastCommittedEntry();
}

void PinTab(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::ContextMenuCommand::CommandTogglePinned);
}

void MuteSite(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::ContextMenuCommand::CommandToggleSiteMuted);
}

void MuteSiteForKeyboardFocusedTab(Browser* browser) {
  if (!HasKeyboardFocusedTab(browser))
    return;
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      *GetKeyboardFocusedTabIndex(browser),
      TabStripModel::ContextMenuCommand::CommandToggleSiteMuted);
}

void PinKeyboardFocusedTab(Browser* browser) {
  if (!HasKeyboardFocusedTab(browser))
    return;
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      *GetKeyboardFocusedTabIndex(browser),
      TabStripModel::ContextMenuCommand::CommandTogglePinned);
}

void DuplicateKeyboardFocusedTab(Browser* browser) {
  if (HasKeyboardFocusedTab(browser)) {
    DuplicateTabAt(browser, *GetKeyboardFocusedTabIndex(browser));
  }
}

bool HasKeyboardFocusedTab(const Browser* browser) {
  return GetKeyboardFocusedTabIndex(browser).has_value();
}

void ConvertPopupToTabbedBrowser(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowAsTab"));
  TabStripModel* tab_strip = browser->tab_strip_model();
  std::unique_ptr<content::WebContents> contents =
      tab_strip->DetachWebContentsAt(tab_strip->active_index());
  Browser* b = new Browser(Browser::CreateParams(browser->profile(), true));
  b->tab_strip_model()->AppendWebContents(std::move(contents), true);
  b->window()->Show();
}

void CloseTabsToRight(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandCloseTabsToRight);
}

void CloseOtherTabs(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandCloseOtherTabs);
}

void Exit() {
  base::RecordAction(UserMetricsAction("Exit"));
  chrome::AttemptUserExit();
}

void BookmarkCurrentTabIgnoringExtensionOverrides(Browser* browser) {
  base::RecordAction(UserMetricsAction("Star"));

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  if (!model || !model->loaded())
    return;  // Ignore requests until bookmarks are loaded.

  GURL url;
  base::string16 title;
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // |web_contents| can be nullptr if the last tab in the browser was closed
  // but the browser wasn't closed yet. https://crbug.com/799668
  if (!web_contents)
    return;
  GetURLAndTitleToBookmark(web_contents, &url, &title);
  bool is_bookmarked_by_any = model->IsBookmarked(url);
  if (!is_bookmarked_by_any &&
      web_contents->GetBrowserContext()->IsOffTheRecord()) {
    // If we're incognito the favicon may not have been saved. Save it now
    // so that bookmarks have an icon for the page.
    favicon::ContentFaviconDriver::FromWebContents(web_contents)
        ->SaveFaviconEvenIfInIncognito();
  }
  bool was_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
  bookmarks::AddIfNotBookmarked(model, url, title);
  bool is_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
  // Make sure the model actually added a bookmark before showing the star. A
  // bookmark isn't created if the url is invalid.
  if (browser->window()->IsActive() && is_bookmarked_by_user) {
    // Only show the bubble if the window is active, otherwise we may get into
    // weird situations where the bubble is deleted as soon as it is shown.
    browser->window()->ShowBookmarkBubble(url, was_bookmarked_by_user);
  }

  if (!was_bookmarked_by_user && is_bookmarked_by_user)
    RecordBookmarksAdded(browser->profile());
}

void BookmarkCurrentTabAllowingExtensionOverrides(Browser* browser) {
  DCHECK(!chrome::ShouldRemoveBookmarkThisTabUI(browser->profile()));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  const extensions::Extension* extension = NULL;
  extensions::Command command;
  if (GetBookmarkOverrideCommand(browser->profile(), &extension, &command)) {
    switch (command.type()) {
      case extensions::Command::Type::kNamed:
        browser->window()->ExecuteExtensionCommand(extension, command);
        break;
      case extensions::Command::Type::kBrowserAction:
      case extensions::Command::Type::kPageAction:
        // BookmarkCurrentTab is called through a user gesture, so it is safe
        // to grant the active tab permission.
        extensions::ExtensionActionAPI::Get(browser->profile())
            ->ShowExtensionActionPopup(extension, browser, true);
        break;
    }
    return;
  }
#endif
  BookmarkCurrentTabIgnoringExtensionOverrides(browser);
}

bool CanBookmarkCurrentTab(const Browser* browser) {
  return CanBookmarkCurrentTabInternal(browser, true);
}

void BookmarkAllTabs(Browser* browser) {
  base::RecordAction(UserMetricsAction("BookmarkAllTabs"));
  RecordBookmarkAllTabsWithTabsCount(browser->profile(),
                                     browser->tab_strip_model()->count());
  // We record the profile that invoked this option.
  RecordBookmarksAdded(browser->profile());
  chrome::ShowBookmarkAllTabsDialog(browser);
}

bool CanBookmarkAllTabs(const Browser* browser) {
  return browser->tab_strip_model()->count() > 1 &&
         !chrome::ShouldRemoveBookmarkAllTabsUI(browser->profile()) &&
         CanBookmarkCurrentTabInternal(browser, false);
}

void SaveCreditCard(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents);
  controller->ReshowBubble();
}

void MigrateLocalCards(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::ManageMigrationUiController* controller =
      autofill::ManageMigrationUiController::FromWebContents(web_contents);
  // Show migration-related Ui when the user clicks the credit card icon.
  controller->OnUserClickedCreditCardIcon();
}

void MaybeShowSaveLocalCardSignInPromo(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::SaveCardBubbleControllerImpl::CreateForWebContents(web_contents);
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents);

  // The sign in promo will only be shown when 1) The user is signed out or 2)
  // The user is signed in through DICe, but did not turn on syncing. Otherwise
  // early returns.
  controller->MaybeShowBubbleForSignInPromo();
}

void CloseSaveLocalCardSignInPromo(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents);

  if (controller)
    controller->HideBubbleForSignInPromo();
}

void Translate(Browser* browser) {
  if (!browser->window()->IsActive())
    return;

  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(web_contents);

  std::string source_language;
  std::string target_language;
  chrome_translate_client->GetTranslateLanguages(web_contents, &source_language,
                                                 &target_language);

  translate::TranslateStep step = translate::TRANSLATE_STEP_BEFORE_TRANSLATE;
  if (chrome_translate_client) {
    if (chrome_translate_client->GetLanguageState().translation_pending())
      step = translate::TRANSLATE_STEP_TRANSLATING;
    else if (chrome_translate_client->GetLanguageState().translation_error())
      step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;
    else if (chrome_translate_client->GetLanguageState().IsPageTranslated())
      step = translate::TRANSLATE_STEP_AFTER_TRANSLATE;
  }
  ShowTranslateBubbleResult result = browser->window()->ShowTranslateBubble(
      web_contents, step, source_language, target_language,
      translate::TranslateErrors::NONE, true);
  if (result != ShowTranslateBubbleResult::SUCCESS)
    translate::ReportUiAction(TranslateBubbleResultToUiEvent(result));
}

void ManagePasswordsForPage(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_contents);
  TabDialogs::FromWebContents(web_contents)
      ->ShowManagePasswordsBubble(!controller->IsAutomaticallyOpeningBubble());
}

void SendTabToSelfFromPageAction(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  send_tab_to_self::SendTabToSelfBubbleController* controller =
      send_tab_to_self::SendTabToSelfBubbleController::
          CreateOrGetFromWebContents(web_contents);
  controller->ShowBubble();
}

void SavePage(Browser* browser) {
  base::RecordAction(UserMetricsAction("SavePage"));
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  DCHECK(current_tab);
  if (current_tab->GetContentsMimeType() == "application/pdf")
    base::RecordAction(UserMetricsAction("PDF.SavePage"));
  current_tab->OnSavePage();
}

bool CanSavePage(const Browser* browser) {
  // LocalState can be NULL in tests.
  if (g_browser_process->local_state() &&
      !g_browser_process->local_state()->GetBoolean(
          prefs::kAllowFileSelectionDialogs)) {
    return false;
  }
  return !browser->is_type_devtools() &&
         !(GetContentRestrictions(browser) & CONTENT_RESTRICTION_SAVE);
}

void Print(Browser* browser) {
#if BUILDFLAG(ENABLE_PRINTING)
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  printing::StartPrint(
      web_contents, nullptr /* print_renderer */,
      browser->profile()->GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled),
      false /* has_selection? */);
#endif
}

bool CanPrint(Browser* browser) {
  // Do not print when printing is disabled via pref or policy.
  // Do not print when a page has crashed.
  // Do not print when a constrained window is showing. It's confusing.
  // TODO(gbillock): Need to re-assess the call to
  // IsShowingWebContentsModalDialog after a popup management policy is
  // refined -- we will probably want to just queue the print request, not
  // block it.
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  return browser->profile()->GetPrefs()->GetBoolean(prefs::kPrintingEnabled) &&
         (current_tab && !current_tab->IsCrashed()) &&
         !(IsShowingWebContentsModalDialog(browser) ||
           GetContentRestrictions(browser) & CONTENT_RESTRICTION_PRINT);
}

#if BUILDFLAG(ENABLE_PRINTING)
void BasicPrint(Browser* browser) {
  printing::StartBasicPrint(browser->tab_strip_model()->GetActiveWebContents());
}

bool CanBasicPrint(Browser* browser) {
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  // If printing is not disabled via pref or policy, it is always possible to
  // advanced print when the print preview is visible.
  return browser->profile()->GetPrefs()->GetBoolean(prefs::kPrintingEnabled) &&
         (PrintPreviewShowing(browser) || CanPrint(browser));
#else
  return false;  // The print dialog is disabled.
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
}
#endif  // BUILDFLAG(ENABLE_PRINTING)

bool CanRouteMedia(Browser* browser) {
  // Do not allow user to open Media Router dialog when there is already an
  // active modal dialog. This avoids overlapping dialogs.
  return media_router::MediaRouterEnabled(browser->profile()) &&
         !IsShowingWebContentsModalDialog(browser);
}

void RouteMedia(Browser* browser) {
  DCHECK(CanRouteMedia(browser));

  media_router::MediaRouterDialogController* dialog_controller =
      media_router::MediaRouterDialogController::GetOrCreateForWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  if (!dialog_controller)
    return;

  dialog_controller->ShowMediaRouterDialog();
}

void EmailPageLocation(Browser* browser) {
  base::RecordAction(UserMetricsAction("EmailPageLocation"));
  WebContents* wc = browser->tab_strip_model()->GetActiveWebContents();
  DCHECK(wc);

  std::string title =
      net::EscapeQueryParamValue(base::UTF16ToUTF8(wc->GetTitle()), false);
  std::string page_url = net::EscapeQueryParamValue(wc->GetURL().spec(), false);
  std::string mailto = std::string("mailto:?subject=Fwd:%20") + title +
                       "&body=%0A%0A" + page_url;
  platform_util::OpenExternal(browser->profile(), GURL(mailto));
}

bool CanEmailPageLocation(const Browser* browser) {
  return browser->location_bar_model()->ShouldDisplayURL() &&
         browser->tab_strip_model()
             ->GetActiveWebContents()
             ->GetURL()
             .is_valid();
}

void CutCopyPaste(Browser* browser, int command_id) {
  if (command_id == IDC_CUT)
    base::RecordAction(UserMetricsAction("Cut"));
  else if (command_id == IDC_COPY)
    base::RecordAction(UserMetricsAction("Copy"));
  else
    base::RecordAction(UserMetricsAction("Paste"));
  browser->window()->CutCopyPaste(command_id);
}

void Find(Browser* browser) {
  base::RecordAction(UserMetricsAction("Find"));
  FindInPage(browser, false, true);
}

void FindNext(Browser* browser) {
  base::RecordAction(UserMetricsAction("FindNext"));
  FindInPage(browser, true, true);
}

void FindPrevious(Browser* browser) {
  base::RecordAction(UserMetricsAction("FindPrevious"));
  FindInPage(browser, true, false);
}

void FindInPage(Browser* browser, bool find_next, bool forward_direction) {
  browser->GetFindBarController()->Show(find_next, forward_direction);
}

bool CanCloseFind(Browser* browser) {
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (!current_tab)
    return false;

  FindTabHelper* find_helper = FindTabHelper::FromWebContents(current_tab);
  return find_helper ? find_helper->find_ui_active() : false;
}

void CloseFind(Browser* browser) {
  browser->GetFindBarController()->EndFindSession(
      FindOnPageSelectionAction::kKeep, FindBoxResultAction::kKeep);
}

void Zoom(Browser* browser, content::PageZoom zoom) {
  zoom::PageZoom::Zoom(browser->tab_strip_model()->GetActiveWebContents(),
                       zoom);
}

void FocusToolbar(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusToolbar"));
  browser->window()->FocusToolbar();
}

void FocusLocationBar(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusLocation"));
  browser->window()->SetFocusToLocationBar(true);
}

void FocusSearch(Browser* browser) {
  // TODO(beng): replace this with FocusLocationBar
  base::RecordAction(UserMetricsAction("FocusSearch"));
  browser->window()->GetLocationBar()->FocusSearch();
}

void FocusAppMenu(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusAppMenu"));
  browser->window()->FocusAppMenu();
}

void FocusBookmarksToolbar(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusBookmarksToolbar"));
  browser->window()->FocusBookmarksToolbar();
}

void FocusInactivePopupForAccessibility(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusInactivePopupForAccessibility"));
  browser->window()->FocusInactivePopupForAccessibility();
}

void FocusNextPane(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusNextPane"));
  browser->window()->RotatePaneFocus(true);
}

void FocusPreviousPane(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusPreviousPane"));
  browser->window()->RotatePaneFocus(false);
}

void ToggleDevToolsWindow(Browser* browser, DevToolsToggleAction action) {
  if (action.type() == DevToolsToggleAction::kShowConsolePanel)
    base::RecordAction(UserMetricsAction("DevTools_ToggleConsole"));
  else
    base::RecordAction(UserMetricsAction("DevTools_ToggleWindow"));
  DevToolsWindow::ToggleDevToolsWindow(browser, action);
}

bool CanOpenTaskManager() {
#if !defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

void OpenTaskManager(Browser* browser) {
#if !defined(OS_ANDROID)
  base::RecordAction(UserMetricsAction("TaskManager"));
  chrome::ShowTaskManager(browser);
#else
  NOTREACHED();
#endif
}

void OpenFeedbackDialog(Browser* browser, FeedbackSource source) {
  base::RecordAction(UserMetricsAction("Feedback"));
  chrome::ShowFeedbackPage(
      browser, source, std::string() /* description_template */,
      std::string() /* description_placeholder_text */,
      std::string() /* category_tag */, std::string() /* extra_diagnostics */);
}

void ToggleBookmarkBar(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowBookmarksBar"));
  ToggleBookmarkBarWhenVisible(browser->profile());
}

void ShowAppMenu(Browser* browser) {
  // We record the user metric for this event in AppMenu::RunMenu.
  browser->window()->ShowAppMenu();
}

void ShowAvatarMenu(Browser* browser) {
  browser->window()->ShowAvatarBubbleFromAvatarButton(
      BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT,
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN, true);
}

void OpenUpdateChromeDialog(Browser* browser) {
  if (UpgradeDetector::GetInstance()->is_outdated_install()) {
    UpgradeDetector::GetInstance()->NotifyOutdatedInstall();
  } else if (UpgradeDetector::GetInstance()->is_outdated_install_no_au()) {
    UpgradeDetector::GetInstance()->NotifyOutdatedInstallNoAutoUpdate();
  } else {
    base::RecordAction(UserMetricsAction("UpdateChrome"));
    browser->window()->ShowUpdateChromeDialog();
  }
}

void ToggleDistilledView(Browser* browser) {
  auto* current_web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (dom_distiller::url_utils::IsDistilledPage(
          current_web_contents->GetLastCommittedURL())) {
    ReturnToOriginalPage(current_web_contents);
  } else {
    DistillCurrentPageAndView(current_web_contents);
  }
}

bool CanRequestTabletSite(WebContents* current_tab) {
  return current_tab &&
         current_tab->GetController().GetLastCommittedEntry() != NULL;
}

bool IsRequestingTabletSite(Browser* browser) {
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (!current_tab)
    return false;
  content::NavigationEntry* entry =
      current_tab->GetController().GetLastCommittedEntry();
  if (!entry)
    return false;
  return entry->GetIsOverridingUserAgent();
}

void ToggleRequestTabletSite(Browser* browser) {
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (!current_tab)
    return;
  NavigationController& controller = current_tab->GetController();
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  if (!entry)
    return;
  if (entry->GetIsOverridingUserAgent())
    entry->SetIsOverridingUserAgent(false);
  else
    SetAndroidOsForTabletSite(current_tab);
  controller.Reload(content::ReloadType::ORIGINAL_REQUEST_URL, true);
}

void SetAndroidOsForTabletSite(content::WebContents* current_tab) {
  DCHECK(current_tab);
  NavigationEntry* entry = current_tab->GetController().GetLastCommittedEntry();
  if (entry) {
    entry->SetIsOverridingUserAgent(true);
    std::string product =
        version_info::GetProductNameAndVersionForUserAgent() + " Mobile";
    current_tab->SetUserAgentOverride(content::BuildUserAgentFromOSAndProduct(
                                          kOsOverrideForTabletSite, product),
                                      false);
  }
}

void ToggleFullscreenMode(Browser* browser) {
  DCHECK(browser);
  browser->exclusive_access_manager()
      ->fullscreen_controller()
      ->ToggleBrowserFullscreenMode();
}

void ClearCache(Browser* browser) {
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser->profile());
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  // BrowsingDataRemover takes care of deleting itself when done.
}

bool IsDebuggerAttachedToCurrentTab(Browser* browser) {
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  return contents ? content::DevToolsAgentHost::IsDebuggerAttached(contents)
                  : false;
}

void CopyURL(Browser* browser) {
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteText(base::UTF8ToUTF16(browser->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetVisibleURL()
                                      .spec()));
}

Browser* OpenInChrome(Browser* hosted_app_browser) {
  DCHECK(hosted_app_browser->app_controller());
  // Find a non-incognito browser.
  Browser* target_browser =
      chrome::FindTabbedBrowser(hosted_app_browser->profile(), false);

  if (!target_browser) {
    target_browser =
        new Browser(Browser::CreateParams(hosted_app_browser->profile(), true));
  }

  TabStripModel* source_tabstrip = hosted_app_browser->tab_strip_model();
  target_browser->tab_strip_model()->AppendWebContents(
      source_tabstrip->DetachWebContentsAt(source_tabstrip->active_index()),
      true);
  target_browser->window()->Show();
  return target_browser;
}

bool CanViewSource(const Browser* browser) {
  return !browser->is_type_devtools() && browser->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetController()
                                             .CanViewSource();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void CreateBookmarkAppFromCurrentWebContents(Browser* browser,
                                             bool force_shortcut_app) {
  // TODO(alancutter): Legacy metric to remove in ~M80.
  base::RecordAction(UserMetricsAction("CreateHostedApp"));
  web_app::CreateWebAppFromCurrentWebContents(browser, force_shortcut_app,
                                              base::DoNothing());
}

bool CanCreateBookmarkApp(const Browser* browser) {
  return web_app::CanCreateWebApp(browser);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !defined(TOOLKIT_VIEWS)
base::Optional<int> GetKeyboardFocusedTabIndex(const Browser* browser) {
  return base::nullopt;
}
#endif

}  // namespace chrome
