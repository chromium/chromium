// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/bookmark_counter.h"

#include "base/bind.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace {

int CountBookmarksFromNode(const bookmarks::BookmarkNode* node,
                           base::Time period_start) {
  int count = 0;
  if (node->is_url()) {
    if (node->date_added() >= period_start)
      ++count;
  } else {
    for (const auto& child : node->children())
      count += CountBookmarksFromNode(child.get(), period_start);
  }
  return count;
}

using BookmarkModelCallback =
    base::OnceCallback<void(const bookmarks::BookmarkModel*)>;

// This class waits for the |bookmark_model| to load, then executes |callback|
// and destroys itself afterwards.
class BookmarkModelHelper : public bookmarks::BaseBookmarkModelObserver {
 public:
  BookmarkModelHelper(bookmarks::BookmarkModel* bookmark_model,
                      BookmarkModelCallback callback)
      : bookmark_model_(bookmark_model), callback_(std::move(callback)) {
    DCHECK(!bookmark_model_->loaded());
    bookmark_model_->AddObserver(this);
  }

  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override {
    std::move(callback_).Run(model);
    delete this;
  }

  void BookmarkModelBeingDeleted(
      bookmarks::BookmarkModel* bookmark_model) override {
    // Don't leak this instance if the BookmarkModel never loads.
    delete this;
  }

  void BookmarkModelChanged() override {}

 private:
  ~BookmarkModelHelper() override { bookmark_model_->RemoveObserver(this); }

  bookmarks::BookmarkModel* bookmark_model_;
  BookmarkModelCallback callback_;
};

}  // namespace

namespace browsing_data {

const char BookmarkCounter::kPrefName[] =
    "browser.clear_data.fake.pref.bookmarks";

BookmarkCounter::BookmarkCounter(bookmarks::BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model) {
  DCHECK(bookmark_model);
}

BookmarkCounter::~BookmarkCounter() {}

void BookmarkCounter::OnInitialized() {}

const char* BookmarkCounter::GetPrefName() const {
  return kPrefName;
}

void BookmarkCounter::Count() {
  if (bookmark_model_->loaded()) {
    CountBookmarks(bookmark_model_);
  } else {
    new BookmarkModelHelper(bookmark_model_,
                            base::BindOnce(&BookmarkCounter::CountBookmarks,
                                           weak_ptr_factory_.GetWeakPtr()));
  }
}

void BookmarkCounter::CountBookmarks(
    const bookmarks::BookmarkModel* bookmark_model) {
  base::Time start = GetPeriodStart();
  int count =
      CountBookmarksFromNode(bookmark_model->bookmark_bar_node(), start) +
      CountBookmarksFromNode(bookmark_model->other_node(), start) +
      CountBookmarksFromNode(bookmark_model->mobile_node(), start);
  ReportResult(count);
}

}  // namespace browsing_data
