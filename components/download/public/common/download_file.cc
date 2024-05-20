// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_file.h"

namespace download {

DownloadFile::~DownloadFile() = default;

bool DownloadFile::IsMemoryFile() {
  return false;
}

}  // namespace download
