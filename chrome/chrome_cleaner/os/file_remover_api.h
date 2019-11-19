// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_FILE_REMOVER_API_H_
#define CHROME_CHROME_CLEANER_OS_FILE_REMOVER_API_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"

namespace chrome_cleaner {

// This class is used as a wrapper around the OS calls to remove files. This
// allows test to more easily provide mock for these OS calls.
class FileRemoverAPI {
 public:
  enum class DeletionValidationStatus {
    ALLOWED,
    FORBIDDEN,

    // Path is unsafe (eg. a UNC path that could be a network share). Do not
    // even call functions like SanitizePath or NormalizePath on it.
    UNSAFE,
  };
  // Callback used for the asynchronous versions of RemoveNow
  // and RegisterPostRebootRemoval.
  typedef base::OnceCallback<void(bool)> DoneCallback;

  virtual ~FileRemoverAPI() {}

  // Remove file at |path| from the user's disk. Return false on failure.
  virtual void RemoveNow(const base::FilePath& path,
                         DoneCallback callback) const = 0;

  // Register the file in |file_path| to be deleted after a machine reboot.
  // Return false on failure.
  virtual void RegisterPostRebootRemoval(const base::FilePath& file_path,
                                         DoneCallback callback) const = 0;

  // Check if file at |path| is not whitelisted and can be deleted.
  virtual DeletionValidationStatus CanRemove(
      const base::FilePath& path) const = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_FILE_REMOVER_API_H_
