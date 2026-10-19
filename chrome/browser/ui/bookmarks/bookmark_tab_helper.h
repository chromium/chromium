// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BookmarkTabHelperObserver;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}  // namespace tabs

// Per-tab class to manage bookmarks.
class BookmarkTabHelper : public bookmarks::BaseBookmarkModelObserver,
                          public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(BookmarkTabHelper);

  explicit BookmarkTabHelper(tabs::TabInterface& tab);

  BookmarkTabHelper(const BookmarkTabHelper&) = delete;
  BookmarkTabHelper& operator=(const BookmarkTabHelper&) = delete;

  ~BookmarkTabHelper() override;

  static BookmarkTabHelper* From(tabs::TabInterface* tab);
  static BookmarkTabHelper* FromWebContents(content::WebContents* web_contents);

  void DiscardContents(content::WebContents* new_contents);

  bool is_starred() const { return is_starred_; }

  void AddObserver(BookmarkTabHelperObserver* observer);
  void RemoveObserver(BookmarkTabHelperObserver* observer);
  bool HasObserver(BookmarkTabHelperObserver* observer) const;

 private:
  // Updates the starred state from the BookmarkModel. If the state has changed,
  // the delegate is notified.
  void UpdateStarredStateForCurrentURL();

  // Overridden from bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;

  // Overridden from content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Whether the current URL is starred.
  bool is_starred_ = false;

  raw_ptr<bookmarks::BookmarkModel, DanglingUntriaged> bookmark_model_ =
      nullptr;

  // A list of observers notified when the url starred state changes.
  base::ObserverList<BookmarkTabHelperObserver>::Unchecked observers_;

  ui::ScopedUnownedUserData<BookmarkTabHelper> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_
