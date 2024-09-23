// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node_data.h"

#include "base/notreached.h"

namespace bookmarks {

// static
bool BookmarkNodeData::ClipboardContainsBookmarks() {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void BookmarkNodeData::WriteToClipboard(bool is_off_the_record) {
  NOTREACHED_IN_MIGRATION();
}

bool BookmarkNodeData::ReadFromClipboard(ui::ClipboardBuffer buffer) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace bookmarks
