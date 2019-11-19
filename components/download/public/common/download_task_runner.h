// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_TASK_RUNNER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_TASK_RUNNER_H_

#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/download/public/common/download_export.h"

namespace download {

// Returns the task runner used to save files and do other blocking operations.
COMPONENTS_DOWNLOAD_EXPORT scoped_refptr<base::SequencedTaskRunner>
GetDownloadTaskRunner();

// Sets the task runner used to perform network IO.
COMPONENTS_DOWNLOAD_EXPORT void SetIOTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

// Returns the task runner used to save files and do other blocking operations.
COMPONENTS_DOWNLOAD_EXPORT scoped_refptr<base::SingleThreadTaskRunner>
GetIOTaskRunner();

// Sets the task runner for download DB, must be called on UI thread.
COMPONENTS_DOWNLOAD_EXPORT void SetDownloadDBTaskRunnerForTesting(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner);

// Gets the task runner for download DB, must be called on UI thread.
COMPONENTS_DOWNLOAD_EXPORT scoped_refptr<base::SequencedTaskRunner>
GetDownloadDBTaskRunnerForTesting();

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_TASK_RUNNER_H_
