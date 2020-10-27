// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CLIPBOARD_UTIL_H_
#define CHROME_BROWSER_UI_ASH_CLIPBOARD_UTIL_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
namespace base {
class FilePath;
}  // namespace base

namespace clipboard_util {

// Reads a local file and then copies that file to the system clipboard. This
// should not be run on the UI Thread as it performs blocking IO.
void ReadFileAndCopyToClipboardLocal(const base::FilePath& local_file);

// Takes an image file as a string and copies it to the system clipboard.
//
// `clipboard_sequence` - Clipboard version to determine whether the clipboard
// state has changed. A sequence of 0 is used to specify an invalid sequence.
// `maintain_clipboard` - Used to determine whether or not we care about
// maintaining the clipboard state or not. If this value is false, it is okay to
// pass a `clipboard_sequence` of 0.
// `png_data` - The image we want to copy to the clipboard as a string.
// `callback` - Reports if the copy was successful. Reasons that this could
// return false include that the sequence numbers do not match and when
// `maintain_clipboard` is true.
void DecodeImageFileAndCopyToClipboard(
    uint64_t clipboard_sequence,
    bool maintain_clipboard,
    scoped_refptr<base::RefCountedString> png_data,
    base::OnceCallback<void(bool)> callback);

}  // namespace clipboard_util

#endif  // CHROME_BROWSER_UI_ASH_CLIPBOARD_UTIL_H_
