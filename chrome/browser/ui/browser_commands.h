// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_COMMANDS_H_
#define CHROME_BROWSER_UI_BROWSER_COMMANDS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/task_manager/task_manager_metrics_recorder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "components/split_tabs/split_tab_id.h"
#include "content/public/common/page_zoom.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/window_open_disposition.h"

class BrowserWindowInterface;
class CommandObserver;
class GURL;
class Profile;
enum class DevToolsOpenedByAction;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace split_tabs {
enum class SplitTabCreatedSource;
}

namespace chrome {

// For all commands, where a tab is not specified, the active tab is assumed.

bool IsCommandEnabled(BrowserWindowInterface* browser, int command);
bool SupportsCommand(BrowserWindowInterface* browser, int command);
bool ExecuteCommand(BrowserWindowInterface* browser,
                    int command,
                    base::TimeTicks time_stamp = base::TimeTicks::Now());
bool ExecuteCommandWithDisposition(BrowserWindowInterface* browser,
                                   int command,
                                   WindowOpenDisposition disposition);
void UpdateCommandEnabled(BrowserWindowInterface* browser, int command, bool enabled);
void AddCommandObserver(BrowserWindowInterface*, int command, CommandObserver* observer);
void RemoveCommandObserver(BrowserWindowInterface*, int command, CommandObserver* observer);

int GetContentRestrictions(const BrowserWindowInterface* browser);

// Opens a new window. If the |should_trigger_session_restore| is true, a new
// window opening should be treated like the start of a session (with potential
// session restore, startup URLs, etc.). Otherwise, don't restore the session,
// opens a new window with the default blank tab.
void NewEmptyWindow(Profile* profile,
                    bool should_trigger_session_restore = true);

// Opens a new window. If the |should_trigger_session_restore| is true, a new
// window opening should be treated like the start of a session (with potential
// session restore, startup URLs, etc.). Otherwise, don't restore the session,
// opens a new window with the default blank tab. This bypasses metrics and
// various internal bookkeeping; NewEmptyWindow (above) is preferred.
// Returns nullptr if browser creation is not possible.
BrowserWindowInterface* OpenEmptyWindow(
    Profile* profile,
    bool should_trigger_session_restore = true);

// Opens a new window with the tabs from |profile|'s TabRestoreService.
void OpenWindowWithRestoredTabs(Profile* profile);

// Opens the specified URL in a new browser window in an incognito session. If
// there is already an existing active incognito session for the specified
// |profile|, that session is re- used.
void OpenURLOffTheRecord(Profile* profile, const GURL& url);

bool CanGoBack(const BrowserWindowInterface* browser);
bool CanGoBack(content::WebContents* web_contents);
bool ShouldEnableBackButton(const BrowserWindowInterface* browser);
void GoBack(BrowserWindowInterface* browser, WindowOpenDisposition disposition);
void GoBack(content::WebContents* web_contents);
bool CanGoForward(const BrowserWindowInterface* browser);
bool CanGoForward(content::WebContents* web_contents);
bool ShouldEnableForwardButton(const BrowserWindowInterface* browser);
void GoForward(BrowserWindowInterface* browser,
               WindowOpenDisposition disposition);
void GoForward(content::WebContents* web_contents);
void NavigateToIndexWithDisposition(BrowserWindowInterface* browser,
                                    int index,
                                    WindowOpenDisposition disposition);
void Reload(BrowserWindowInterface* browser, WindowOpenDisposition disposition);
void ReloadBypassingCache(BrowserWindowInterface* browser,
                          WindowOpenDisposition disposition);
bool CanReload(const BrowserWindowInterface* browser);
void Home(BrowserWindowInterface* browser, WindowOpenDisposition disposition);
base::WeakPtr<content::NavigationHandle> OpenCurrentURL(
    BrowserWindowInterface* browser);
void Stop(BrowserWindowInterface* browser);
void NewWindow(BrowserWindowInterface* browser);
void NewIncognitoWindow(Profile* profile);
void CloseWindow(BrowserWindowInterface* browser);
content::WebContents& NewTab(BrowserWindowInterface* browser,
                             NewTabTypes context);
void NewTabToRight(BrowserWindowInterface* browser);
void NewTabFromClipboardURL(BrowserWindowInterface* browser);
void CloseTab(BrowserWindowInterface* browser);
bool CanZoomIn(content::WebContents* contents);
bool CanZoomOut(content::WebContents* contents);
bool CanResetZoom(content::WebContents* contents);
void RestoreTab(BrowserWindowInterface* browser);
void SelectNextTab(
    BrowserWindowInterface* browser,
    TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
        TabStripUserGestureDetails::GestureType::kOther));
