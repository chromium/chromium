// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node_data.h"

#include "base/notreached.h"

namespace bookmarks {

// static
bool BookmarkNodeData::ClipboardContainsBookmarks() {
  NOTREACHED();
}

void BookmarkNodeData::WriteToClipboard(bool is_off_the_record) {
  NOTREACHED();
}

// static
void BookmarkNodeData::ReadFromClipboard(
    ui::ClipboardBuffer buffer,
    base::OnceCallback<void(std::unique_ptr<BookmarkNodeData>)> callback) {
  NOTREACHED();
}

}  // namespace bookmarks
