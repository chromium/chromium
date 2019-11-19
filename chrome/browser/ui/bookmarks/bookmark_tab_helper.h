// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_

#include "base/macros.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class BookmarkTabHelperObserver;

namespace bookmarks {
struct BookmarkNodeData;
}

namespace content {
class WebContents;
}

// Per-tab class to manage bookmarks.
class BookmarkTabHelper
    : public bookmarks::BaseBookmarkModelObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<BookmarkTabHelper> {
 public:
  // Interface for forwarding bookmark drag and drop to extenstions.
  class BookmarkDrag {
   public:
    virtual void OnDragEnter(const bookmarks::BookmarkNodeData& data) = 0;
    virtual void OnDragOver(const bookmarks::BookmarkNodeData& data) = 0;
    virtual void OnDragLeave(const bookmarks::BookmarkNodeData& data) = 0;
    virtual void OnDrop(const bookmarks::BookmarkNodeData& data) = 0;

   protected:
    virtual ~BookmarkDrag() {}
  };

  ~BookmarkTabHelper() override;

  // It is up to callers to call set_bookmark_drag_delegate(NULL) when
  // |bookmark_drag| is deleted since this class does not take ownership of
  // |bookmark_drag|.
  void set_bookmark_drag_delegate(BookmarkDrag* bookmark_drag) {
    bookmark_drag_ = bookmark_drag;
  }
  BookmarkDrag* bookmark_drag_delegate() { return bookmark_drag_; }

  bool is_starred() const { return is_starred_; }

  bool ShouldShowBookmarkBar() const;

  void AddObserver(BookmarkTabHelperObserver* observer);
  void RemoveObserver(BookmarkTabHelperObserver* observer);
  bool HasObserver(BookmarkTabHelperObserver* observer) const;

 private:
  friend class content::WebContentsUserData<BookmarkTabHelper>;

  explicit BookmarkTabHelper(content::WebContents* web_contents);

  // Updates the starred state from the BookmarkModel. If the state has changed,
  // the delegate is notified.
  void UpdateStarredStateForCurrentURL();

  // Overridden from bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;

  // Overridden from content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;

  // Whether the current URL is starred.
  bool is_starred_;

  bookmarks::BookmarkModel* bookmark_model_;

  // A list of observers notified when when the url starred changed.
  base::ObserverList<BookmarkTabHelperObserver>::Unchecked observers_;

  // The BookmarkDrag is used to forward bookmark drag and drop events to
  // extensions.
  BookmarkDrag* bookmark_drag_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(BookmarkTabHelper);
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TAB_HELPER_H_
