// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_COMMANDS_H_
#define CHROME_BROWSER_UI_BROWSER_COMMANDS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/public/common/page_zoom.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
Browser* OpenEmptyWindow(Profile* profile,
                         bool should_trigger_session_restore = true);

// Opens a new window with the tabs from |profile|'s TabRestoreService.
void OpenWindowWithRestoredTabs(Profile* profile);

// Opens the specified URL in a new browser window in an incognito session. If
// there is already an existing active incognito session for the specified
// |profile|, that session is re- used.
void OpenURLOffTheRecord(Profile* profile, const GURL& url);

bool CanGoBack(const Browser* browser);
bool CanGoBack(content::WebContents* web_contents);
void GoBack(Browser* browser, WindowOpenDisposition disposition);
void GoBack(content::WebContents* web_contents);
bool CanGoForward(const Browser* browser);
bool CanGoForward(content::WebContents* web_contents);
void GoForward(Browser* browser, WindowOpenDisposition disposition);
void GoForward(content::WebContents* web_contents);
void NavigateToIndexWithDisposition(Browser* browser,
                                    int index,
                                    WindowOpenDisposition disposition);
void Reload(Browser* browser, WindowOpenDisposition disposition);
void ReloadBypassingCache(Browser* browser, WindowOpenDisposition disposition);
bool CanReload(const Browser* browser);
void Home(Browser* browser, WindowOpenDisposition disposition);
base::WeakPtr<content::NavigationHandle> OpenCurrentURL(Browser* browser);
void Stop(Browser* browser);
void NewWindow(Browser* browser);
void NewIncognitoWindow(Profile* profile);
void CloseWindow(Browser* browser);
void NewTab(Browser* browser);
void NewTabToRight(Browser* browser);
void CloseTab(Browser* browser);
bool CanZoomIn(content::WebContents* contents);
bool CanZoomOut(content::WebContents* contents);
bool CanResetZoom(content::WebContents* contents);
void RestoreTab(Browser* browser);
void SelectNextTab(
    Browser* browser,
    TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
        TabStripUserGestureDetails::GestureType::kOther));
void SelectPreviousTab(
    Browser* browser,
    TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
        TabStripUserGestureDetails::GestureType::kOther));
void MoveTabNext(Browser* browser);
void MoveTabPrevious(Browser* browser);
void SelectNumberedTab(
    Browser* browser,
    int index,
    TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
        TabStripUserGestureDetails::GestureType::kOther));
void SelectLastTab(
    Browser* browser,
    TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
        TabStripUserGestureDetails::GestureType::kOther));
void DuplicateTab(Browser* browser);
bool CanDuplicateTab(const Browser* browser);
bool CanDuplicateKeyboardFocusedTab(const Browser* browser);
bool CanMoveActiveTabToNewWindow(Browser* browser);
void MoveActiveTabToNewWindow(Browser* browser);
bool CanMoveTabsToNewWindow(Browser* browser,
                            const std::vector<int>& tab_indices);
// Moves the specified |tab_indices| to a newly-created window. If |group| is
// specified, adds all the moved tabs to a new group. This group will have the
// appearance as |group| but a different ID, since IDs can't be shared across
// windows.
void MoveTabsToNewWindow(
    Browser* browser,
    const std::vector<int>& tab_indices,
    absl::optional<tab_groups::TabGroupId> group = absl::nullopt);
bool CanCloseTabsToRight(const Browser* browser);
bool CanCloseOtherTabs(const Browser* browser);
content::WebContents* DuplicateTabAt(Browser* browser, int index);
bool CanDuplicateTabAt(const Browser* browser, int index);
void MoveTabsToExistingWindow(Browser* source,
                              Browser* target,
                              const std::vector<int>& tab_indices);
