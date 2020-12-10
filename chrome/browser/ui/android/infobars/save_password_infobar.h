// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_SAVE_PASSWORD_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_SAVE_PASSWORD_INFOBAR_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/infobars/chrome_confirm_infobar.h"
#include "components/signin/public/identity_manager/account_info.h"

class SavePasswordInfoBarDelegate;

// The infobar to be used with SavePasswordInfoBarDelegate.
class SavePasswordInfoBar : public ChromeConfirmInfoBar {
 public:
  explicit SavePasswordInfoBar(
      std::unique_ptr<SavePasswordInfoBarDelegate> delegate,
      base::Optional<AccountInfo> account_info);
  ~SavePasswordInfoBar() override;

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;
  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj) override;

  base::android::ScopedJavaGlobalRef<jobject> java_infobar_;

  base::Optional<AccountInfo> account_info_;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_SAVE_PASSWORD_INFOBAR_H_
