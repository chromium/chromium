// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_LOAD_WAITER_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_LOAD_WAITER_H_

#include "base/functional/callback_forward.h"

namespace bookmarks {

class BookmarkModel;

// Schedules a task to run after the bookmark model loads. If the model is
// already loaded, the callback is posted to the current sequence. The callback
// will not run if BookmarkModel is deleted before load completes.
void ScheduleCallbackOnBookmarkModelLoad(BookmarkModel& model,
                                         base::OnceClosure callback);

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_LOAD_WAITER_H_
