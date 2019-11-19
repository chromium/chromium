// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/throw_uncaught_exception.h"

#include "base/android/jni_android.h"

#include "components/viz/service/service_jni_headers/ThrowUncaughtException_jni.h"

namespace viz {

void ThrowUncaughtException() {
  Java_ThrowUncaughtException_post(base::android::AttachCurrentThread());
}

}  // namespace viz
