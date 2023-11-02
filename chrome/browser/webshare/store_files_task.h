// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_STORE_FILES_TASK_H_
#define CHROME_BROWSER_WEBSHARE_STORE_FILES_TASK_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/webshare/store_file_task.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

namespace webshare {

// Stores shared |files| using the specified |filenames|.
class StoreFilesTask {
 public:
  StoreFilesTask(std::vector<base::FilePath> filenames,
                 std::vector<blink::mojom::SharedFilePtr> files,
                 uint64_t available_space,
                 blink::mojom::ShareService::ShareCallback callback);
  StoreFilesTask(const StoreFilesTask&) = delete;
  StoreFilesTask& operator=(const StoreFilesTask&) = delete;
  ~StoreFilesTask();

  // Takes ownership of the StoreFilesTask. |callback_| will be called on the
  // original (UI) thread when the task completes.
  void Start();

 private:
  // Runs on |file_task_runner_| thread.
  void OnStoreFile(blink::mojom::ShareError result);

  std::vector<base::FilePath> filenames_;
  std::vector<blink::mojom::SharedFilePtr> files_;
  uint64_t available_space_;
  blink::mojom::ShareService::ShareCallback callback_;
  unsigned index_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  std::unique_ptr<StoreFileTask> store_file_task_;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_STORE_FILES_TASK_H_
