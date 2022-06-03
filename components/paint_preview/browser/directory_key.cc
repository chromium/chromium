// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/directory_key.h"

namespace paint_preview {

bool operator<(const DirectoryKey& a, const DirectoryKey& b) {
  return a.AsciiDirname() < b.AsciiDirname();
}

bool operator==(const DirectoryKey& a, const DirectoryKey& b) {
  return a.AsciiDirname() == b.AsciiDirname();
}

}  // namespace paint_preview
