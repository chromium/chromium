// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"

#include "components/bookmarks/browser/bookmark_model.h"

namespace bookmarks {

ScopedGroupBookmarkActions::ScopedGroupBookmarkActions(BookmarkModel* model)
    : model_(model) {
  if (model_)
    model_->BeginGroupedChanges();
}

ScopedGroupBookmarkActions::~ScopedGroupBookmarkActions() {
  if (model_)
    model_->EndGroupedChanges();
}

}  // namespace bookmarks
