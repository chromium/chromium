// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_HISTORY_INDEX_RESTORE_OBSERVER_H_
#define COMPONENTS_OMNIBOX_BROWSER_HISTORY_INDEX_RESTORE_OBSERVER_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "components/omnibox/browser/in_memory_url_index.h"

// HistoryIndexRestoreObserver is used when blocking until the InMemoryURLIndex
// finishes restoring. As soon as the InMemoryURLIndex finishes restoring the
// provided OnceClosure is invoked.
class HistoryIndexRestoreObserver
    : public InMemoryURLIndex::RestoreCacheObserver {
 public:
  explicit HistoryIndexRestoreObserver(base::OnceClosure task);
  ~HistoryIndexRestoreObserver() override;
  HistoryIndexRestoreObserver(const HistoryIndexRestoreObserver&) = delete;
  HistoryIndexRestoreObserver& operator=(const HistoryIndexRestoreObserver&) =
      delete;

  bool succeeded() const { return succeeded_; }

  // RestoreCacheObserver implementation.
  void OnCacheRestoreFinished(bool success) override;

 private:
  base::OnceClosure task_;
  bool succeeded_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_HISTORY_INDEX_RESTORE_OBSERVER_H_
