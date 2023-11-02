// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_SIMPLE_FACTORY_KEY_SIMPLE_FACTORY_KEY_HANDLE_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_SIMPLE_FACTORY_KEY_SIMPLE_FACTORY_KEY_HANDLE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

class SimpleFactoryKey;

namespace simple_factory_key {

// Returns a pointer to the native SimpleFactoryKey wrapped by the given Java
// SimpleFactoryKeyHandle reference.
SimpleFactoryKey* SimpleFactoryKeyFromJavaHandle(
    const base::android::JavaRef<jobject>& jhandle);

}  // namespace simple_factory_key

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_SIMPLE_FACTORY_KEY_SIMPLE_FACTORY_KEY_HANDLE_H_
