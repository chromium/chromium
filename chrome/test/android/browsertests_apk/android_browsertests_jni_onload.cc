// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "chrome/app/android/chrome_jni_onload.h"
#include "content/public/test/nested_message_pump_android.h"

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  if (!android::OnJNIOnLoadInit())
    return -1;

  // chrome's OnJNIOnLoadInit will set the product version number, but tests do
  // not have a version so they expect the version number to be "" in java. We
  // reset the value back to empty string here, and hope that no code in between
  // depended on it.
  base::android::SetVersionNumber("");

  // This needs to be done before base::TestSuite::Initialize() is called,
  // as it also tries to set MessagePumpForUIFactory.
  base::MessagePump::OverrideMessagePumpForUIFactory(
      []() -> std::unique_ptr<base::MessagePump> {
        return std::make_unique<content::NestedMessagePumpAndroid>();
      });

  // Other browser test implementations of JNI_OnLoad set the
  // ContentMainDelegate here, but //chrome's OnJNIOnLoadInit will already set
  // the delegate, so we do not repeat that here.

  return JNI_VERSION_1_4;
}