void SelectPreviousTab(
    BrowserWindowInterface* browser,
    TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
        TabStripUserGestureDetails::GestureType::kOther));
void MoveTabNext(BrowserWindowInterface* browser);
void MoveTabPrevious(BrowserWindowInterface* browser);
void SelectNumberedTab(
    BrowserWindowInterface* browser,
    int index,
    TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
        TabStripUserGestureDetails::GestureType::kOther));
void SelectLastTab(
    BrowserWindowInterface* browser,
    TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
        TabStripUserGestureDetails::GestureType::kOther));
void DuplicateTab(BrowserWindowInterface* browser);
bool CanDuplicateTab(const BrowserWindowInterface* browser);
bool CanDuplicateKeyboardFocusedTab(const BrowserWindowInterface* browser);
bool CanMoveActiveTabToNewWindow(BrowserWindowInterface* browser);
void MoveActiveTabToNewWindow(BrowserWindowInterface* browser);
bool CanMoveTabsToNewWindow(BrowserWindowInterface* browser,
                            const std::vector<int>& tab_indices);
// Moves the specified |tab_indices| to a newly-created window. If |group| is
// specified, adds all the moved tabs to a new group. This group will have the
// appearance as |group| but a different ID, since IDs can't be shared across
// windows.
void MoveTabsToNewWindow(BrowserWindowInterface* browser,
                         const std::vector<int>& tab_indices);
void MoveGroupToNewWindow(BrowserWindowInterface* browser,
                          tab_groups::TabGroupId group);
bool CanCloseTabsToRight(const BrowserWindowInterface* browser);
bool CanCloseOtherTabs(const BrowserWindowInterface* browser);
content::WebContents* DuplicateTabAt(BrowserWindowInterface* browser,
                                     int index);
void DuplicateSplit(BrowserWindowInterface* browser,
                    split_tabs::SplitTabId split);
bool CanDuplicateTabAt(const BrowserWindowInterface* browser, int index);
void MoveTabsToExistingWindow(BrowserWindowInterface* source,
                              BrowserWindowInterface* target,
                              const std::vector<int>& tab_indices);
void MoveGroupToExistingWindow(BrowserWindowInterface* source,
                               BrowserWindowInterface* target,
                               tab_groups::TabGroupId group);
void MuteSite(BrowserWindowInterface* browser);
void PinTab(BrowserWindowInterface* browser);
void GroupTab(BrowserWindowInterface* browser);
void NewSplitTab(BrowserWindowInterface* browser,
                 split_tabs::SplitTabLayout layout,
                 split_tabs::SplitTabCreatedSource source);

// Tab group commands
// These values are persisted to logs. Entries should not be renumbered
// and  numeric values should never be reused.
//
// LINT.IfChange(TabGroupShortcut)
enum class TabGroupShortcut {
  kCreateNewTabGroup = 0,
  kCloseTabGroup = 1,
  kAddNewTabToGroup = 2,
  kFocusNextTabGroup = 3,
  kFocusPrevTabGroup = 4,
  kMaxValue = kFocusPrevTabGroup
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:TabGroupShortcut)

// Creates a new tab at the end of the active tab's group.
void AddNewTabToGroup(BrowserWindowInterface* browser);
// Creates a new tab group at the end of the tab strip.
void CreateNewTabGroup(BrowserWindowInterface* browser);
// Closes the entire tab group the active tab is in.
void CloseTabGroup(BrowserWindowInterface* browser);
// Finds the next tab group that isn't the current one in the tab strip and
// activates the first tab in the group.
void FocusNextTabGroup(BrowserWindowInterface* browser);
// Finds the previous tab group that isn't the current one in the tabstrip and
// activates the first tab in the group.
void FocusPreviousTabGroup(BrowserWindowInterface* browser);
// Takes all ungrouped tabs and places them in a new group.
// Returns true if a group was made, and false otherwise.
bool GroupAllUngroupedTabs(BrowserWindowInterface* browser);
// Creates a new tab at the end of the group which last had the active tab.
void AddNewTabToRecentGroup(BrowserWindowInterface* browser);
// Unfocuses the currently focused tab group, if any.
void UnfocusTabGroup(BrowserWindowInterface* browser);

