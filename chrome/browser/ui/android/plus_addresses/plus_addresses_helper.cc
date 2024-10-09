// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/plus_addresses_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/plus_addresses/features.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PlusAddressesHelper_jni.h"

namespace plus_addresses {

void ShowManagePlusAddressesPage(content::WebContents& web_contents) {
  if (web_contents.GetNativeView() != nullptr &&
      web_contents.GetNativeView()->GetWindowAndroid() != nullptr) {
    Java_PlusAddressesHelper_openManagePlusAddresses(
        base::android::AttachCurrentThread(),
        web_contents.GetNativeView()->GetWindowAndroid()->GetJavaObject(),
        plus_addresses::features::kPlusAddressManagementUrl.Get());
  }
}

static jni_zero::ScopedJavaLocalRef<jstring>
JNI_PlusAddressesHelper_GetPlusAddressManagementUrl(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, plus_addresses::features::kPlusAddressManagementUrl.Get());
}

}  // namespace plus_addresses
