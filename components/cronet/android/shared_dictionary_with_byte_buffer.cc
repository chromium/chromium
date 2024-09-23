// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/shared_dictionary_with_byte_buffer.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "net/base/net_errors.h"

namespace cronet {
namespace {

net::SHA256HashValue ByteArrayToSHA256(
    JNIEnv* env,
    const base::android::JavaRef<jbyteArray>& jdictionary_sha256_hash) {
  const auto dictionary_sha256_hash_size =
      base::android::SafeGetArrayLength(env, jdictionary_sha256_hash);
  CHECK_EQ(dictionary_sha256_hash_size, sizeof(net::SHA256HashValue::data));
  net::SHA256HashValue dictionary_sha256_hash;
  void* const bytes = env->GetPrimitiveArrayCritical(
      jdictionary_sha256_hash.obj(), /*isCopy=*/nullptr);
  CHECK(bytes);
  memcpy(&dictionary_sha256_hash.data, bytes, dictionary_sha256_hash_size);
  env->ReleasePrimitiveArrayCritical(jdictionary_sha256_hash.obj(), bytes,
                                     JNI_ABORT);
  return dictionary_sha256_hash;
}

}  // namespace

// static
scoped_refptr<SharedDictionaryWithByteBuffer>
SharedDictionaryWithByteBuffer::MaybeCreate(
    JNIEnv* env,
    const base::android::JavaRef<jbyteArray>& dictionary_sha256_hash,
    const base::android::JavaRef<jobject>& dictionary_content_byte_buffer,
    jint dictionary_content_position,
    jint dictionary_content_limit,
    const base::android::JavaRef<jstring>& dictionary_id) {
  if (dictionary_sha256_hash.obj() == nullptr ||
      dictionary_content_byte_buffer.obj() == nullptr) {
    return nullptr;
  }
  return new SharedDictionaryWithByteBuffer(
      env, dictionary_sha256_hash, dictionary_content_byte_buffer,
      dictionary_content_position, dictionary_content_limit, dictionary_id);
}

SharedDictionaryWithByteBuffer::SharedDictionaryWithByteBuffer(
    JNIEnv* env,
    const base::android::JavaRef<jbyteArray>& dictionary_sha256_hash,
    const base::android::JavaRef<jobject>& dictionary_content_byte_buffer,
    jint dictionary_content_position,
    jint dictionary_content_limit,
    const base::android::JavaRef<jstring>& dictionary_id)
    : hash_(ByteArrayToSHA256(env, dictionary_sha256_hash)),
      content_(base::MakeRefCounted<IOBufferWithByteBuffer>(
          env,
          dictionary_content_byte_buffer,
          dictionary_content_position,
          dictionary_content_limit)),
      id_(base::android::ConvertJavaStringToUTF8(env, dictionary_id)) {}

SharedDictionaryWithByteBuffer::~SharedDictionaryWithByteBuffer() = default;

int SharedDictionaryWithByteBuffer::ReadAll(
    base::OnceCallback<void(int)> callback) {
  // Dictionary is always in memory.
  return net::OK;
}
scoped_refptr<net::IOBuffer> SharedDictionaryWithByteBuffer::data() const {
  return content_;
}
const net::SHA256HashValue& SharedDictionaryWithByteBuffer::hash() const {
  return hash_;
}
const std::string& SharedDictionaryWithByteBuffer::id() const {
  return id_;
}

size_t SharedDictionaryWithByteBuffer::size() const {
  return content_->size();
}

}  // namespace cronet
