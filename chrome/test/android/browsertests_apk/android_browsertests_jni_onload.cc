// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "chrome/app/android/chrome_jni_onload.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "content/public/test/nested_message_pump_android.h"

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  // We avoid calling chrome's android::OnJNIOnLoadInit() so we can inject
  // our own ChromeMainDelegate, and avoid it setting the wrong version number.
  // We jump directly to the content method instead, which chrome's would call
  // also.
  if (!content::android::OnJNIOnLoadInit())
    return -1;

  // Tests do not have a version so they expect the version number to be "" in
  // java.
  base::android::SetVersionNumber("");

  // This needs to be done before base::TestSuite::Initialize() is called,
  // as it also tries to set MessagePumpForUIFactory.
  base::MessagePump::OverrideMessagePumpForUIFactory(
      []() -> std::unique_ptr<base::MessagePump> {
        return std::make_unique<content::NestedMessagePumpAndroid>();
      });

  content::SetContentMainDelegate(new ChromeTestChromeMainDelegate());

  return JNI_VERSION_1_4;
}
