// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/io_buffer_with_byte_buffer.h"

#include "base/check_op.h"

namespace cronet {

IOBufferWithByteBuffer::IOBufferWithByteBuffer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jbyte_buffer,
    void* byte_buffer_data,
    jint position,
    jint limit)
    : net::WrappedIOBuffer(static_cast<char*>(byte_buffer_data) + position),
      byte_buffer_(env, jbyte_buffer),
      initial_position_(position),
      initial_limit_(limit) {
  DCHECK(byte_buffer_data);
  DCHECK_EQ(env->GetDirectBufferAddress(jbyte_buffer), byte_buffer_data);
}

IOBufferWithByteBuffer::~IOBufferWithByteBuffer() {}

ByteBufferWithIOBuffer::ByteBufferWithIOBuffer(
    JNIEnv* env,
    scoped_refptr<net::IOBuffer> io_buffer,
    int io_buffer_len)
    : io_buffer_(std::move(io_buffer)), io_buffer_len_(io_buffer_len) {
  // An intermediate ScopedJavaLocalRef is needed here to release the local
  // reference created by env->NewDirectByteBuffer().
  base::android::ScopedJavaLocalRef<jobject> java_buffer(
      env, env->NewDirectByteBuffer(io_buffer_->data(), io_buffer_len_));
  byte_buffer_.Reset(env, java_buffer.obj());
}

ByteBufferWithIOBuffer::~ByteBufferWithIOBuffer() {}

}  // namespace cronet
