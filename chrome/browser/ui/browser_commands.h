// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_COMMANDS_H_
#define CHROME_BROWSER_UI_BROWSER_COMMANDS_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "content/public/common/page_zoom.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class CommandObserver;
class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace chrome {

// For all commands, where a tab is not specified, the active tab is assumed.

bool IsCommandEnabled(Browser* browser, int command);
bool SupportsCommand(Browser* browser, int command);
bool ExecuteCommand(Browser* browser,
                    int command,
                    base::TimeTicks time_stamp = base::TimeTicks::Now());
bool ExecuteCommandWithDisposition(Browser* browser,
                                   int command,
                                   WindowOpenDisposition disposition);
void UpdateCommandEnabled(Browser* browser, int command, bool enabled);
void AddCommandObserver(Browser*, int command, CommandObserver* observer);
void RemoveCommandObserver(Browser*, int command, CommandObserver* observer);

int GetContentRestrictions(const Browser* browser);

// Opens a new window with the default blank tab.
void NewEmptyWindow(Profile* profile);

// Opens a new window with the default blank tab. This bypasses metrics and
// various internal bookkeeping; NewEmptyWindow (above) is preferred.
Browser* OpenEmptyWindow(Profile* profile);

// Opens a new window with the tabs from |profile|'s TabRestoreService.
void OpenWindowWithRestoredTabs(Profile* profile);

// Opens the specified URL in a new browser window in an incognito session. If
// there is already an existing active incognito session for the specified
// |profile|, that session is re- used.
void OpenURLOffTheRecord(Profile* profile, const GURL& url);

bool CanGoBack(const Browser* browser);
void GoBack(Browser* browser, WindowOpenDisposition disposition);
bool CanGoForward(const Browser* browser);
void GoForward(Browser* browser, WindowOpenDisposition disposition);
void NavigateToIndexWithDisposition(Browser* browser,
                                    int index,
                                    WindowOpenDisposition disposition);
void Reload(Browser* browser, WindowOpenDisposition disposition);
void ReloadBypassingCache(Browser* browser, WindowOpenDisposition disposition);
bool CanReload(const Browser* browser);
void Home(Browser* browser, WindowOpenDisposition disposition);
void OpenCurrentURL(Browser* browser);
void Stop(Browser* browser);
void NewWindow(Browser* browser);
void NewIncognitoWindow(Profile* profile);
void CloseWindow(Browser* browser);
void NewTab(Browser* browser);
void CloseTab(Browser* browser);
bool CanZoomIn(content::WebContents* contents);
bool CanZoomOut(content::WebContents* contents);
bool CanResetZoom(content::WebContents* contents);
void RestoreTab(Browser* browser);
void SelectNextTab(
    Browser* browser,
    TabStripModel::UserGestureDetails gesture_detail =
        TabStripModel::UserGestureDetails(TabStripModel::GestureType::kOther));
void SelectPreviousTab(
    Browser* browser,
    TabStripModel::UserGestureDetails gesture_detail =
        TabStripModel::UserGestureDetails(TabStripModel::GestureType::kOther));
void MoveTabNext(Browser* browser);
void MoveTabPrevious(Browser* browser);
void SelectNumberedTab(
    Browser* browser,
    int index,
    TabStripModel::UserGestureDetails gesture_detail =
        TabStripModel::UserGestureDetails(TabStripModel::GestureType::kOther));
void SelectLastTab(
    Browser* browser,
    TabStripModel::UserGestureDetails gesture_detail =
        TabStripModel::UserGestureDetails(TabStripModel::GestureType::kOther));
void DuplicateTab(Browser* browser);
bool CanDuplicateTab(const Browser* browser);
bool CanDuplicateKeyboardFocusedTab(const Browser* browser);
bool CanCloseTabsToRight(const Browser* browser);
bool CanCloseOtherTabs(const Browser* browser);
content::WebContents* DuplicateTabAt(Browser* browser, int index);
bool CanDuplicateTabAt(const Browser* browser, int index);
void MuteSite(Browser* browser);
void PinTab(Browser* browser);
void MuteSiteForKeyboardFocusedTab(Browser* browser);
bool HasKeyboardFocusedTab(const Browser* browser);
void PinKeyboardFocusedTab(Browser* browser);
void DuplicateKeyboardFocusedTab(Browser* browser);
void ConvertPopupToTabbedBrowser(Browser* browser);
void CloseTabsToRight(Browser* browser);
void CloseOtherTabs(Browser* browser);
void Exit();
void BookmarkCurrentTabIgnoringExtensionOverrides(Browser* browser);
void BookmarkCurrentTabAllowingExtensionOverrides(Browser* browser);
bool CanBookmarkCurrentTab(const Browser* browser);
void BookmarkAllTabs(Browser* browser);
bool CanBookmarkAllTabs(const Browser* browser);
void SaveCreditCard(Browser* browser);
void MigrateLocalCards(Browser* browser);
void MaybeShowSaveLocalCardSignInPromo(Browser* browser);
void CloseSaveLocalCardSignInPromo(Browser* browser);
void Translate(Browser* browser);
void ManagePasswordsForPage(Browser* browser);
void SendTabToSelfFromPageAction(Browser* browser);
void SavePage(Browser* browser);
bool CanSavePage(const Browser* browser);
void Print(Browser* browser);
bool CanPrint(Browser* browser);
#if BUILDFLAG(ENABLE_PRINTING)
void BasicPrint(Browser* browser);
bool CanBasicPrint(Browser* browser);
#endif  // ENABLE_PRINTING
bool CanRouteMedia(Browser* browser);
void RouteMedia(Browser* browser);
void EmailPageLocation(Browser* browser);
bool CanEmailPageLocation(const Browser* browser);
void CutCopyPaste(Browser* browser, int command_id);
void Find(Browser* browser);
void FindNext(Browser* browser);
void FindPrevious(Browser* browser);
void FindInPage(Browser* browser, bool find_next, bool forward_direction);
bool CanCloseFind(Browser* browser);
void CloseFind(Browser* browser);
void Zoom(Browser* browser, content::PageZoom zoom);
void FocusToolbar(Browser* browser);
void FocusLocationBar(Browser* browser);
void FocusSearch(Browser* browser);
void FocusAppMenu(Browser* browser);
void FocusBookmarksToolbar(Browser* browser);
void FocusInactivePopupForAccessibility(Browser* browser);
void FocusNextPane(Browser* browser);
void FocusPreviousPane(Browser* browser);
void ToggleDevToolsWindow(Browser* browser, DevToolsToggleAction action);
bool CanOpenTaskManager();
void OpenTaskManager(Browser* browser);
void OpenFeedbackDialog(Browser* browser, FeedbackSource source);
void ToggleBookmarkBar(Browser* browser);
void ShowAppMenu(Browser* browser);
void ShowAvatarMenu(Browser* browser);
void OpenUpdateChromeDialog(Browser* browser);
void ToggleDistilledView(Browser* browser);
bool CanRequestTabletSite(content::WebContents* current_tab);
bool IsRequestingTabletSite(Browser* browser);
void ToggleRequestTabletSite(Browser* browser);
// Overwrite the user agent's OS with Android OS so that the web content is
// using its mobile version layout. Note it won't take effect until the web
// contents is reloaded.
void SetAndroidOsForTabletSite(content::WebContents* current_tab);
void ToggleFullscreenMode(Browser* browser);
void ClearCache(Browser* browser);
bool IsDebuggerAttachedToCurrentTab(Browser* browser);
void CopyURL(Browser* browser);
// Moves the WebContents of a hosted app Browser to a tabbed Browser. Returns
// the tabbed Browser.
Browser* OpenInChrome(Browser* hosted_app_browser);
bool CanViewSource(const Browser* browser);

// Initiates user flow for creating a bookmark app for the current page.
// Will install a PWA hosted app if the site meets installability requirements
// (see |AppBannerManager::PerformInstallableCheck|) unless |force_shortcut_app|
// is true.
void CreateBookmarkAppFromCurrentWebContents(Browser* browser,
                                             bool force_shortcut_app);
bool CanCreateBookmarkApp(const Browser* browser);

base::Optional<int> GetKeyboardFocusedTabIndex(const Browser* browser);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_COMMANDS_H_
