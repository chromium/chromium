// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/reader_mode_infobar.h"

#include <memory>
#include <utility>

#include "chrome/android/chrome_jni_headers/ReaderModeInfoBar_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

class ReaderModeInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  ~ReaderModeInfoBarDelegate() override {}

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return InfoBarDelegate::InfoBarIdentifier::READER_MODE_INFOBAR_ANDROID;
  }

  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override {
    return delegate->GetIdentifier() == GetIdentifier();
  }
};

ReaderModeInfoBar::ReaderModeInfoBar(
    std::unique_ptr<ReaderModeInfoBarDelegate> delegate)
    : InfoBarAndroid(std::move(delegate)) {}

ReaderModeInfoBar::~ReaderModeInfoBar() {}

infobars::InfoBarDelegate* ReaderModeInfoBar::GetDelegate() {
  return delegate();
}

ScopedJavaLocalRef<jobject> ReaderModeInfoBar::CreateRenderInfoBar(
    JNIEnv* env) {
  return Java_ReaderModeInfoBar_create(env);
}

base::android::ScopedJavaLocalRef<jobject> ReaderModeInfoBar::GetTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(this);
  if (!web_contents)
    return nullptr;

  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  return tab_android ? tab_android->GetJavaObject() : nullptr;
}

void ReaderModeInfoBar::ProcessButton(int action) {}

void JNI_ReaderModeInfoBar_Create(JNIEnv* env,
                                  const JavaParamRef<jobject>& j_tab) {
  InfoBarService* service = InfoBarService::FromWebContents(
      TabAndroid::GetNativeTab(env, j_tab)->web_contents());

  service->AddInfoBar(std::make_unique<ReaderModeInfoBar>(
      std::make_unique<ReaderModeInfoBarDelegate>()));
}
