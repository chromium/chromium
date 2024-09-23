// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_SHARED_DICTIONARY_WITH_BYTE_BUFFER_H_
#define COMPONENTS_CRONET_ANDROID_SHARED_DICTIONARY_WITH_BYTE_BUFFER_H_

#include <jni.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "components/cronet/android/io_buffer_with_byte_buffer.h"
#include "net/base/hash_value.h"
#include "net/shared_dictionary/shared_dictionary.h"

namespace cronet {

class SharedDictionaryWithByteBuffer final : public net::SharedDictionary {
 public:
  static scoped_refptr<SharedDictionaryWithByteBuffer> MaybeCreate(
      JNIEnv* env,
      const base::android::JavaRef<jbyteArray>& dictionary_sha256_hash,
      const base::android::JavaRef<jobject>& dictionary_content_byte_buffer,
      jint dictionary_content_position,
      jint dictionary_content_limit,
      const base::android::JavaRef<jstring>& dictionary_id);

  int ReadAll(base::OnceCallback<void(int)> callback) override;
  scoped_refptr<net::IOBuffer> data() const override;
  const net::SHA256HashValue& hash() const override;
  const std::string& id() const override;
  size_t size() const override;

 private:
  SharedDictionaryWithByteBuffer(
      JNIEnv* env,
      const base::android::JavaRef<jbyteArray>& dictionary_sha256_hash,
      const base::android::JavaRef<jobject>& dictionary_content_byte_buffer,
      jint dictionary_content_position,
      jint dictionary_content_limit,
      const base::android::JavaRef<jstring>& dictionary_id);
  ~SharedDictionaryWithByteBuffer() override;

  net::SHA256HashValue hash_;
  scoped_refptr<IOBufferWithByteBuffer> content_;
  std::string id_;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_SHARED_DICTIONARY_WITH_BYTE_BUFFER_H_
