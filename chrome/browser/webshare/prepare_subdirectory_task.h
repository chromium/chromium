// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_PREPARE_SUBDIRECTORY_TASK_H_
#define CHROME_BROWSER_WEBSHARE_PREPARE_SUBDIRECTORY_TASK_H_

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

namespace webshare {

class PrepareSubDirectoryTask {
 public:
  PrepareSubDirectoryTask(std::vector<base::FilePath> subdirectories,
                          blink::mojom::ShareService::ShareCallback callback);
  PrepareSubDirectoryTask(const PrepareSubDirectoryTask&) = delete;
  PrepareSubDirectoryTask& operator=(const PrepareSubDirectoryTask) = delete;
  ~PrepareSubDirectoryTask();

  void Start();

 private:
  static base::File::Error PrepareSubDirectories(
      std::vector<base::FilePath> subdirectories);

  void OnPrepareSubDirectories(base::File::Error result);

  std::vector<base::FilePath> subdirectories_;
  blink::mojom::ShareService::ShareCallback callback_;
  base::WeakPtrFactory<PrepareSubDirectoryTask> weak_ptr_factory_{this};
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_PREPARE_SUBDIRECTORY_TASK_H_
