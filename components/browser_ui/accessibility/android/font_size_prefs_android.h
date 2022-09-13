// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_ACCESSIBILITY_ANDROID_FONT_SIZE_PREFS_ANDROID_H_
#define COMPONENTS_BROWSER_UI_ACCESSIBILITY_ANDROID_FONT_SIZE_PREFS_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

class PrefChangeRegistrar;
class PrefService;

namespace browser_ui {
namespace prefs {

// Prefs related to this class.
extern const char kWebKitFontScaleFactor[];
extern const char kWebKitForceEnableZoom[];

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
                          jfloat font);
  float GetFontScaleFactor(JNIEnv* env,
                           const base::android::JavaRef<jobject>& obj);
  void SetForceEnableZoom(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj,
                          jboolean enabled);
  bool GetForceEnableZoom(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj);
  void Destroy(JNIEnv* env);

 private:
  // Callback for FontScaleFactor changes from pref change registrar.
  void OnFontScaleFactorChanged();
  // Callback for ForceEnableZoom changes from pref change registrar.
  void OnForceEnableZoomChanged();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  const raw_ptr<PrefService> pref_service_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace browser_ui

#endif  // COMPONENTS_BROWSER_UI_ACCESSIBILITY_ANDROID_FONT_SIZE_PREFS_ANDROID_H_
