// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_ACCESSIBILITY_ANDROID_FONT_SIZE_PREFS_ANDROID_H_
#define COMPONENTS_BROWSER_UI_ACCESSIBILITY_ANDROID_FONT_SIZE_PREFS_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

class PrefService;

namespace browser_ui {
namespace prefs {

// Prefs related to this class.
extern const char kWebKitFontScaleFactor[];

}  // namespace prefs

/*
 * Native implementation of FontSizePrefs. This class is used to get and set
 * FontScaleFactor and ForceEnableZoom.
 */
class FontSizePrefsAndroid {
 public:
  FontSizePrefsAndroid(
      JNIEnv* env,
      jobject obj,
      const base::android::JavaParamRef<jobject>& jbrowser_context_handle);

  FontSizePrefsAndroid(const FontSizePrefsAndroid&) = delete;
  FontSizePrefsAndroid& operator=(const FontSizePrefsAndroid&) = delete;

  ~FontSizePrefsAndroid();

  void SetFontScaleFactor(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj,
                          jfloat font_scale_factor);
  void Destroy(JNIEnv* env);

 private:
  const raw_ptr<PrefService> pref_service_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace browser_ui

#endif  // COMPONENTS_BROWSER_UI_ACCESSIBILITY_ANDROID_FONT_SIZE_PREFS_ANDROID_H_
