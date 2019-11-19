// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_FILE_STREAM_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_FILE_STREAM_H_

#include "base/files/file.h"
#include "third_party/skia/include/core/SkStream.h"

namespace paint_preview {

// An implementation of the SkWStream interface backed by base::File.
class FileWStream : public SkWStream {
 public:
  // Note: |file| must support writing.
  FileWStream(base::File file);
  ~FileWStream() override;

  bool write(const void* buffer, size_t size) override;
  void flush() override;
  size_t bytesWritten() const override;

  // Closes the file (occurs automatically on destruction).
  void Close();

 private:
  base::File file_;
  size_t bytes_written_;

  FileWStream(const FileWStream&) = delete;
  FileWStream& operator=(const FileWStream&) = delete;
};

// An implementation of the SkWStream interface backed by base::File. Only
// implements the minimal interface and not the Seekable/Rewindable variants.
class FileRStream : public SkStream {
 public:
  // Note: |file| must support reading. It *cannot* be modified while streaming.
  FileRStream(base::File file);
  ~FileRStream() override;

  size_t read(void* buffer, size_t size) override;
  bool isAtEnd() const override;

 private:
  base::File file_;

  // Length is fixed at the start of streaming as the file should not be
  // modified. This value must be cached as base::File::GetLength() is not a
  // const operation.
  const size_t length_;
  size_t bytes_read_;

  FileRStream(const FileRStream&) = delete;
  FileRStream& operator=(const FileRStream&) = delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_FILE_STREAM_H_
