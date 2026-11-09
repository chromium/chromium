// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_DISTILLER_CORE_DISTILLED_PAGE_PREFS_ANDROID_H_
#define COMPONENTS_WEB_DISTILLER_CORE_DISTILLED_PAGE_PREFS_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/web_distiller/core/distilled_page_prefs.h"

namespace web_distiller {
namespace android {

class DistilledPagePrefsObserverAndroid;

class DistilledPagePrefsAndroid {
 public:
  DistilledPagePrefsAndroid(JNIEnv* env,
                            const base::android::JavaRef<jobject>& obj,
                            DistilledPagePrefs* distilled_page_prefs_ptr);

  DistilledPagePrefsAndroid(const DistilledPagePrefsAndroid&) = delete;
  DistilledPagePrefsAndroid& operator=(const DistilledPagePrefsAndroid&) =
      delete;

  virtual ~DistilledPagePrefsAndroid();
  void SetFontFamily(JNIEnv* env, int32_t font_family);
  int32_t GetFontFamily(JNIEnv* env);
  void SetUserPrefTheme(JNIEnv* env, int32_t theme);
  void SetDefaultTheme(JNIEnv* env, int32_t theme);
  int32_t GetTheme(JNIEnv* env);
  void SetUserPrefFontScaling(JNIEnv* env, float scaling);
  void SetDefaultFontScaling(JNIEnv* env, float scaling);
  float GetFontScaling(JNIEnv* env);
  void SetLinksEnabled(JNIEnv* env, bool enabled);
  bool GetLinksEnabled(JNIEnv* env);

  void AddObserver(JNIEnv* env, DistilledPagePrefsObserverAndroid* obs);
  void RemoveObserver(JNIEnv* env, DistilledPagePrefsObserverAndroid* obs);

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
  void OnChangeLinksEnabled(bool enabled) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace android
}  // namespace web_distiller

#endif  // COMPONENTS_WEB_DISTILLER_CORE_DISTILLED_PAGE_PREFS_ANDROID_H_
