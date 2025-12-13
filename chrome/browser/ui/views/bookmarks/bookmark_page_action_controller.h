// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_PAGE_ACTION_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_observer.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "components/prefs/pref_member.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class PrefService;

namespace content {
class Page;
class WebContents;
enum class Visibility;
}  // namespace content

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

// Controller for managing the state of the bookmark star icon.
class BookmarkPageActionController : public BookmarkTabHelperObserver,
                                     public tabs::ContentsObservingTabFeature {
 public:
  DECLARE_USER_DATA(BookmarkPageActionController);

  BookmarkPageActionController(
      tabs::TabInterface& tab,
      PrefService* pref_service,
      page_actions::PageActionController& page_action_controller);

  ~BookmarkPageActionController() override;

  BookmarkPageActionController(const BookmarkPageActionController&) = delete;
  BookmarkPageActionController& operator=(const BookmarkPageActionController&) =
      delete;

  static BookmarkPageActionController* From(tabs::TabInterface* tab);

  // Reports to a histogram indicating how the bookmark page action was
  // triggered.
  static void RecordPageActionExecution(
      page_actions::PageActionTrigger trigger);

  // BookmarkTabHelperObserver
  void URLStarredChanged(content::WebContents* web_contents,
                         bool starred) override;

 private:
  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // tabs::ContentsObservingTabFeature
  void OnDiscardContents(tabs::TabInterface* tab,
                         content::WebContents* old_contents,
                         content::WebContents* new_contents) override;

  void ObserveBookmarkTabHelper(content::WebContents* contents);

  void UpdatePageActionVisibility();
  bool ShouldShowPageAction() const;
  void SetStarred(bool starred);

  const raw_ref<page_actions::PageActionController> page_action_controller_;
  BooleanPrefMember edit_bookmarks_enabled_;

  base::ScopedObservation<BookmarkTabHelper, BookmarkTabHelperObserver>
      tab_helper_observation_{this};

  ui::ScopedUnownedUserData<BookmarkPageActionController>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_PAGE_ACTION_CONTROLLER_H_
