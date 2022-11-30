// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_SURVEY_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_SURVEY_INFOBAR_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/infobars/android/infobar_android.h"
#include "components/infobars/core/infobar_delegate.h"

class SurveyInfoBarDelegate;

// An infobar that prompts the user to take a survey.
class SurveyInfoBar : public infobars::InfoBarAndroid {
 public:
  explicit SurveyInfoBar(std::unique_ptr<SurveyInfoBarDelegate> delegate);

  SurveyInfoBar(const SurveyInfoBar&) = delete;
  SurveyInfoBar& operator=(const SurveyInfoBar&) = delete;

  ~SurveyInfoBar() override;

  base::android::ScopedJavaLocalRef<jobject> GetTab(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 protected:
  infobars::InfoBarDelegate* GetDelegate();

  // infobars::InfoBarAndroid overrides.
  void ProcessButton(int action) override;
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_SURVEY_INFOBAR_H_
