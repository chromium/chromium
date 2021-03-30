// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/browser_context/browser_context_handle.h"

#include "base/android/jni_android.h"
#include "components/embedder_support/android/browser_context_jni_headers/BrowserContextHandle_jni.h"
#include "content/public/browser/browser_context.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;

namespace browser_context {

// static
content::BrowserContext* BrowserContextFromJavaHandle(
    const JavaRef<jobject>& jhandle) {
  if (!jhandle)
    return nullptr;

  return reinterpret_cast<content::BrowserContext*>(
      Java_BrowserContextHandle_getNativeBrowserContextPointer(
          AttachCurrentThread(), jhandle));
}

}  // namespace browser_context
