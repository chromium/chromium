// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/android/pure_java_exception_handler.h"

#include "components/crash/android/jni_headers/PureJavaExceptionHandler_jni.h"

void UninstallPureJavaExceptionHandler() {
  Java_PureJavaExceptionHandler_uninstallHandler(
      base::android::AttachCurrentThread());
}
