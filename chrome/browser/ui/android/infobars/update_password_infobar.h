// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_UPDATE_PASSWORD_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_UPDATE_PASSWORD_INFOBAR_H_

#include "base/android/scoped_java_ref.h"
#include "components/infobars/android/confirm_infobar.h"
#include "components/signin/public/identity_manager/account_info.h"

class UpdatePasswordInfoBarDelegate;

// The infobar to be used with UpdatePasswordInfoBarDelegate.
class UpdatePasswordInfoBar : public infobars::ConfirmInfoBar {
 public:
  UpdatePasswordInfoBar(std::unique_ptr<UpdatePasswordInfoBarDelegate> delegate,
                        const AccountInfo& account_info);

  UpdatePasswordInfoBar(const UpdatePasswordInfoBar&) = delete;
  UpdatePasswordInfoBar& operator=(const UpdatePasswordInfoBar&) = delete;

  ~UpdatePasswordInfoBar() override;

  int GetIdOfSelectedUsername() const;

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;
  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj) override;

  base::android::ScopedJavaGlobalRef<jobject> java_infobar_;

  AccountInfo account_info_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_UPDATE_PASSWORD_INFOBAR_H_
