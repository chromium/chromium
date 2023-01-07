// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_NODE_SORTER_H_
#define COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_NODE_SORTER_H_

#include <vector>

#include "base/containers/flat_set.h"

namespace bookmarks {

class TitledUrlNode;

class TitledUrlNodeSorter {
 public:
  using TitledUrlNodes = std::vector<const TitledUrlNode*>;
  using TitledUrlNodeSet = base::flat_set<const TitledUrlNode*>;

  virtual ~TitledUrlNodeSorter() {}

  // Sorts |matches| in an implementation-specific way, placing the results in
  // |sorted_nodes|.
  virtual void SortMatches(const TitledUrlNodeSet& matches,
                           TitledUrlNodes* sorted_nodes) const = 0;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_NODE_SORTER_H_
