// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_store.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "sql/database.h"

namespace offline_pages {
namespace {

const char kPrefetchStoreFileName[] = "PrefetchStore.db";

void DeleteSync(base::FilePath path) {
  sql::Database::Delete(path);
}

}  // namespace

void PrefetchStore::Delete(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  blocking_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteSync, path.AppendASCII(kPrefetchStoreFileName)));
}

}  // namespace offline_pages
