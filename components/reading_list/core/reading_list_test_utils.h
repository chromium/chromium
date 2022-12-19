// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_TEST_UTILS_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_TEST_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "components/reading_list/core/reading_list_model_observer.h"

class GURL;
class ReadingListModel;

// ReadingListLoadObserver is used to observe the ReadingListModel passed in the
// constructor for the ReadingListModelLoaded event.
class ReadingListLoadObserver : public ReadingListModelObserver {
 public:
  explicit ReadingListLoadObserver(ReadingListModel* model);
  ReadingListLoadObserver(const ReadingListLoadObserver&) = delete;
  ReadingListLoadObserver& operator=(const ReadingListLoadObserver&) = delete;
  ~ReadingListLoadObserver() override;

  void Wait();

 private:
  // ReadingListModelObserver:
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelBeganBatchUpdates(
      const ReadingListModel* model) override {}
  void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) override {}
  void ReadingListModelBeingShutdown(const ReadingListModel* model) override {}
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override {}
  void ReadingListWillRemoveEntry(const ReadingListModel* model,
                                  const GURL& url) override {}
  void ReadingListWillMoveEntry(const ReadingListModel* model,
                                const GURL& url) override {}
  void ReadingListDidMoveEntry(const ReadingListModel* model,
                               const GURL& url) override {}
  void ReadingListWillAddEntry(const ReadingListModel* model,
                               const ReadingListEntry& entry) override {}
  void ReadingListDidAddEntry(const ReadingListModel* model,
                              const GURL& url,
                              reading_list::EntrySource source) override {}
  void ReadingListWillUpdateEntry(const ReadingListModel* model,
                                  const GURL& url) override {}
  void ReadingListDidApplyChanges(ReadingListModel* model) override {}

  const raw_ptr<ReadingListModel> model_;
  base::RunLoop run_loop_;
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_TEST_UTILS_H_
