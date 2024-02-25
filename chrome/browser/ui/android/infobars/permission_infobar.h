// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_PERMISSION_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_PERMISSION_INFOBAR_H_

#include "components/infobars/android/confirm_infobar.h"

class PermissionInfoBarDelegate;

class PermissionInfoBar : public infobars::ConfirmInfoBar {
 public:
  explicit PermissionInfoBar(
      std::unique_ptr<PermissionInfoBarDelegate> delegate);

  PermissionInfoBar(const PermissionInfoBar&) = delete;
  PermissionInfoBar& operator=(const PermissionInfoBar&) = delete;

  ~PermissionInfoBar() override;

 private:
  // infobars::InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  PermissionInfoBarDelegate* GetDelegate();
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_PERMISSION_INFOBAR_H_
