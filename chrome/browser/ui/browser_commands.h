// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_COMMANDS_H_
#define CHROME_BROWSER_UI_BROWSER_COMMANDS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
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
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
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
enum class SplitTabLayout;
}

namespace chrome {

// This service provides wrappers for basic commands associated with a
// specific browser window (for example, navigations forward and back,
// or pinning a tab).
class BrowserCommands {
 public:
  DECLARE_USER_DATA(BrowserCommands);

  // Retrieves the BrowserCommands instance associated with the browser
  // window.
  static BrowserCommands* From(BrowserWindowInterface* browser);

  explicit BrowserCommands(BrowserWindowInterface* browser);
  BrowserCommands(const BrowserCommands&) = delete;
  BrowserCommands& operator=(const BrowserCommands&) = delete;
  ~BrowserCommands();

  // Returns true if the command is enabled.
  bool IsCommandEnabled(int command);

  // Returns true if the command is supported.
  bool SupportsCommand(int command);

  // Executes the given command with the default disposition and current
  // timestamp.
  bool ExecuteCommand(int command,
                      base::TimeTicks time_stamp = base::TimeTicks::Now());

  // Executes the given command with the specified disposition.
  bool ExecuteCommandWithDisposition(int command,
                                     WindowOpenDisposition disposition);

  // Updates whether the given command is enabled or disabled.
  void UpdateCommandEnabled(int command, bool enabled);

  // Registers an observer to be notified when the given command's enabled state
  // changes.
  void AddCommandObserver(int command, CommandObserver* observer);

  // Unregisters a previously registered command observer.
  void RemoveCommandObserver(int command, CommandObserver* observer);

  // Retrieves the content restrictions currently applied (e.g. no print/no
  // save).
  int GetContentRestrictions();

  // Creates a new empty browser window. This is static because a browser window
  // might not exist yet.
  static void NewEmptyWindow(Profile* profile,
                             bool should_trigger_session_restore = true);

  // Opens a new empty browser window. This is static because a browser window
  // might not exist yet.
  static BrowserWindowInterface* OpenEmptyWindow(
      Profile* profile,
      bool should_trigger_session_restore = true);

  // Restores the last closed tabs in a new window. This is static because a
  // browser window might not exist yet.
  static void OpenWindowWithRestoredTabs(Profile* profile);

  // Opens the given URL in an incognito window. This is static because a
  // browser window might not exist yet.
  static void OpenURLOffTheRecord(Profile* profile, const GURL& url);

  // Checks if the browser can navigate backward.
  bool CanGoBack();

  // Checks if the specified WebContents can navigate backward.
  static bool CanGoBack(content::WebContents* web_contents);

  // Returns true if the back button should be visually enabled.
  bool ShouldEnableBackButton();

  // Navigates backward with the given disposition.
  void GoBack(WindowOpenDisposition disposition);

  // Navigates backward on the specified WebContents.
  static void GoBack(content::WebContents* web_contents);

  // Checks if the browser can navigate forward.
  bool CanGoForward();

  // Checks if the specified WebContents can navigate forward.
  static bool CanGoForward(content::WebContents* web_contents);

  // Returns true if the forward button should be visually enabled.
  bool ShouldEnableForwardButton();

  // Navigates forward with the given disposition.
  void GoForward(WindowOpenDisposition disposition);

  // Navigates forward on the specified WebContents.
  static void GoForward(content::WebContents* web_contents);

  // Navigates to a specific index in the navigation history.
  void NavigateToIndexWithDisposition(int index,
                                      WindowOpenDisposition disposition);

  // Reloads the current page.
  void Reload(WindowOpenDisposition disposition);

  // Reloads the current page, bypassing the cache.
  void ReloadBypassingCache(WindowOpenDisposition disposition);

  // Checks if reloading the current page is allowed.
  bool CanReload();

  // Navigates to the home page.
  void Home(WindowOpenDisposition disposition);

  // Navigates to the URL currently in the location bar.
  base::WeakPtr<content::NavigationHandle> OpenCurrentURL();

  // Stops loading the current page.
  void Stop();

  // Opens a new browser window.
  void NewWindow();

  // Opens a new incognito browser window.
  static void NewIncognitoWindow(Profile* profile);

  // Closes the browser window.
  void CloseWindow();

  // Creates a new tab in the browser window.
  content::WebContents& NewTab(
      NewTabTypes context = NewTabTypes::kNewTabCommand);

  // Creates a new tab to the right of the active tab.
  void NewTabToRight();

