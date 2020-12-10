// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_GROUPED_PERMISSION_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_GROUPED_PERMISSION_INFOBAR_H_

#include "chrome/browser/ui/android/infobars/chrome_confirm_infobar.h"

class GroupedPermissionInfoBarDelegate;

// TODO(andypaicu): rename this to PermissionInfoBar, grouped permissions are
// not a thing anymore.
class GroupedPermissionInfoBar : public ChromeConfirmInfoBar {
 public:
  explicit GroupedPermissionInfoBar(
      std::unique_ptr<GroupedPermissionInfoBarDelegate> delegate);
  ~GroupedPermissionInfoBar() override;

 private:
  // infobars::InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  GroupedPermissionInfoBarDelegate* GetDelegate();

  DISALLOW_COPY_AND_ASSIGN(GroupedPermissionInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_GROUPED_PERMISSION_INFOBAR_H_
