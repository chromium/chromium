// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_H_

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/offline_pages/task/sql_store_base.h"

namespace offline_pages {

// TODO(crbug.com/1424920): This was deleted, what remains is a function to
// delete the database. Remove this code after it's been live for one milestone.
class PrefetchStore {
 public:
  static void Delete(
      const base::FilePath& path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_H_
