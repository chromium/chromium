// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class TabStripModel;

namespace content {
class NavigationHandle;
}  // namespace content

// Manages bookmark bar of its associated Browser instance.
class BookmarkBarController : public TabStripModelObserver,
                              public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(BookmarkBarController);

  // Describes where the bookmark bar state change originated from.
  enum class StateChangeReason {
    // From the constructor.
    kInit,
    // Change is the result of the active tab changing.
    kTabSwitch,
    // Change is the result of the bookmark bar pref changing.
    kPrefChange,
    // Change is the result of a state change in the active tab.
    kTabState,
    // Change is the result of window toggling in/out of fullscreen mode.
    kToggleFullscreen,
    // Change is the result of switching the option of showing toolbar in full
    // screen. Only used on Mac.
    kToolbarOptionChange,
    // Change is the result of a force show reason.
    kForceShow,
    // Change is the result of a split tab being created or removed.
    kSplitTabChange,
  };

  // Represents the reasons for force showing bookmark bar.
  enum ForceShowFlag {
    kNone = 0,
    kTabGroupsTutorialActive = 1 << 0,
    kTabGroupSaved = 1 << 1,
  };

  BookmarkBarController(BrowserWindowInterface& browser,
                        TabStripModel& tab_strip_model);
  ~BookmarkBarController() override;

  BookmarkBarController(const BookmarkBarController&) = delete;
  BookmarkBarController& operator=(const BookmarkBarController&) = delete;

  static BookmarkBarController* From(
      BrowserWindowInterface* browser_window_interface);
  static const BookmarkBarController* From(
      const BrowserWindowInterface* browser_window_interface);

  // Returns the current state of the bookmark bar.
  BookmarkBar::State bookmark_bar_state() const { return bookmark_bar_state_; }

  // Sets or clears the flags to force showing bookmark bar.
  void SetForceShowBookmarkBarFlag(ForceShowFlag flag);
  void ClearForceShowBookmarkBarFlag(ForceShowFlag flag);

  // Resets |bookmark_bar_state_| based on the active tab. Notifies the
  // BrowserWindow if necessary.
  void UpdateBookmarkBarState(StateChangeReason reason);

  // TabStripModelObserver implementation:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;

  // content::WebContentsObserver implementation:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Checks whether bookmark bar should be shown for the current browser.
  bool ShouldShowBookmarkBar() const;

  const raw_ref<BrowserWindowInterface> browser_;
  const raw_ref<TabStripModel> tab_strip_model_;

  BookmarkBar::State bookmark_bar_state_ = BookmarkBar::HIDDEN;

  int force_show_bookmark_bar_flags_ = ForceShowFlag::kNone;

  PrefChangeRegistrar pref_change_registrar_;

  ui::ScopedUnownedUserData<BookmarkBarController> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BAR_CONTROLLER_H_
