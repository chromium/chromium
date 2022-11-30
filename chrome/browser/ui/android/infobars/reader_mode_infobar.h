// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_READER_MODE_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_READER_MODE_INFOBAR_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/infobars/android/infobar_android.h"
#include "components/infobars/core/infobar_delegate.h"

class ReaderModeInfoBarDelegate;

class ReaderModeInfoBar : public infobars::InfoBarAndroid {
 public:
  explicit ReaderModeInfoBar(
      std::unique_ptr<ReaderModeInfoBarDelegate> delegate);

  ReaderModeInfoBar(const ReaderModeInfoBar&) = delete;
  ReaderModeInfoBar& operator=(const ReaderModeInfoBar&) = delete;

  ~ReaderModeInfoBar() override;

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

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_READER_MODE_INFOBAR_H_
