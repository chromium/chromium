// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_SMS_SMS_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_SMS_SMS_INFOBAR_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

class SmsInfoBarDelegate;

class SmsInfoBar : public ConfirmInfoBar {
 public:
  SmsInfoBar(content::WebContents* web_contents,
             std::unique_ptr<SmsInfoBarDelegate> delegate);
  ~SmsInfoBar() override;

  // Creates an SMS receiver infobar and delegate and adds it to
  // |infobar_service|.
  static void Create(content::WebContents* web_contents,
                     const url::Origin& origin,
                     const std::string& one_time_code,
                     base::OnceCallback<void()> on_confirm,
                     base::OnceCallback<void()> on_cancel);

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(SmsInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_SMS_SMS_INFOBAR_H_
