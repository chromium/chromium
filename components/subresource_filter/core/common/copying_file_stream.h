// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_COPYING_FILE_STREAM_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_COPYING_FILE_STREAM_H_

#include "base/files/file.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace subresource_filter {

// Implements a CopyingInputStream that reads from a base::File. Can be used in
// combination with CopyingInputStreamAdaptor for reading from that file through
// the ZeroCopyInputStream interface.
class CopyingFileInputStream : public google::protobuf::io::CopyingInputStream {
 public:
  explicit CopyingFileInputStream(base::File file);

  CopyingFileInputStream(const CopyingFileInputStream&) = delete;
  CopyingFileInputStream& operator=(const CopyingFileInputStream&) = delete;

  ~CopyingFileInputStream() override;

  // google::protobuf::io::CopyingInputStream:
  int Read(void* buffer, int size) override;

 private:
  base::File file_;
};

// Implements a CopyingOutputStream that writes to a base::File. Can be used in
// combination with CopyingOutputStreamAdaptor for writing to that file through
// the ZeroCopyOutputStream interface.
class CopyingFileOutputStream
    : public google::protobuf::io::CopyingOutputStream {
 public:
  explicit CopyingFileOutputStream(base::File file);

  CopyingFileOutputStream(const CopyingFileOutputStream&) = delete;
  CopyingFileOutputStream& operator=(const CopyingFileOutputStream&) = delete;

  ~CopyingFileOutputStream() override;

  // google::protobuf::io::CopyingOutputStream:
  bool Write(const void* buffer, int size) override;

 private:
  base::File file_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_COPYING_FILE_STREAM_H_
