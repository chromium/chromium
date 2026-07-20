// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARKS_VIEW_OBSERVER_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARKS_VIEW_OBSERVER_H_

#include <vector>

#include "base/observer_list_types.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"

namespace bookmarks_api {

class BookmarksView;

class BookmarksViewObserver : public base::CheckedObserver {
 public:
  // Invoked when bookmark events occur in the view.
  virtual void OnBookmarksEvents(
      BookmarksView* view,
      const std::vector<mojom::BookmarksEventPtr>& events) = 0;

  // Invoked when the observed BookmarksView is being deleted.
  virtual void OnBookmarksViewBeingDeleted(BookmarksView* view) {}

 protected:
  ~BookmarksViewObserver() override = default;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARKS_VIEW_OBSERVER_H_
