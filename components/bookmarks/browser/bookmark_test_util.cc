// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_test_util.h"

#include <iostream>

#include "base/logging.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace bookmarks {

void PrintTo(const BookmarkNode& node, std::ostream* os) {
  if (node.is_root()) {
    *os << "root bookmark node";
    return;
  }

  if (node.is_permanent_node())
    *os << "permanent ";

  switch (node.type()) {
    case BookmarkNode::URL:
      *os << "bookmark URL " << node.url() << " title \"" << node.GetTitle()
          << "\" UUID " << node.uuid() << " icon ";
      if (node.icon_url())
        *os << *node.icon_url();
      else
        *os << "null";
      break;
    case BookmarkNode::FOLDER:
      *os << "bookmark folder with title \"" << node.GetTitle() << "\" UUID "
          << node.uuid();
      break;
    case BookmarkNode::BOOKMARK_BAR:
      *os << "BOOKMARK_BAR folder";
      break;
    case BookmarkNode::OTHER_NODE:
      *os << "OTHER_NODE folder";
      break;
    case BookmarkNode::MOBILE:
      *os << "MOBILE folder";
      break;
  }
}

void PrintTo(const BookmarkNode* node, std::ostream* os) {
  if (node) {
    PrintTo(*node, os);
  } else {
    *os << "null";
  }
}

}  // namespace bookmarks
