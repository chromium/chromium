// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/android/dumpstate_writer.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chromecast/base/jni_headers/DumpstateWriter_jni.h"

namespace chromecast {

// static
void DumpstateWriter::AddDumpValue(const std::string& name,
                                   const std::string& value) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DumpstateWriter_addDumpValue(
      env, base::android::ConvertUTF8ToJavaString(env, name),
      base::android::ConvertUTF8ToJavaString(env, value));
}

}  // namespace chromecast
