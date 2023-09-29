// Copyright 2019 The Chromium Authors
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
  // Note: |file| must support writing. |max_size| of 0 means unlimited size.
  explicit FileWStream(base::File file);
  FileWStream(base::File file, size_t max_size);
  ~FileWStream() override;

  // SkWStream impl.
  bool write(const void* buffer, size_t size) override;
  void flush() override;
  // NOTE: this returns |fake_bytes_written_| rather than |bytes_written_| in
  // order to hack around the fact that Skia requires serialization to succeed.
  // This hack is necessary to:
  // 1. Prevents crashes when a file cannot be enlarged due to running out of
  //    device disk space.
  // 2. Allows us to artificially cap the size of a SkPicture when writing it to
  //    avoid using consuming an unreasonable amount of disk space.
  size_t bytesWritten() const override;

  // Closes the file (occurs automatically on destruction).
  void Close();

  bool DidWriteFail() const { return has_write_failed_; }

  size_t ActualBytesWritten() const { return bytes_written_; }

 private:
  base::File file_;
  size_t max_size_;
  size_t bytes_written_;
  size_t fake_bytes_written_;
  bool has_write_failed_;

  FileWStream(const FileWStream&) = delete;
  FileWStream& operator=(const FileWStream&) = delete;
};

// An implementation of the SkWStream interface backed by base::File. Only
// implements the minimal interface and not the Seekable/Rewindable variants.
class FileRStream : public SkStream {
 public:
  // Note: |file| must support reading. It *cannot* be modified while streaming.
  explicit FileRStream(base::File file);
  ~FileRStream() override;

  size_t length() const { return length_; }

  // SkStream impl.
  size_t read(void* buffer, size_t size) override;
  bool isAtEnd() const override;
  bool hasLength() const override;
  size_t getLength() const override;

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
