// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_TYPED_COUNT_SORTER_H_
#define COMPONENTS_BOOKMARKS_BROWSER_TYPED_COUNT_SORTER_H_

#include "base/macros.h"
#include "components/bookmarks/browser/titled_url_node_sorter.h"

namespace bookmarks {

class BookmarkClient;

// A sort method for sorting a set of bookmarks in decreasing order according to
// the number of times each bookmark's URL was typed into the omnibox. Duplicate
// matches are removed.
class TypedCountSorter : public TitledUrlNodeSorter {
 public:
  explicit TypedCountSorter(BookmarkClient* client);
  ~TypedCountSorter() override;

  // TitledUrlNodeSorter
  void SortMatches(const TitledUrlNodeSet& matches,
                   TitledUrlNodes* sorted_nodes) const override;

 private:
  BookmarkClient* client_;

  DISALLOW_COPY_AND_ASSIGN(TypedCountSorter);
};

}

#endif  // COMPONENTS_BOOKMARKS_BROWSER_TYPED_COUNT_SORTER_H_
