// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_index_restore_observer.h"

HistoryIndexRestoreObserver::HistoryIndexRestoreObserver(base::OnceClosure task)
    : task_(std::move(task)), succeeded_(false) {}

HistoryIndexRestoreObserver::~HistoryIndexRestoreObserver() {}

void HistoryIndexRestoreObserver::OnCacheRestoreFinished(bool success) {
  succeeded_ = success;
  std::move(task_).Run();
}
