// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_COUNTERS_BOOKMARK_COUNTER_H_
#define COMPONENTS_BROWSING_DATA_CORE_COUNTERS_BOOKMARK_COUNTER_H_

#include "base/memory/raw_ptr.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

namespace bookmarks {
class BookmarkModel;
}

namespace browsing_data {

class BookmarkCounter : public browsing_data::BrowsingDataCounter {
 public:
  // This is not a registered preference. It is here because a pref_name
  // is required for mapping a counter to ui elements.
  static const char kPrefName[];

  explicit BookmarkCounter(bookmarks::BookmarkModel* bookmark_model);
  ~BookmarkCounter() override;

  void OnInitialized() override;

  const char* GetPrefName() const override;

 private:
  void Count() override;
  void CountBookmarks(const bookmarks::BookmarkModel* bookmark_model);

  raw_ptr<bookmarks::BookmarkModel, FlakyDanglingUntriaged> bookmark_model_;
  base::WeakPtrFactory<BookmarkCounter> weak_ptr_factory_{this};
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_COUNTERS_BOOKMARK_COUNTER_H_
