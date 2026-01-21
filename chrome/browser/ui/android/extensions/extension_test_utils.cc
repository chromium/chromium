// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/test_support_jni_headers/ExtensionTestUtils_jni.h"

using base::FilePath;

namespace extensions {

static void JNI_ExtensionTestUtils_LoadUnpackedExtensionAsync(
    JNIEnv* env,
    Profile* profile,
    std::string& root_dir,
    const base::android::JavaRef<jobject>& callback) {
  ChromeTestExtensionLoader loader(profile);
  loader.LoadUnpackedExtensionAsync(
      FilePath::FromUTF8Unsafe(root_dir),
      base::BindOnce(
          [](const base::android::JavaRef<jobject>& callback,
             const Extension* extension) {
            base::android::RunStringCallbackAndroid(callback, extension->id());
          },
          base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

}  // namespace extensions

DEFINE_JNI(ExtensionTestUtils)
