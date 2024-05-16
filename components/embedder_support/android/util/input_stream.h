// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_INPUT_STREAM_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_INPUT_STREAM_H_

#include <stdint.h>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"

namespace net {
class IOBuffer;
}

namespace embedder_support {

BASE_DECLARE_FEATURE(kEnableCustomInputStreamBufferSize);

// Abstract wrapper used to access the InputStream Java class.
// This class is safe to pass around between threads (the destructor,
// constructor and methods can be called on different threads) but calling
// methods concurrently might have undefined results.
class InputStream {
 public:
  // Returns the size to be used for the intermediate buffer to copy from Java's
  // InputStream into C++'s net::IOBuffer.
  static int GetIntermediateBufferSize();

  // |stream| should be an instance of the InputStream Java class.
  // |stream| can't be null.
  InputStream(const base::android::JavaRef<jobject>& stream);

  InputStream(const InputStream&) = delete;
  InputStream& operator=(const InputStream&) = delete;

  virtual ~InputStream();

  // Gets the underlying Java object. Guaranteed non-NULL.
  const base::android::JavaRef<jobject>& jobj() const { return jobject_; }

  // Sets |bytes_available| to the number of bytes that can be read (or skipped
  // over) from this input stream without blocking by the next caller of a
  // method for this input stream.
  // Returns true if completed successfully or false if an exception was
  // thrown.
  virtual bool BytesAvailable(int* bytes_available) const;

  // Skips over and discards |n| bytes of data from this input stream. Sets
  // |bytes_skipped| to the number of of bytes skipped.
  // Returns true if completed successfully or false if an exception was
  // thrown.
  virtual bool Skip(int64_t n, int64_t* bytes_skipped);

  // Reads at most |length| bytes into |dest|. Sets |bytes_read| to the total
  // number of bytes read into |dest| or 0 if there is no more data because the
  // end of the stream was reached.
  // |dest| must be at least |length| in size.
  // Returns true if completed successfully or false if an exception was
  // thrown.
  virtual bool Read(net::IOBuffer* dest, int length, int* bytes_read);

 protected:
  // Parameterless constructor exposed for testing.
  InputStream();

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobject_;
  base::android::ScopedJavaGlobalRef<jbyteArray> buffer_;
};

}  // namespace embedder_support

#endif  //  COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_INPUT_STREAM_H_
