// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/survey_infobar.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SurveyInfoBar_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;

class SurveyInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  SurveyInfoBarDelegate(JNIEnv* env,
                        const std::string& siteId,
                        bool showAsBottomSheet,
                        int displayLogoResourceId,
                        jobject surveyInfoBarDelegate)
      : site_id_(siteId),
        show_as_bottom_sheet_(showAsBottomSheet),
        display_logo_resource_id_(displayLogoResourceId),
        survey_info_bar_delegate_(
            ScopedJavaGlobalRef<jobject>(env, surveyInfoBarDelegate)) {}

  ~SurveyInfoBarDelegate() override {}

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return InfoBarDelegate::InfoBarIdentifier::SURVEY_INFOBAR_ANDROID;
  }

  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override {
    if (delegate->GetIdentifier() != GetIdentifier())
      return false;
    SurveyInfoBarDelegate* survey_delegate =
        static_cast<SurveyInfoBarDelegate*>(delegate);
    return site_id_ == survey_delegate->GetSiteId();
  }

  std::string GetSiteId() { return site_id_; }

  bool GetShowAsBottomSheet() { return show_as_bottom_sheet_; }

  int GetDisplayLogoResourceId() { return display_logo_resource_id_; }

  const ScopedJavaGlobalRef<jobject>& GetSurveyInfoBarDelegate() {
    return survey_info_bar_delegate_;
  }

 private:
  std::string site_id_;
  bool show_as_bottom_sheet_;
  int display_logo_resource_id_;
  ScopedJavaGlobalRef<jobject> survey_info_bar_delegate_;
};

SurveyInfoBar::SurveyInfoBar(std::unique_ptr<SurveyInfoBarDelegate> delegate)
    : InfoBarAndroid(std::move(delegate)) {}

SurveyInfoBar::~SurveyInfoBar() {}

base::android::ScopedJavaLocalRef<jobject> SurveyInfoBar::GetTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(this);
  DCHECK(web_contents);

  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  return tab_android->GetJavaObject();
}

infobars::InfoBarDelegate* SurveyInfoBar::GetDelegate() {
  return delegate();
}

void SurveyInfoBar::ProcessButton(int action) {}

ScopedJavaLocalRef<jobject> SurveyInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  SurveyInfoBarDelegate* survey_delegate =
      static_cast<SurveyInfoBarDelegate*>(delegate());
  return Java_SurveyInfoBar_create(
      env,
      base::android::ConvertUTF8ToJavaString(env, survey_delegate->GetSiteId()),
      survey_delegate->GetShowAsBottomSheet(),
      survey_delegate->GetDisplayLogoResourceId(),
      survey_delegate->GetSurveyInfoBarDelegate());
}

void JNI_SurveyInfoBar_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents,
    const JavaParamRef<jstring>& j_site_id,
    jboolean j_show_as_bottom_sheet,
    jint j_display_logo_resource_id,
    const JavaParamRef<jobject>& j_survey_info_bar_delegate) {
  InfoBarService* service = InfoBarService::FromWebContents(
      content::WebContents::FromJavaWebContents(j_web_contents));

  service->AddInfoBar(
      std::make_unique<SurveyInfoBar>(std::make_unique<SurveyInfoBarDelegate>(
          env, base::android::ConvertJavaStringToUTF8(env, j_site_id),
          j_show_as_bottom_sheet, j_display_logo_resource_id,
          j_survey_info_bar_delegate.obj())));
}
