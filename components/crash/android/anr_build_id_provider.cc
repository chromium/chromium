// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/debug/elf_reader.h"
#include "components/crash/android/anr_collector_jni_headers/AnrCollector_jni.h"

extern char __executable_start;

base::android::ScopedJavaLocalRef<jstring>
JNI_AnrCollector_GetSharedLibraryBuildId(JNIEnv* env) {
  base::debug::ElfBuildIdBuffer build_id;
  base::debug::ReadElfBuildId(&__executable_start, false, build_id);
  return base::android::ConvertUTF8ToJavaString(env, std::string(build_id));
}
