// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_INPUT_STREAM_READER_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_INPUT_STREAM_READER_H_

#include "base/memory/raw_ptr.h"

namespace net {
class HttpByteRange;
class IOBuffer;
}  // namespace net

namespace embedder_support {
class InputStream;

// Class responsible for reading the InputStream.
class InputStreamReader {
 public:
  // The constructor is called on the IO thread, not on the worker thread.
  explicit InputStreamReader(InputStream* stream);

  InputStreamReader(const InputStreamReader&) = delete;
  InputStreamReader& operator=(const InputStreamReader&) = delete;

  virtual ~InputStreamReader();

  // Perform a seek operation on the InputStream associated with this job.
  // On successful completion the InputStream would have skipped reading the
  // number of bytes equal to the lower range of |byte_range|.
  // This method should be called on the |g_worker_thread| thread.
  //
  // |byte_range| is the range of bytes to be read from |stream|
  //
  // A negative return value will indicate an error code, a positive value
  // will indicate the expected size of the content.
  virtual int Seek(const net::HttpByteRange& byte_range);

  // Read data from |stream_|.  This method should be called on the
  // |g_worker_thread| thread.
  //
  // A negative return value will indicate an error code, a positive value
  // will indicate the expected size of the content.
  virtual int ReadRawData(net::IOBuffer* buffer, int buffer_size);

 private:
  // Verify the requested range against the stream size.
  // net::OK is returned on success, the error code otherwise.
  int VerifyRequestedRange(net::HttpByteRange* byte_range, int* content_size);

  // Skip to the first byte of the requested read range.
  // net::OK is returned on success, the error code otherwise.
  int SkipToRequestedRange(const net::HttpByteRange& byte_range);

  raw_ptr<InputStream> stream_;
};

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_INPUT_STREAM_READER_H_
