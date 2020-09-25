// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_CHROMEOS_PREPARE_DIRECTORY_TASK_H_
#define CHROME_BROWSER_WEBSHARE_CHROMEOS_PREPARE_DIRECTORY_TASK_H_

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

namespace webshare {

// Creates a directory to hold files being shared to SharesheetService.
class PrepareDirectoryTask {
 public:
  PrepareDirectoryTask(base::FilePath directory,
                       blink::mojom::ShareService::ShareCallback callback);
  PrepareDirectoryTask(const PrepareDirectoryTask&) = delete;
  PrepareDirectoryTask& operator=(const PrepareDirectoryTask&) = delete;
  ~PrepareDirectoryTask();

  // Launches the task. |callback_| will be called on the original (UI) thread
  // when the task completes.
  void Start();

 private:
  // Runs on a thread where blocking is permitted.
  static base::File::Error PrepareDirectory(base::FilePath directory);

  void OnPrepareDirectory(base::File::Error result);

  const base::FilePath directory_;
  blink::mojom::ShareService::ShareCallback callback_;

  base::WeakPtrFactory<PrepareDirectoryTask> weak_ptr_factory_{this};
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_CHROMEOS_PREPARE_DIRECTORY_TASK_H_
