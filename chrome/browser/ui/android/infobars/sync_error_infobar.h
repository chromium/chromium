// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_SYNC_ERROR_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_SYNC_ERROR_INFOBAR_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "chrome/browser/sync/sync_error_infobar_delegate_android.h"
#include "chrome/browser/ui/android/infobars/chrome_confirm_infobar.h"

class SyncErrorInfoBar : public ChromeConfirmInfoBar {
 public:
  explicit SyncErrorInfoBar(
      std::unique_ptr<SyncErrorInfoBarDelegateAndroid> delegate);
  ~SyncErrorInfoBar() override;

 protected:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  DISALLOW_COPY_AND_ASSIGN(SyncErrorInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_SYNC_ERROR_INFOBAR_H_