void MuteSite(Browser* browser);
void PinTab(Browser* browser);
void GroupTab(Browser* browser);
void MuteSiteForKeyboardFocusedTab(Browser* browser);
bool HasKeyboardFocusedTab(const Browser* browser);
void PinKeyboardFocusedTab(Browser* browser);
void GroupKeyboardFocusedTab(Browser* browser);
void DuplicateKeyboardFocusedTab(Browser* browser);
void ConvertPopupToTabbedBrowser(Browser* browser);
void CloseTabsToRight(Browser* browser);
void CloseOtherTabs(Browser* browser);
void Exit();
// Bookmarks the current tab in the most recently used folder and shows the
// edit dialog.
void BookmarkCurrentTab(Browser* browser);
// Bookmarks the current tab in the given folder and does not show the edit
// dialog.
void BookmarkCurrentTabInFolder(Browser* browser, int64_t folder_id);
bool CanBookmarkCurrentTab(const Browser* browser);
void BookmarkAllTabs(Browser* browser);
bool CanBookmarkAllTabs(const Browser* browser);
bool CanMoveActiveTabToReadLater(Browser* browser);
bool MoveCurrentTabToReadLater(Browser* browser);
bool MoveTabToReadLater(Browser* browser, content::WebContents* web_contents);
bool MarkCurrentTabAsReadInReadLater(Browser* browser);
bool IsCurrentTabUnreadInReadLater(Browser* browser);
void ShowOffersAndRewardsForPage(Browser* browser);
void SaveCreditCard(Browser* browser);
void SaveIBAN(Browser* browser);
void ShowMandatoryReauthOptInPrompt(Browser* browser);
void MigrateLocalCards(Browser* browser);
void SaveAutofillAddress(Browser* browser);
void ShowVirtualCardManualFallbackBubble(Browser* browser);
void ShowVirtualCardEnrollBubble(Browser* browser);
void ShowTranslateBubble(Browser* browser);
void ManagePasswordsForPage(Browser* browser);
bool CanSendTabToSelf(const Browser* browser);
void SendTabToSelfFromPageAction(Browser* browser);
bool CanGenerateQrCode(const Browser* browser);
void GenerateQRCodeFromPageAction(Browser* browser);
void SharingHubFromPageAction(Browser* browser);
void ScreenshotCaptureFromPageAction(Browser* browser);
void SavePage(Browser* browser);
bool CanSavePage(const Browser* browser);
void Print(Browser* browser);
bool CanPrint(Browser* browser);
#if BUILDFLAG(ENABLE_PRINTING)
void BasicPrint(Browser* browser);
bool CanBasicPrint(Browser* browser);
#endif  // ENABLE_PRINTING
bool CanRouteMedia(Browser* browser);
// NOTE: For metrics collection purposes, this method is assumed to be invoked
// from the app menu. That will need to be changed if this is to be invoked from
// elsewhere.
void RouteMediaInvokedFromAppMenu(Browser* browser);
void CutCopyPaste(Browser* browser, int command_id);
void Find(Browser* browser);
void FindNext(Browser* browser);
void FindPrevious(Browser* browser);
void FindInPage(Browser* browser, bool find_next, bool forward_direction);
void ShowTabSearch(Browser* browser);
void CloseTabSearch(Browser* browser);
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
void FocusWebContentsPane(Browser* browser);
void ToggleDevToolsWindow(Browser* browser,
                          DevToolsToggleAction action,
                          DevToolsOpenedByAction opened_by);
bool CanOpenTaskManager();
// Opens task manager UI. Note that |browser| can be nullptr as input.
void OpenTaskManager(Browser* browser);
void OpenFeedbackDialog(
    Browser* browser,
    FeedbackSource source,
    const std::string& description_template = std::string());
void ToggleBookmarkBar(Browser* browser);
void ToggleShowFullURLs(Browser* browser);
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
void CopyURL(content::WebContents* web_contents);
// Moves the WebContents of a hosted app Browser to a tabbed Browser. Returns
// the tabbed Browser.
Browser* OpenInChrome(Browser* hosted_app_browser);
bool CanViewSource(const Browser* browser);
bool CanToggleCaretBrowsing(Browser* browser);
void ToggleCaretBrowsing(Browser* browser);
void PromptToNameWindow(Browser* browser);
#if BUILDFLAG(IS_CHROMEOS)
void ToggleMultitaskMenu(Browser* browser);
#endif
void ToggleCommander(Browser* browser);
void ExecuteUIDebugCommand(int id, const Browser* browser);

absl::optional<int> GetKeyboardFocusedTabIndex(const Browser* browser);

void ShowIncognitoClearBrowsingDataDialog(Browser* browser);
void ShowIncognitoHistoryDisclaimerDialog(Browser* browser);
bool ShouldInterceptChromeURLNavigationInIncognito(Browser* browser,
                                                   const GURL& url);
void ProcessInterceptedChromeURLNavigationInIncognito(Browser* browser,
                                                      const GURL& url);

// Follows/unfollows a web feed associated with the main frame of specified web
// contents.
void FollowSite(content::WebContents* web_contents);
void UnfollowSite(content::WebContents* web_contents);

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
// Triggers the Screen AI visual annotations to be run once on the |browser|.
void RunScreenAIVisualAnnotation(Browser* browser);
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

void ExecLensRegionSearch(Browser* browser);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_COMMANDS_H_
