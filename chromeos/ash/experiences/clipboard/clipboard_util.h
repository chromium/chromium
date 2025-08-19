// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_CLIPBOARD_CLIPBOARD_UTIL_H_
#define CHROMEOS_ASH_EXPERIENCES_CLIPBOARD_CLIPBOARD_UTIL_H_

#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}  // namespace base

namespace clipboard_util {

enum class ReadFileAndCopyToClipboardResult {
  kSuccess,
  kFailedToReadFile,
  kFailedToDecodeImage,
};

using ReadFileAndCopyToClipboardCallback =
    base::OnceCallback<void(ReadFileAndCopyToClipboardResult)>;

// Reads a local file and then copies that file to the system clipboard.
// This posts a task to the thread pool. `callback` will be posted to the
// original task runner.
void ReadFileAndCopyToClipboard(const base::FilePath& local_file,
                                ReadFileAndCopyToClipboardCallback callback);

}  // namespace clipboard_util

#endif  // CHROMEOS_ASH_EXPERIENCES_CLIPBOARD_CLIPBOARD_UTIL_H_
