// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_SYNC_ERROR_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_SYNC_ERROR_INFOBAR_H_

#include "base/android/jni_android.h"
#include "chrome/browser/sync/sync_error_infobar_delegate_android.h"
#include "components/infobars/android/confirm_infobar.h"

class SyncErrorInfoBar : public infobars::ConfirmInfoBar {
 public:
  explicit SyncErrorInfoBar(
      std::unique_ptr<SyncErrorInfoBarDelegateAndroid> delegate);

  SyncErrorInfoBar(const SyncErrorInfoBar&) = delete;
  SyncErrorInfoBar& operator=(const SyncErrorInfoBar&) = delete;

  ~SyncErrorInfoBar() override;

 protected:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_SYNC_ERROR_INFOBAR_H_
