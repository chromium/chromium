// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/base_jni_onload.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/library_loader/library_loader_hooks.h"

// This is called by the VM when the shared library is first loaded.
// Checks the available version of JNI. Also, caches Java reflection artifacts.
extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  if (!base::android::OnJNIOnLoadInit()) {
    return -1;
  }
  return JNI_VERSION_1_6;
}

extern "C" void JNI_OnUnLoad(JavaVM* vm, void* reserved) {
  base::android::LibraryLoaderExitHook();
}
