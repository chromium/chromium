// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/in_memory_url_index_test_util.h"

#include "base/run_loop.h"
#include "components/omnibox/browser/history_index_restore_observer.h"
#include "components/omnibox/browser/in_memory_url_index.h"

void BlockUntilInMemoryURLIndexIsRefreshed(InMemoryURLIndex* index) {
  if (!index || index->restored())
    return;
  base::RunLoop run_loop;
  HistoryIndexRestoreObserver observer(run_loop.QuitClosure());
  index->set_restore_cache_observer(&observer);
  run_loop.Run();
  index->set_restore_cache_observer(nullptr);
  DCHECK(index->restored());
}
