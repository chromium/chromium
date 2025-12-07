// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/copying_file_stream.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"

namespace subresource_filter {

// CopyingFileInputStream ------------------------------------------------------

CopyingFileInputStream::~CopyingFileInputStream() = default;

CopyingFileInputStream::CopyingFileInputStream(base::File file)
    : file_(std::move(file)) {}

int CopyingFileInputStream::Read(void* buffer, int size) {
  std::optional<size_t> result =
      file_.ReadAtCurrentPosNoBestEffort(UNSAFE_TODO(base::span(
          static_cast<uint8_t*>(buffer), base::checked_cast<size_t>(size))));

  return result.has_value() ? base::checked_cast<int>(result.value()) : -1;
}

// CopyingFileOutputStream -----------------------------------------------------
CopyingFileOutputStream::~CopyingFileOutputStream() = default;

CopyingFileOutputStream::CopyingFileOutputStream(base::File file)
    : file_(std::move(file)) {}

bool CopyingFileOutputStream::Write(const void* buffer, int size) {
  return UNSAFE_TODO(
      file_.WriteAtCurrentPos(static_cast<const char*>(buffer), size) == size);
}

}  // namespace subresource_filter
