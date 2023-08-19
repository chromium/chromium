// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_HATS_SURVEY_CONFIG_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_HATS_SURVEY_CONFIG_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "chrome/browser/ui/hats/survey_config.h"

using base::android::JavaParamRef;

namespace hats {

class SurveyConfigHolder {
 public:
  SurveyConfigHolder(JNIEnv* env, const JavaParamRef<jobject>& obj);
  ~SurveyConfigHolder();

  void Destroy(JNIEnv* env);

 private:
  // Initialize Java holders
  void InitJavaHolder();

  SurveyConfigs survey_configs_by_triggers_;
  base::android::ScopedJavaGlobalRef<jobject> jobj_;
};

}  // namespace hats

#endif  // CHROME_BROWSER_UI_ANDROID_HATS_SURVEY_CONFIG_ANDROID_H_
