// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/crash/android/anr_build_id_provider.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/debug/elf_reader.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/crash/android/anr_collector_jni_headers/AnrCollector_jni.h"

extern char __executable_start;

namespace crash_reporter {
std::string GetElfBuildId() {
  base::debug::ElfBuildIdBuffer build_id;
  base::debug::ReadElfBuildId(&__executable_start, false, build_id);
  return std::string(build_id);
}

}  // namespace crash_reporter

base::android::ScopedJavaLocalRef<jstring>
JNI_AnrCollector_GetSharedLibraryBuildId(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, crash_reporter::GetElfBuildId());
}
