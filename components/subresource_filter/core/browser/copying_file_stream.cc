// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/copying_file_stream.h"

namespace subresource_filter {

// CopyingFileInputStream ------------------------------------------------------

CopyingFileInputStream::~CopyingFileInputStream() = default;

CopyingFileInputStream::CopyingFileInputStream(base::File file)
    : file_(std::move(file)) {}

int CopyingFileInputStream::Read(void* buffer, int size) {
  return file_.ReadAtCurrentPosNoBestEffort(static_cast<char*>(buffer), size);
}

// CopyingFileOutputStream -----------------------------------------------------
CopyingFileOutputStream::~CopyingFileOutputStream() = default;

CopyingFileOutputStream::CopyingFileOutputStream(base::File file)
    : file_(std::move(file)) {}

bool CopyingFileOutputStream::Write(const void* buffer, int size) {
  return file_.WriteAtCurrentPos(static_cast<const char*>(buffer), size) ==
         size;
}

}  // namespace subresource_filter
