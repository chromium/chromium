// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_DUPLICATE_DOWNLOAD_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_DUPLICATE_DOWNLOAD_INFOBAR_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/android/infobars/chrome_confirm_infobar.h"

namespace android {
class DuplicateDownloadInfoBarDelegate;
}

// A native-side implementation of an infobar to ask whether to continue
// downloading if target file already exists.
class DuplicateDownloadInfoBar : public ChromeConfirmInfoBar {
 public:
  static std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<android::DuplicateDownloadInfoBarDelegate> delegate);
  ~DuplicateDownloadInfoBar() override;

 private:
  explicit DuplicateDownloadInfoBar(
      std::unique_ptr<android::DuplicateDownloadInfoBarDelegate> delegate);

  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  android::DuplicateDownloadInfoBarDelegate* GetDelegate();

  DISALLOW_COPY_AND_ASSIGN(DuplicateDownloadInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_DUPLICATE_DOWNLOAD_INFOBAR_H_
