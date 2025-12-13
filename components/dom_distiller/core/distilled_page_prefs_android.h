// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_ANDROID_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"

namespace dom_distiller {
namespace android {

class DistilledPagePrefsAndroid {
 public:
  DistilledPagePrefsAndroid(JNIEnv* env,
                            const base::android::JavaRef<jobject>& obj,
                            DistilledPagePrefs* distilled_page_prefs_ptr);

  DistilledPagePrefsAndroid(const DistilledPagePrefsAndroid&) = delete;
  DistilledPagePrefsAndroid& operator=(const DistilledPagePrefsAndroid&) =
      delete;

  virtual ~DistilledPagePrefsAndroid();
  void SetFontFamily(JNIEnv* env,
                     jint font_family);
  jint GetFontFamily(JNIEnv* env);
  void SetUserPrefTheme(JNIEnv* env, jint theme);
  void SetDefaultTheme(JNIEnv* env, jint theme);
  jint GetTheme(JNIEnv* env);
  void SetUserPrefFontScaling(JNIEnv* env, jfloat scaling);
  void SetDefaultFontScaling(JNIEnv* env, jfloat scaling);
  jfloat GetFontScaling(JNIEnv* env);

  void AddObserver(JNIEnv* env,
                   jlong obs);
  void RemoveObserver(JNIEnv* env,
                      jlong obs);

 private:
  raw_ptr<DistilledPagePrefs> distilled_page_prefs_;
};

class DistilledPagePrefsObserverAndroid : public DistilledPagePrefs::Observer {
 public:
  DistilledPagePrefsObserverAndroid(JNIEnv* env,
                                    const base::android::JavaRef<jobject>& obj);
  ~DistilledPagePrefsObserverAndroid() override;

  // DistilledPagePrefs::Observer implementation.
  void OnChangeFontFamily(mojom::FontFamily new_font_family) override;
  void OnChangeTheme(mojom::Theme new_theme,
                     ThemeSettingsUpdateSource source) override;
  void OnChangeFontScaling(float scaling) override;

  virtual void DestroyObserverAndroid(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace android
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_ANDROID_H_
