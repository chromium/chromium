// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_CHROMEOS_STORE_FILES_TASK_H_
#define CHROME_BROWSER_WEBSHARE_CHROMEOS_STORE_FILES_TASK_H_

#include <vector>

#include "base/files/file_path.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "content/public/browser/browser_context.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

namespace webshare {

// Stores shared |files| using the specified |filenames|.
class StoreFilesTask {
 public:
  StoreFilesTask(content::BrowserContext::BlobContextGetter blob_context_getter,
                 std::vector<base::FilePath> filenames,
                 std::vector<blink::mojom::SharedFilePtr> files,
                 blink::mojom::ShareService::ShareCallback callback);
  StoreFilesTask(const StoreFilesTask&) = delete;
  StoreFilesTask& operator=(const StoreFilesTask&) = delete;
  ~StoreFilesTask();

  // Takes ownership of the StoreFilesTask. |callback_| will be called on the
  // original (UI) thread when the task completes.
  void Start();

 private:
  // Runs on the IO thread.
  void OnProgress(storage::mojom::WriteBlobToFileResult result);

  content::BrowserContext::BlobContextGetter blob_context_getter_;
  std::vector<base::FilePath> filenames_;
  std::vector<blink::mojom::SharedFilePtr> files_;
  blink::mojom::ShareService::ShareCallback callback_;
  unsigned index_;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_CHROMEOS_STORE_FILES_TASK_H_
