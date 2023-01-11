// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_SMS_ANDROID_SMS_INFOBAR_H_
#define COMPONENTS_BROWSER_UI_SMS_ANDROID_SMS_INFOBAR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/infobars/android/confirm_infobar.h"

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace sms {

class SmsInfoBarDelegate;

class SmsInfoBar : public infobars::ConfirmInfoBar {
 public:
  SmsInfoBar(content::WebContents* web_contents,
             std::unique_ptr<SmsInfoBarDelegate> delegate);

  SmsInfoBar(const SmsInfoBar&) = delete;
  SmsInfoBar& operator=(const SmsInfoBar&) = delete;

  ~SmsInfoBar() override;

  // Creates an SMS receiver infobar and delegate and adds it to
  // |infobar_manager|.
  static void Create(content::WebContents* web_contents,
                     infobars::InfoBarManager* manager,
                     const std::vector<url::Origin>& origin_list,
                     const std::string& one_time_code,
                     base::OnceClosure on_confirm,
                     base::OnceClosure on_cancel);

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace sms

#endif  // COMPONENTS_BROWSER_UI_SMS_ANDROID_SMS_INFOBAR_H_
