// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_DOWNLOAD_PROGRESS_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_DOWNLOAD_PROGRESS_INFOBAR_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/infobars/android/infobar_android.h"
#include "components/infobars/core/infobar_delegate.h"

class DownloadProgressInfoBarDelegate;

class DownloadProgressInfoBar : public infobars::InfoBarAndroid {
 public:
  explicit DownloadProgressInfoBar(
      std::unique_ptr<DownloadProgressInfoBarDelegate> delegate);
  ~DownloadProgressInfoBar() override;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadProgressInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_DOWNLOAD_PROGRESS_INFOBAR_H_
