// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_PREPARE_DIRECTORY_TASK_H_
#define CHROME_BROWSER_WEBSHARE_PREPARE_DIRECTORY_TASK_H_

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

namespace webshare {

// Creates a directory to hold files being shared to SharesheetService.
// Deletes any old files remaining from past shares.
class PrepareDirectoryTask {
 public:
  static constexpr base::TimeDelta kSharedFileLifetime = base::Minutes(10);

  PrepareDirectoryTask(base::FilePath directory, uint64_t required_space);
  PrepareDirectoryTask(base::FilePath directory,
                       uint64_t required_space,
                       blink::mojom::ShareService::ShareCallback callback);
  PrepareDirectoryTask(const PrepareDirectoryTask&) = delete;
  PrepareDirectoryTask& operator=(const PrepareDirectoryTask&) = delete;
  ~PrepareDirectoryTask();

  // Deletes specified |file_paths| after waiting |delay|.
  static void ScheduleSharedFileDeletion(std::vector<base::FilePath> file_paths,
                                         base::TimeDelta delay);

  // Launches the task. |callback_| will be called on the original (UI) thread
  // when the task completes.
  void Start();

  using PrepareDirectoryCallback =
      base::OnceCallback<void(base::File::Error result)>;

  // Launches the task. Supplied |callback| will be called on the UI thread
  // when the PrepareDirectory task completes.
  void StartWithCallback(PrepareDirectoryCallback callback);

 private:
  // Runs on a thread where blocking is permitted.
  static base::File::Error PrepareDirectory(base::FilePath directory,
                                            uint64_t required_space);

  void OnPrepareDirectory(base::File::Error result);

  const base::FilePath directory_;
  const uint64_t required_space_;
  blink::mojom::ShareService::ShareCallback callback_;

  base::WeakPtrFactory<PrepareDirectoryTask> weak_ptr_factory_{this};
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_PREPARE_DIRECTORY_TASK_H_