  // Creates a new tab from the URL stored in the clipboard.
  void NewTabFromClipboardURL();

  // Closes the active tab.
  void CloseTab();

  // Checks if the specified WebContents can be zoomed in.
  static bool CanZoomIn(content::WebContents* contents);

  // Checks if the specified WebContents can be zoomed out.
  static bool CanZoomOut(content::WebContents* contents);

  // Checks if the specified WebContents's zoom can be reset to default.
  static bool CanResetZoom(content::WebContents* contents);

  // Restores the last closed tab.
  void RestoreTab();

  // Selects the next tab in the tab strip.
  void SelectNextTab(
      TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kOther));

  // Selects the previous tab in the tab strip.
  void SelectPreviousTab(
      TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kOther));

  // Moves the active tab one position to the right.
  void MoveTabNext();

  // Moves the active tab one position to the left.
  void MoveTabPrevious();

  // Selects the tab at the specified index.
  void SelectNumberedTab(
      int index,
      TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kOther));

  // Selects the last tab in the tab strip.
  void SelectLastTab(
      TabStripUserGestureDetails gesture_detail = TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kOther));

  // Duplicates the active tab.
  void DuplicateTab();

  // Checks if the active tab can be duplicated.
  bool CanDuplicateTab();

  // Checks if the keyboard-focused tab can be duplicated.
  bool CanDuplicateKeyboardFocusedTab();

  // Checks if the active tab can be moved to a new window.
  bool CanMoveActiveTabToNewWindow();

  // Moves the active tab to a new browser window.
  void MoveActiveTabToNewWindow();

  // Checks if the tabs at the specified indices can be moved to a new window.
  bool CanMoveTabsToNewWindow(const std::vector<int>& tab_indices);

  // Moves the tabs at the specified indices to a new browser window.
  void MoveTabsToNewWindow(const std::vector<int>& tab_indices);

  // Moves the specified tab group to a new browser window.
  void MoveGroupToNewWindow(tab_groups::TabGroupId group);

  // Checks if tabs to the right of the active tab can be closed.
  bool CanCloseTabsToRight();

  // Checks if other tabs (not active) can be closed.
  bool CanCloseOtherTabs();

  // Duplicates the tab at the specified index.
  content::WebContents* DuplicateTabAt(int index);

  // Duplicates the specified split tab.
  void DuplicateSplit(split_tabs::SplitTabId split);

  // Checks if the tab at the specified index can be duplicated.
  bool CanDuplicateTabAt(int index);

  // Moves the tabs at the specified indices to an existing target window.
  void MoveTabsToExistingWindow(BrowserWindowInterface* target,
                                const std::vector<int>& tab_indices);

  // Moves the specified tab group to an existing target window.
  void MoveGroupToExistingWindow(BrowserWindowInterface* target,
                                 tab_groups::TabGroupId group);

  // Mutes the active site.
  void MuteSite();

  // Pins the active tab.
  void PinTab();

  // Groups the active tab.
  void GroupTab();

  // Creates a new split tab with the specified layout and source.
  void NewSplitTab(split_tabs::SplitTabLayout layout,
                   split_tabs::SplitTabCreatedSource source);

  // Adds the active tab to the current tab group.
  void AddNewTabToGroup();

  // Creates a new tab group and adds the active tab to it.
  void CreateNewTabGroup();

  // Closes the active tab group.
  void CloseTabGroup();

  // Focuses the next tab group.
  void FocusNextTabGroup();

  // Focuses the previous tab group.
  void FocusPreviousTabGroup();

  // Groups all ungrouped tabs in the tab strip.
  bool GroupAllUngroupedTabs();

  // Adds the active tab to the recently used tab group.
  void AddNewTabToRecentGroup();

  // Unfocuses the active tab group.
  void UnfocusTabGroup();

  // Mutes the site of the keyboard-focused tab.
  void MuteSiteForKeyboardFocusedTab();

  // Checks if there is a keyboard-focused tab.
  bool HasKeyboardFocusedTab();

  // Pins the keyboard-focused tab.
  void PinKeyboardFocusedTab();

  // Groups the keyboard-focused tab.
  void GroupKeyboardFocusedTab();

  // Duplicates the keyboard-focused tab.
  void DuplicateKeyboardFocusedTab();

  // Converts a popup window into a tabbed browser window.
  void ConvertPopupToTabbedBrowser();

  // Closes all tabs to the right of the active tab.
  void CloseTabsToRight();

  // Closes all other tabs (except the active tab).
  void CloseOtherTabs();

  // Exits the browser application. Static because no windows may be open.
  static void Exit();

  // Bookmarks the active tab.
  void BookmarkCurrentTab();

  // Bookmarks the active tab and places it in the specified folder.
  void BookmarkCurrentTabInFolder(bookmarks::BookmarkModel* model,
                                  int64_t folder_id);

  // Checks if the active tab can be bookmarked.
  bool CanBookmarkCurrentTab();

  // Bookmarks all open tabs in the window.
  void BookmarkAllTabs();

  // Checks if all open tabs can be bookmarked.
  bool CanBookmarkAllTabs();

  // Checks if the active tab can be added to Read Later.
  bool CanMoveActiveTabToReadLater();

  // Moves the active tab to Read Later.
  void MoveCurrentTabToReadLater();

  // Moves the specified WebContents list to Read Later.
  void MoveTabsToReadLater(std::vector<content::WebContents*> web_contentses);

  // Marks the active tab as read in Read Later.
  bool MarkCurrentTabAsReadInReadLater();

  // Checks if the active tab is unread in Read Later.
  bool IsCurrentTabUnreadInReadLater();

  // Shows commerce offers and rewards for the current page.
  void ShowOffersAndRewardsForPage();

  // Saves the credit card information from the page.
  void SaveCreditCard();

  // Saves the IBAN information from the page.
  void SaveIban();

  // Prompts the user to opt-in to mandatory re-authentication for payments.
  void ShowMandatoryReauthOptInPrompt();

  // Saves the autofill address from the page.
  void SaveAutofillAddress();

  // Shows the bubble displaying filled virtual card information.
  void ShowFilledCardInformationBubble();

  // Shows the virtual card enrollment bubble.
  void ShowVirtualCardEnrollBubble();

  // Initiates a tab organization request.
  void StartTabOrganizationRequest();

  // Shows the translation bubble for the current page.
  void ShowTranslateBubble();

  // Opens the password manager bubble for the current page.
  void ManagePasswordsForPage();

  // Checks if the active tab can be sent to another device.
  bool CanSendTabToSelf();

  // Sends the active tab to another device.
  void SendTabToSelf();

  // Checks if a QR code can be generated for the page.
  bool CanGenerateQrCode();

  // Generates a QR code for the page.
  void GenerateQRCode();

  // Opens the sharing hub bubble.
  void SharingHub();

  // Triggers screenshot capture for the page.
  void ScreenshotCapture();

  // Saves the current page.
  void SavePage();

  // Checks if the current page can be saved.
  bool CanSavePage();

  // Triggers print for the current page.
  void Print();

  // Checks if printing is allowed.
  bool CanPrint();