void MuteSiteForKeyboardFocusedTab(BrowserWindowInterface* browser);
bool HasKeyboardFocusedTab(const BrowserWindowInterface* browser);
void PinKeyboardFocusedTab(BrowserWindowInterface* browser);
void GroupKeyboardFocusedTab(BrowserWindowInterface* browser);
void DuplicateKeyboardFocusedTab(BrowserWindowInterface* browser);
void ConvertPopupToTabbedBrowser(BrowserWindowInterface* browser);
void CloseTabsToRight(BrowserWindowInterface* browser);
void CloseOtherTabs(BrowserWindowInterface* browser);
void Exit();
// Bookmarks the current tab in the most recently used folder and shows the
// edit dialog.
void BookmarkCurrentTab(BrowserWindowInterface* browser);
// Bookmarks the current tab in the given folder and does not show the edit
// dialog.
void BookmarkCurrentTabInFolder(BrowserWindowInterface* browser,
                                bookmarks::BookmarkModel* model,
                                int64_t folder_id);
bool CanBookmarkCurrentTab(BrowserWindowInterface* browser);
void BookmarkAllTabs(BrowserWindowInterface* browser);
bool CanBookmarkAllTabs(BrowserWindowInterface* browser);
bool CanMoveActiveTabToReadLater(BrowserWindowInterface* browser);
void MoveCurrentTabToReadLater(BrowserWindowInterface* browser);
void MoveTabsToReadLater(BrowserWindowInterface* browser,
                         std::vector<content::WebContents*> web_contentses);
bool MarkCurrentTabAsReadInReadLater(BrowserWindowInterface* browser);
bool IsCurrentTabUnreadInReadLater(BrowserWindowInterface* browser);
void ShowOffersAndRewardsForPage(BrowserWindowInterface* browser);
void SaveCreditCard(BrowserWindowInterface* browser);
void SaveIban(BrowserWindowInterface* browser);
void ShowMandatoryReauthOptInPrompt(BrowserWindowInterface* browser);
void SaveAutofillAddress(BrowserWindowInterface* browser);
void ShowFilledCardInformationBubble(BrowserWindowInterface* browser);
void ShowVirtualCardEnrollBubble(BrowserWindowInterface* browser);
void StartTabOrganizationRequest(BrowserWindowInterface* browser);
void ShowTranslateBubble(BrowserWindowInterface* browser);
void ManagePasswordsForPage(BrowserWindowInterface* browser);
bool CanSendTabToSelf(BrowserWindowInterface* browser);
void SendTabToSelf(BrowserWindowInterface* browser);
bool CanGenerateQrCode(BrowserWindowInterface* browser);
void GenerateQRCode(BrowserWindowInterface* browser);
void SharingHub(BrowserWindowInterface* browser);
void ScreenshotCapture(BrowserWindowInterface* browser);
void SavePage(BrowserWindowInterface* browser);
bool CanSavePage(const BrowserWindowInterface* browser);
void Print(BrowserWindowInterface* browser);
bool CanPrint(BrowserWindowInterface* browser);
#if BUILDFLAG(ENABLE_PRINTING)
void BasicPrint(BrowserWindowInterface* browser);
bool CanBasicPrint(BrowserWindowInterface* browser);
#endif  // ENABLE_PRINTING
bool CanRouteMedia(BrowserWindowInterface* browser);
// NOTE: For metrics collection purposes, this method is assumed to be invoked
// from the app menu. That will need to be changed if this is to be invoked from
// elsewhere.
void RouteMediaInvokedFromAppMenu(BrowserWindowInterface* browser);
void Find(BrowserWindowInterface* browser);
void FindNext(BrowserWindowInterface* browser);
void FindPrevious(BrowserWindowInterface* browser);
void FindInPage(BrowserWindowInterface* browser, bool find_next, bool forward_direction);
void ShowTabSearch(BrowserWindowInterface* browser);
void CloseTabSearch(BrowserWindowInterface* browser);
void ToggleTabSearchPin(BrowserWindowInterface* browser);
void ToggleContextualTasksSidePanel(BrowserWindowInterface* browser);
void ToggleVerticalTabs(BrowserWindowInterface* browser);
void ToggleVerticalTabsExpandOnHover(BrowserWindowInterface* browser);
bool CanCloseFind(BrowserWindowInterface* browser);
void CloseFind(BrowserWindowInterface* browser);
void Zoom(BrowserWindowInterface* browser, content::PageZoom zoom);
void FocusToolbar(BrowserWindowInterface* browser);
void FocusLocationBar(BrowserWindowInterface* browser);
void FocusSearch(BrowserWindowInterface* browser);
void FocusAppMenu(BrowserWindowInterface* browser);
void FocusBookmarksToolbar(BrowserWindowInterface* browser);
void FocusInactivePopupForAccessibility(BrowserWindowInterface* browser);
void FocusNextPane(BrowserWindowInterface* browser);
void FocusPreviousPane(BrowserWindowInterface* browser);
void FocusWebContentsPane(BrowserWindowInterface* browser);
void ToggleDevToolsWindow(BrowserWindowInterface* browser,
                          DevToolsToggleAction action,
                          DevToolsOpenedByAction opened_by);
