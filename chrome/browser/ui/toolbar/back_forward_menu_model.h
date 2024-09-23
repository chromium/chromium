// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_BACK_FORWARD_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_BACK_FORWARD_MENU_MODEL_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/favicon/core/favicon_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/window_open_disposition.h"

class Browser;

namespace favicon_base {
struct FaviconImageResult;
}

namespace content {
class NavigationEntry;
class WebContents;
}

///////////////////////////////////////////////////////////////////////////////
//
// BackForwardMenuModel
//
// Interface for the showing of the dropdown menu for the Back/Forward buttons.
// Actual implementations are platform-specific.
///////////////////////////////////////////////////////////////////////////////
class BackForwardMenuModel final : public ui::MenuModel,
                                   public content::WebContentsObserver {
 public:
  // These are IDs used to identify individual UI elements within the
  // browser window using View::GetViewByID.
  enum class ModelType { kForward = 1, kBackward = 2 };

  BackForwardMenuModel(Browser* browser, ModelType model_type);

  BackForwardMenuModel(const BackForwardMenuModel&) = delete;
  BackForwardMenuModel& operator=(const BackForwardMenuModel&) = delete;

  ~BackForwardMenuModel() override;

  // ui::MenuModel:
  base::WeakPtr<ui::MenuModel> AsWeakPtr() override;
  size_t GetItemCount() const override;
  ItemType GetTypeAt(size_t index) const override;
  ui::MenuSeparatorType GetSeparatorTypeAt(size_t index) const override;
  int GetCommandIdAt(size_t index) const override;
  std::u16string GetLabelAt(size_t index) const override;
  bool IsItemDynamicAt(size_t index) const override;
  bool GetAcceleratorAt(size_t index,
                        ui::Accelerator* accelerator) const override;
  bool IsItemCheckedAt(size_t index) const override;
  int GetGroupIdAt(size_t index) const override;
  ui::ImageModel GetIconAt(size_t index) const override;
  ui::ButtonMenuItemModel* GetButtonMenuItemAt(size_t index) const override;
  bool IsEnabledAt(size_t index) const override;
  MenuModel* GetSubmenuModelAt(size_t index) const override;
  void ActivatedAt(size_t index) override;
  void ActivatedAt(size_t index, int event_flags) override;
  void MenuWillShow() override;
  void MenuWillClose() override;

  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void NavigationEntriesDeleted() override;

  // Is the item at |index| a separator?
  bool IsSeparator(size_t index) const;

 private:
  friend class BackFwdMenuModelTest;
  FRIEND_TEST_ALL_PREFIXES(BackFwdMenuModelTest, BasicCase);
  FRIEND_TEST_ALL_PREFIXES(BackFwdMenuModelTest, MaxItemsTest);
  FRIEND_TEST_ALL_PREFIXES(BackFwdMenuModelTest, ChapterStops);
  FRIEND_TEST_ALL_PREFIXES(BackFwdMenuModelTest, EscapeLabel);
  FRIEND_TEST_ALL_PREFIXES(BackFwdMenuModelTest, FaviconLoadTest);
  FRIEND_TEST_ALL_PREFIXES(BackFwdMenuModelTest, NavigationWhenMenuShownTest);
  FRIEND_TEST_ALL_PREFIXES(BackFwdMenuModelIncognitoTest, IncognitoCaseTest);
  FRIEND_TEST_ALL_PREFIXES(ChromeNavigationBrowserTest,
                           NoUserActivationSetSkipOnBackForward);

  // Requests a favicon from the FaviconService. Called by GetIconAt if the
  // NavigationEntry has an invalid favicon.
  void FetchFavicon(content::NavigationEntry* entry);

  // Callback from the favicon service.
  void OnFavIconDataAvailable(
      int navigation_entry_unique_id,
      const favicon_base::FaviconImageResult& image_result);

  // Allows the unit test to use its own dummy tab contents.
  void set_test_web_contents(content::WebContents* test_web_contents) {
    test_web_contents_ = test_web_contents;
  }

  // Returns how many history items the menu should show. For example, if the
  // navigation controller of the current tab has a current entry index of 5 and
  // forward_direction_ is false (we are the back button delegate) then this
  // function will return 5 (representing 0-4). If forward_direction_ is
  // true (we are the forward button delegate), then this function will return
  // the number of entries after 5. Note, though, that in either case it will
  // not report more than kMaxHistoryItems. The number returned also does not
  // include the separator line after the history items (nor the separator for
  // the "Show Full History" link).
  size_t GetHistoryItemCount() const;

  // Returns how many chapter-stop items the menu should show. For the
  // definition of a chapter-stop, see GetIndexOfNextChapterStop(). The number
  // returned does not include the separator lines before and after the
  // chapter-stops.
  size_t GetChapterStopCount(size_t history_items) const;

  // Finds the next chapter-stop in the NavigationEntryList starting from
  // the index specified in |start_from| and continuing in the direction
  // specified (|forward|) until either a chapter-stop is found or we reach the
  // end, in which case nullopt is returned. If |start_from| is out of bounds,
  // nullopt will also be returned. A chapter-stop is defined as the last page
  // the user browsed to within the same domain. For example, if the user's
  // homepage is Google and they navigate to Google pages G1, G2 and G3 before
  // heading over to WikiPedia for pages W1 and W2 and then back to Google for
  // pages G4 and G5 then G3, W2 and G5 are considered chapter-stops. The return
  // value from this function is an index into the NavigationEntryList vector.
  std::optional<size_t> GetIndexOfNextChapterStop(size_t start_from,
                                                  bool forward) const;

  // Finds a given chapter-stop starting at the currently active entry in the
  // NavigationEntryList vector advancing first forward or backward by |offset|
  // (depending on the direction specified in parameter |forward|). It also
  // allows you to skip chapter-stops by specifying a positive value for |skip|.
  // Example: FindChapterStop(5, false, 3) starts with the currently active
  // index, subtracts 5 from it and then finds the fourth chapter-stop before
  // that index (skipping the first 3 it finds).
  // Example: FindChapterStop(0, true, 0) is functionally equivalent to
  // calling GetIndexOfNextChapterStop(GetCurrentEntryIndex(), true).
  //
  // NOTE: Both |offset| and |skip| must be non-negative. The return value from
  // this function is an index into the NavigationEntryList vector. If |offset|
  // is out of bounds or if we skip too far (run out of chapter-stops) this
  // function returns nullopt.
  std::optional<size_t> FindChapterStop(size_t offset,
                                        bool forward,
                                        size_t skip) const;

  // How many items (max) to show in the back/forward history menu dropdown.
  static const size_t kMaxHistoryItems;

  // How many chapter-stops (max) to show in the back/forward dropdown list.
  static const size_t kMaxChapterStops;

  // Takes a menu item index as passed in through one of the menu delegate
  // functions and converts it into an index into the NavigationEntryList
  // vector. |index| can point to a separator, or the
  // "Show Full History" link in which case this function returns nullopt.
  std::optional<size_t> MenuIndexToNavEntryIndex(size_t index) const;

  // Does the item have a command associated with it?
  bool ItemHasCommand(size_t index) const;

  // Returns true if there is an icon for this menu item.
  bool ItemHasIcon(size_t index) const;

  // Allow the unit test to use the "Show Full History" label.
  std::u16string GetShowFullHistoryLabel() const;

  // Looks up a NavigationEntry by menu id.
  content::NavigationEntry* GetNavigationEntry(size_t index) const;

  // Retrieves the WebContents pointer to use, which is either the one that
  // the unit test sets (using set_test_web_contents) or the one from
  // the browser window.
  content::WebContents* GetWebContents() const;

  // Build a string version of a user action on this menu, used as an
  // identifier for logging user behavior.
  // E.g. BuildActionName("Click", 2) returns "BackMenu_Click2".
  // An index of nullopt means no index.
  std::string BuildActionName(const std::string& name,
                              std::optional<size_t> index) const;

  // Returns true if "Show Full History" item should be visible. It is visible
  // only in outside incognito mode.
  bool ShouldShowFullHistoryBeVisible() const;

  const raw_ptr<Browser> browser_;

  // The unit tests will provide their own WebContents to use.
  raw_ptr<content::WebContents> test_web_contents_ = nullptr;

  // Represents whether this is the delegate for the forward button or the
  // back button.
  const ModelType model_type_;

  // Keeps track of which favicons have already been requested from the history
  // to prevent duplicate requests, identified by
  // NavigationEntry->GetUniqueID().
  base::flat_set<int> requested_favicons_;

  // Used for loading favicons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // The timestamp of the previous opening of the BackForwardMenuModel.
  // This is used to calculate the time spent between the model's opening and
  // the clicking of a menu item.
  // Note: This timestamp will be set from `MenuWillShow()` and will be accessed
  // from `MenuWillClose()` and `ActivateAt()`. Since it will be read once or
  // twice depending on whether any of the menu item is activated, the timestamp
  // will not be reset.
  std::optional<base::TimeTicks> menu_model_open_timestamp_;

  base::WeakPtrFactory<BackForwardMenuModel> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_BACK_FORWARD_MENU_MODEL_H_