#if BUILDFLAG(ENABLE_PRINTING)
  // Opens the basic print dialog.
  void BasicPrint();

  // Checks if the basic print dialog is supported.
  bool CanBasicPrint();
#endif

  // Checks if media routing is supported.
  bool CanRouteMedia();

  // Opens media router dialog from the app menu.
  void RouteMediaInvokedFromAppMenu();

  // Opens the Find-in-page bar.
  void Find();

  // Finds the next occurrence of the search term.
  void FindNext();

  // Finds the previous occurrence of the search term.
  void FindPrevious();

  // Triggers Find-in-page with search direction options.
  void FindInPage(bool find_next, bool forward_direction);

  // Shows the tab search UI.
  void ShowTabSearch();

  // Closes the tab search UI.
  void CloseTabSearch();

  // Toggles whether the tab search UI is pinned.
  void ToggleTabSearchPin();

  // Toggles the contextual tasks side panel.
  void ToggleContextualTasksSidePanel();

  // Toggles vertical tabs state.
  void ToggleVerticalTabs();

  // Toggles whether vertical tabs expand on hover.
  void ToggleVerticalTabsExpandOnHover();

  // Checks if the Find-in-page bar can be closed.
  bool CanCloseFind();

  // Closes the Find-in-page bar.
  void CloseFind();

  // Adjusts the zoom level of the current page.
  void Zoom(content::PageZoom zoom);

  // Focuses the toolbar.
  void FocusToolbar();

  // Focuses the location bar.
  void FocusLocationBar();

  // Focuses the search box.
  void FocusSearch();

  // Focuses the app menu.
  void FocusAppMenu();

  // Focuses the bookmarks toolbar.
  void FocusBookmarksToolbar();

  // Focuses an inactive popup window for accessibility.
  void FocusInactivePopupForAccessibility();

  // Focuses the next pane in the window.
  void FocusNextPane();

  // Focuses the previous pane in the window.
  void FocusPreviousPane();

  // Focuses the main WebContents pane.
  void FocusWebContentsPane();

  // Toggles the DevTools window.
  void ToggleDevToolsWindow(DevToolsToggleAction action,
                            DevToolsOpenedByAction opened_by);

  // Checks if the task manager can be opened. Static because it is a global
  // action.
  static bool CanOpenTaskManager();

  // Opens the task manager window. Static because a browser window might not
  // exist yet.
  static void OpenTaskManager(BrowserWindowInterface* browser,
                              task_manager::StartAction start_action =
                                  task_manager::StartAction::kOther);

  // Opens the feedback dialog.
  void OpenFeedbackDialog(
      feedback::FeedbackSource source,
      const std::string& description_template = std::string(),
      const std::string& category_tag = std::string());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Opens the unsafe site reporting dialog.
  void OpenReportUnsafeSiteDialog();
