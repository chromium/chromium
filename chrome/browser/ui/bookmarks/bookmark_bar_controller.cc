// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/sad_tab.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

// TODO(crbug.com/382494946): Similar bespoke checks are used throughout the
// codebase. This should be factored out as a common util and other callsites
// converted to use this.
bool IsShowingNTP(content::WebContents* web_contents) {
  if (SadTab::ShouldShow(web_contents->GetCrashedStatus())) {
    return false;
  }

  // Use the committed entry (or the visible entry, if the committed entry is
  // the initial NavigationEntry) so the bookmarks bar disappears at the same
  // time the page does.
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry->IsInitialEntry()) {
    entry = web_contents->GetController().GetVisibleEntry();
  }
  const GURL& url = entry->GetURL();
  return NewTabUI::IsNewTab(url) || NewTabPageUI::IsNewTabPageOrigin(url) ||
         NewTabPageThirdPartyUI::IsNewTabPageOrigin(url) ||
         search::NavEntryIsInstantNTP(web_contents, entry);
}

}  // namespace

DEFINE_USER_DATA(BookmarkBarController);

BookmarkBarController::BookmarkBarController(BrowserWindowInterface& browser,
                                             TabStripModel& tab_strip_model)
    : browser_(browser),
      tab_strip_model_(tab_strip_model),
      scoped_data_holder_(browser.GetUnownedUserDataHost(), *this) {
  tab_strip_model_->AddObserver(this);

  // Set up preference observer for bookmark bar visibility.
  Profile* profile = browser_->GetProfile();
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      bookmarks::prefs::kShowBookmarkBar,
      base::BindRepeating(&BookmarkBarController::UpdateBookmarkBarState,
                          base::Unretained(this),
                          StateChangeReason::kPrefChange));

  // Initialize the bookmark bar state.
  UpdateBookmarkBarState(StateChangeReason::kInit);
}

BookmarkBarController::~BookmarkBarController() = default;

BookmarkBarController* BookmarkBarController::From(
    BrowserWindowInterface* browser_window_interface) {
  return ui::ScopedUnownedUserData<BookmarkBarController>::Get(
      browser_window_interface->GetUnownedUserDataHost());
}

const BookmarkBarController* BookmarkBarController::From(
    const BrowserWindowInterface* browser_window_interface) {
  return ui::ScopedUnownedUserData<BookmarkBarController>::Get(
      browser_window_interface->GetUnownedUserDataHost());
}

void BookmarkBarController::SetForceShowBookmarkBarFlag(ForceShowFlag flag) {
  force_show_bookmark_bar_flags_ |= flag;
  UpdateBookmarkBarState(StateChangeReason::kForceShow);
}

void BookmarkBarController::ClearForceShowBookmarkBarFlag(ForceShowFlag flag) {
  force_show_bookmark_bar_flags_ &= ~flag;
  UpdateBookmarkBarState(StateChangeReason::kForceShow);
}

void BookmarkBarController::UpdateBookmarkBarState(StateChangeReason reason) {
  BookmarkBar::State state =
      ShouldShowBookmarkBar() ? BookmarkBar::SHOW : BookmarkBar::HIDDEN;

  if (state == bookmark_bar_state_) {
    return;
  }

  bookmark_bar_state_ = state;

  if (reason == StateChangeReason::kTabSwitch) {
    // Don't notify BrowserWindow on a tab switch as at the time this is invoked
    // BrowserWindow hasn't yet switched tabs. The BrowserWindow implementations
    // end up querying state once they process the tab switch.
    return;
  }

  bool should_animate = reason == StateChangeReason::kPrefChange ||
                        reason == StateChangeReason::kForceShow;
  Browser* browser = browser_->GetBrowserForMigrationOnly();
  if (browser && browser->window()) {
    browser->window()->BookmarkBarStateChanged(
        should_animate ? BookmarkBar::ANIMATE_STATE_CHANGE
                       : BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
  }
}

void BookmarkBarController::OnSplitTabChanged(const SplitTabChange& change) {
  if (change.type == SplitTabChange::Type::kAdded ||
      change.type == SplitTabChange::Type::kRemoved) {
    UpdateBookmarkBarState(StateChangeReason::kSplitTabChange);
  }
}

bool BookmarkBarController::ShouldShowBookmarkBar() const {
  Profile* profile = browser_->GetProfile();
  if (profile->IsGuestSession()) {
    return false;
  }

  if (browser_defaults::bookmarks_enabled &&
      profile->GetPrefs()->GetBoolean(bookmarks::prefs::kShowBookmarkBar) &&
      !browser_->ShouldHideUIForFullscreen()) {
    return true;
  }

  if (force_show_bookmark_bar_flags_ != ForceShowFlag::kNone) {
    return true;
  }

  if (!browser_defaults::bookmarks_enabled) {
    return false;
  }

  PrefService* prefs = profile->GetPrefs();
  if (prefs->IsManagedPreference(bookmarks::prefs::kShowBookmarkBar) &&
      !prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar)) {
    return false;
  }

  std::vector<tabs::TabInterface*> tabs = tab_strip_model_->GetForegroundTabs();
  if (tabs.empty()) {
    return false;
  }

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  const bool has_bookmarks = bookmark_model && bookmark_model->HasBookmarks();

  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  const bool has_saved_tab_groups =
      tab_group_service && !tab_group_service->GetAllGroups().empty();

  // The bookmark bar is only shown if the user has added something to it.
  if (!has_bookmarks && !has_saved_tab_groups) {
    return false;
  }

  // Prevent the bookmark bar from showing itself when entering fullscreen if
  // fullscreen is entered through webview (TAB). This creates a consistent
  // experience for split view fullscreen and the rest of the UI.
  const tabs::TabInterface* active_tab = tab_strip_model_->GetActiveTab();
  if (active_tab->GetContents()->IsFullscreen()) {
    return false;
  }

  return std::any_of(
      tabs.begin(), tabs.end(), [](const tabs::TabInterface* tab) {
        return tab->GetContents() && IsShowingNTP(tab->GetContents());
      });
}

void BookmarkBarController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    if (web_contents()) {
      content::WebContentsObserver::Observe(nullptr);
    }
    if (selection.new_contents) {
      content::WebContentsObserver::Observe(selection.new_contents);
    }

    // Intentionally not updating the state to kTabSwitch. It is already updated
    // in Browser::OnActiveTabChanged(), since the BrowserWindow may query the
    // bookmark state there.
  }
}

void BookmarkBarController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted()) {
    CHECK_EQ(web_contents(), tab_strip_model_->GetActiveWebContents());
    UpdateBookmarkBarState(StateChangeReason::kTabState);
  }
}
