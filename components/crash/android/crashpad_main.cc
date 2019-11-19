// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/crash/android/java_handler_jni_headers/CrashpadMain_jni.h"
#include "third_party/crashpad/crashpad/client/client_argv_handling.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"

namespace crashpad {

static void JNI_CrashpadMain_CrashpadMain(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& j_argv) {
  std::vector<std::string> argv_strings;
  base::android::AppendJavaStringArrayToStringVector(env, j_argv,
                                                     &argv_strings);

  std::vector<const char*> argv;
  StringVectorToCStringVector(argv_strings, &argv);
  CrashpadHandlerMain(argv.size() - 1, const_cast<char**>(argv.data()));
}

}  // namespace crashpad
