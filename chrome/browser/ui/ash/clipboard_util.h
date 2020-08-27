// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CLIPBOARD_UTIL_H_
#define CHROME_BROWSER_UI_ASH_CLIPBOARD_UTIL_H_

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
void DecodeImageFileAndCopyToClipboard(
    scoped_refptr<base::RefCountedString> png_data);

}  // namespace clipboard_util

#endif  // CHROME_BROWSER_UI_ASH_CLIPBOARD_UTIL_H_
