// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VALUE_STORE_VALUE_STORE_TASK_RUNNER_H_
#define COMPONENTS_VALUE_STORE_VALUE_STORE_TASK_RUNNER_H_

#include "base/memory/ref_counted.h"

namespace base {
class SequencedTaskRunner;
}

namespace value_store {

// Returns the singleton instance of the task runner to be used for value store
// tasks that read, modify, or delete files.
scoped_refptr<base::SequencedTaskRunner> GetValueStoreTaskRunner();

}  // namespace value_store

#endif  // COMPONENTS_VALUE_STORE_VALUE_STORE_TASK_RUNNER_H_
