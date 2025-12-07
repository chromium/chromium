// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_IO_BUFFER_WITH_BYTE_BUFFER_H_
#define COMPONENTS_CRONET_ANDROID_IO_BUFFER_WITH_BYTE_BUFFER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "net/base/io_buffer.h"

namespace cronet {

// net::WrappedIOBuffer subclass for a buffer owned by a Java ByteBuffer. Keeps
// the ByteBuffer alive until destroyed. Uses WrappedIOBuffer because data() is
// owned by the embedder.
class IOBufferWithByteBuffer : public net::WrappedIOBuffer {
 public:
  // Creates a buffer wrapping the Java ByteBuffer |jbyte_buffer|.
  // |position| is the index of the first byte of data inside of the buffer.
  // |limit| is the the index of the first element that should not be read or
  // written, preserved to verify that buffer is not changed externally during
  // networking operations.
  IOBufferWithByteBuffer(JNIEnv* env,
                         const base::android::JavaRef<jobject>& jbyte_buffer,
                         jint position,
                         jint limit);

  IOBufferWithByteBuffer(const IOBufferWithByteBuffer&) = delete;
  IOBufferWithByteBuffer& operator=(const IOBufferWithByteBuffer&) = delete;

  jint initial_position() const { return initial_position_; }
  jint initial_limit() const { return initial_limit_; }

  const base::android::JavaRef<jobject>& byte_buffer() const {
    return byte_buffer_;
  }

 private:
  ~IOBufferWithByteBuffer() override;

  base::android::ScopedJavaGlobalRef<jobject> byte_buffer_;

  const jint initial_position_;
  const jint initial_limit_;
};

// Represents a Java direct ByteBuffer backed by a net::IOBuffer. Keeps both the
// net::IOBuffer and the Java ByteBuffer object alive until destroyed.
class ByteBufferWithIOBuffer {
 public:
  ByteBufferWithIOBuffer(JNIEnv* env,
                         scoped_refptr<net::IOBuffer> io_buffer,
                         int io_buffer_len);

  ByteBufferWithIOBuffer(const ByteBufferWithIOBuffer&) = delete;
  ByteBufferWithIOBuffer& operator=(const ByteBufferWithIOBuffer&) = delete;

  ~ByteBufferWithIOBuffer();
  const net::IOBuffer* io_buffer() const { return io_buffer_.get(); }
  int io_buffer_len() const { return io_buffer_len_; }

  const base::android::JavaRef<jobject>& byte_buffer() const {
    return byte_buffer_;
  }

 private:
  scoped_refptr<net::IOBuffer> io_buffer_;
  int io_buffer_len_;

  base::android::ScopedJavaGlobalRef<jobject> byte_buffer_;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_IO_BUFFER_WITH_BYTE_BUFFER_H_
