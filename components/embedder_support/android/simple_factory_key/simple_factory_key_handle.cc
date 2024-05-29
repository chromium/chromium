// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/simple_factory_key/simple_factory_key_handle.h"

#include "base/android/jni_android.h"
#include "components/keyed_service/core/simple_factory_key.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/simple_factory_key_jni_headers/SimpleFactoryKeyHandle_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;

namespace simple_factory_key {

// static
SimpleFactoryKey* SimpleFactoryKeyFromJavaHandle(
    const JavaRef<jobject>& jhandle) {
  if (!jhandle)
    return nullptr;

  return reinterpret_cast<SimpleFactoryKey*>(
      Java_SimpleFactoryKeyHandle_getNativeSimpleFactoryKeyPointer(
          AttachCurrentThread(), jhandle));
}

}  // namespace simple_factory_key