#endif

  // Toggles the bookmark bar visibility.
  void ToggleBookmarkBar();

  // Toggles showing full URLs in the location bar.
  void ToggleShowFullURLs();

  // Toggles showing the Google Lens shortcut in the omnibox.
  void ToggleShowGoogleLensShortcut();

  // Toggles showing the AI mode omnibox button.
  void ToggleShowAiModeOmniboxButton();

  // Toggles showing search tools under the omnibox.
  void ToggleShowSearchTools();

  // Opens the app menu.
  void ShowAppMenu();

  // Opens the profile/avatar menu.
  void ShowAvatarMenu();

  // Opens the "Chrome is out of date" update dialog.
  void OpenUpdateChromeDialog();

  // Checks if requesting the tablet site is allowed.
  static bool CanRequestTabletSite(content::WebContents* current_tab);

  // Checks if the tablet site is currently being requested.
  bool IsRequestingTabletSite();

  // Toggles between mobile and tablet user agent.
  void ToggleRequestTabletSite();

  // Sets the user agent to Android tablet for the active tab.
  static void SetAndroidOsForTabletSite(content::WebContents* current_tab);

  // Toggles fullscreen mode.
  void ToggleFullscreenMode(bool user_initiated = false);

  // Clears the browser's cache.
  void ClearCache();

  // Checks if DevTools debugger is attached to the current tab.
  bool IsDebuggerAttachedToCurrentTab();

  // Copies the active URL to the clipboard.
  void CopyURL(content::WebContents* web_contents);

  // Checks if the active URL can be copied.
  bool CanCopyUrl();

  // Checks if the window is a web app or custom tab.
  bool IsWebAppOrCustomTab();

  // Reparents and opens a hosted app in a full Chrome tabbed browser window.
  BrowserWindowInterface* OpenInChrome();

  // Checks if viewing page source is allowed.
  bool CanViewSource();

  // Checks if caret browsing can be toggled.
  bool CanToggleCaretBrowsing();

  // Toggles caret browsing.
  void ToggleCaretBrowsing();

  // Prompts the user to name the browser window.
  void PromptToNameWindow();

#if BUILDFLAG(IS_CHROMEOS)
  // Toggles the multitask menu (ChromeOS only).
  void ToggleMultitaskMenu();
#endif

  // Executes a UI debug command.
  void ExecuteUIDebugCommand(int id);

  // Retrieves the index of the keyboard-focused tab, if any.
  std::optional<int> GetKeyboardFocusedTabIndex();

  // Shows the incognito clear browsing data dialog.
  void ShowIncognitoClearBrowsingDataDialog();

  // Shows the incognito history disclaimer dialog.
  void ShowIncognitoHistoryDisclaimerDialog();

  // Checks if a Chrome URL navigation should be intercepted in incognito.
  bool ShouldInterceptChromeURLNavigationInIncognito(const GURL& url);

  // Intercepts and processes a Chrome URL navigation in incognito.
  void ProcessInterceptedChromeURLNavigationInIncognito(const GURL& url);

  // Executes Lens Overlay on the current page.
  void ExecLensOverlay();

  // Executes Lens Region Search on the current page.
  void ExecLensRegionSearch();

 private:
  const raw_ptr<BrowserWindowInterface> browser_;
  std::optional<ui::ScopedUnownedUserData<BrowserCommands>>
      scoped_unowned_user_data_;
};

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
                             NewTabTypes context = NewTabTypes::kNewTabCommand);
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
