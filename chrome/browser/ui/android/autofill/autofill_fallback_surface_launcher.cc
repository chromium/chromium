// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_fallback_surface_launcher.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "components/plus_addresses/core/common/features.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutofillFallbackSurfaceLauncher_jni.h"

namespace autofill {

void ShowManagePlusAddressesPage(content::WebContents& web_contents) {
  if (web_contents.GetNativeView() &&
      web_contents.GetNativeView()->GetWindowAndroid()) {
    Java_AutofillFallbackSurfaceLauncher_openManagePlusAddresses(
        base::android::AttachCurrentThread(),
        web_contents.GetNativeView()->GetWindowAndroid()->GetJavaObject(),
        Profile::FromBrowserContext(web_contents.GetBrowserContext())
            ->GetJavaObject());
  }
}

void ShowGoogleWalletPassesPage(content::WebContents& web_contents) {
  if (web_contents.GetNativeView() &&
      web_contents.GetNativeView()->GetWindowAndroid()) {
    Java_AutofillFallbackSurfaceLauncher_openGoogleWalletPassesPage(
        base::android::AttachCurrentThread(),
        web_contents.GetNativeView()->GetWindowAndroid()->GetJavaObject());
  }
}

static std::string
JNI_AutofillFallbackSurfaceLauncher_GetPlusAddressManagementUrl(JNIEnv* env) {
  return plus_addresses::features::kPlusAddressManagementUrl.Get();
}

}  // namespace autofill

DEFINE_JNI(AutofillFallbackSurfaceLauncher)
