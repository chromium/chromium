// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/accessibility/android/font_size_prefs_android.h"

#include "components/prefs/android/pref_service_android.h"
#include "components/prefs/pref_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/accessibility/android/accessibility_jni_headers/FontSizePrefs_jni.h"

namespace browser_ui {

namespace prefs {
const char kWebKitFontScaleFactor[] = "webkit.webprefs.font_scale_factor";
}  // namespace prefs

static void JNI_FontSizePrefs_SetFontScaleFactor(JNIEnv* env,
                                                 PrefService* pref_service,
                                                 float font_scale_factor) {
  pref_service->SetDouble(prefs::kWebKitFontScaleFactor,
                          static_cast<double>(font_scale_factor));
}

}  // namespace browser_ui

DEFINE_JNI(FontSizePrefs)