bool CanOpenTaskManager();
// Opens task manager UI. Note that |browser| can be nullptr as input.
// StartAction denotes which location the task manager UI was started from.
void OpenTaskManager(
    BrowserWindowInterface* browser,
    task_manager::StartAction start_action = task_manager::StartAction::kOther);
void OpenFeedbackDialog(BrowserWindowInterface* browser,
                        feedback::FeedbackSource source,
                        const std::string& description_template = std::string(),
                        const std::string& category_tag = std::string());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void OpenReportUnsafeSiteDialog(BrowserWindowInterface* browser);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
void ToggleBookmarkBar(BrowserWindowInterface* browser);
void ToggleShowFullURLs(BrowserWindowInterface* browser);
void ToggleShowGoogleLensShortcut(BrowserWindowInterface* browser);
void ToggleShowAiModeOmniboxButton(BrowserWindowInterface* browser);
void ToggleShowSearchTools(BrowserWindowInterface* browser);
void ShowAppMenu(BrowserWindowInterface* browser);
void ShowAvatarMenu(BrowserWindowInterface* browser);
void OpenUpdateChromeDialog(BrowserWindowInterface* browser);
bool CanRequestTabletSite(content::WebContents* current_tab);
bool IsRequestingTabletSite(BrowserWindowInterface* browser);
void ToggleRequestTabletSite(BrowserWindowInterface* browser);
// Overwrite the user agent's OS with Android OS so that the web content is
// using its mobile version layout. Note it won't take effect until the web
// contents is reloaded.
void SetAndroidOsForTabletSite(content::WebContents* current_tab);
void ToggleFullscreenMode(BrowserWindowInterface* browser,
                          bool user_initiated = false);
void ClearCache(BrowserWindowInterface* browser);
bool IsDebuggerAttachedToCurrentTab(BrowserWindowInterface* browser);
void CopyURL(BrowserWindowInterface* browser,
             content::WebContents* web_contents);
bool CanCopyUrl(BrowserWindowInterface* browser);
// Returns true if the browser window is for a web app or custom tab.
bool IsWebAppOrCustomTab(const BrowserWindowInterface* browser);
// Moves the WebContents of a hosted app Browser to a tabbed Browser. Returns
// the tabbed Browser.
BrowserWindowInterface* OpenInChrome(
    BrowserWindowInterface* hosted_app_browser);
bool CanViewSource(BrowserWindowInterface* browser);
bool CanToggleCaretBrowsing(BrowserWindowInterface* browser);
void ToggleCaretBrowsing(BrowserWindowInterface* browser);
void PromptToNameWindow(BrowserWindowInterface* browser);
#if BUILDFLAG(IS_CHROMEOS)
void ToggleMultitaskMenu(BrowserWindowInterface* browser);
#endif
void ExecuteUIDebugCommand(int id, const BrowserWindowInterface* browser);

std::optional<int> GetKeyboardFocusedTabIndex(
    const BrowserWindowInterface* browser);

void ShowIncognitoClearBrowsingDataDialog(BrowserWindowInterface* browser);
void ShowIncognitoHistoryDisclaimerDialog(BrowserWindowInterface* browser);
bool ShouldInterceptChromeURLNavigationInIncognito(BrowserWindowInterface* browser,
                                                   const GURL& url);
void ProcessInterceptedChromeURLNavigationInIncognito(BrowserWindowInterface* browser,
                                                      const GURL& url);
void ExecLensOverlay(BrowserWindowInterface* browser);
void ExecLensRegionSearch(BrowserWindowInterface* browser);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_COMMANDS_H_
