// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_UTIL_H_
#define CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_UTIL_H_

#include <stdint.h>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/clipboard/clipboard.h"

namespace base {
class FilePath;
}  // namespace base

namespace clipboard_util {

// Reads a local file and then copies that file to the system clipboard. This
// should not be run on the UI Thread as it performs blocking IO.
void ReadFileAndCopyToClipboardLocal(const base::FilePath& local_file);

// Takes the content of a PNG file, decodes it and copies it to the system
// clipboard.
//
// `clipboard_sequence` - Clipboard version to determine whether the clipboard
// state has changed. An empty token is used to specify an invalid sequence.
// `png_data` - The image we want to copy to the clipboard as a string.
void DecodeImageFileAndCopyToClipboard(
    ui::ClipboardSequenceNumberToken clipboard_sequence,
    std::string png_data);

}  // namespace clipboard_util

#endif  // CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_UTIL_H_
