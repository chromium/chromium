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
                            jobject obj,
                            DistilledPagePrefs* distilled_page_prefs_ptr);

  DistilledPagePrefsAndroid(const DistilledPagePrefsAndroid&) = delete;
  DistilledPagePrefsAndroid& operator=(const DistilledPagePrefsAndroid&) =
      delete;

  virtual ~DistilledPagePrefsAndroid();
  void SetFontFamily(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     jint font_family);
  jint GetFontFamily(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  void SetTheme(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jint theme);
  jint GetTheme(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetFontScaling(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jfloat scaling);
  jfloat GetFontScaling(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);

  void AddObserver(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jlong obs);
  void RemoveObserver(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jlong obs);

 private:
  raw_ptr<DistilledPagePrefs> distilled_page_prefs_;
};

class DistilledPagePrefsObserverAndroid : public DistilledPagePrefs::Observer {
 public:
  DistilledPagePrefsObserverAndroid(JNIEnv* env, jobject obj);
  virtual ~DistilledPagePrefsObserverAndroid();

  // DistilledPagePrefs::Observer implementation.
  void OnChangeFontFamily(mojom::FontFamily new_font_family) override;
  void OnChangeTheme(mojom::Theme new_theme) override;
  void OnChangeFontScaling(float scaling) override;

  virtual void DestroyObserverAndroid(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace android
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_ANDROID_H_
