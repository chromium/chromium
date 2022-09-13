// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_library_loader.h"

// This is called by the VM when the shared library is first loaded.
extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  return cronet::CronetOnLoad(vm, reserved);
}

extern "C" void JNI_OnUnLoad(JavaVM* vm, void* reserved) {
  cronet::CronetOnUnLoad(vm, reserved);
}

