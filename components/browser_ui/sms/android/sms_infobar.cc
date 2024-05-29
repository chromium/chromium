// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/sms/android/sms_infobar.h"

#include "base/android/jni_string.h"
#include "components/browser_ui/sms/android/sms_infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/sms/android/jni_headers/WebOTPServiceInfoBar_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;
using infobars::InfoBarDelegate;

namespace sms {

// static
void SmsInfoBar::Create(content::WebContents* web_contents,
                        infobars::InfoBarManager* manager,
                        const std::vector<url::Origin>& origin_list,
                        const std::string& one_time_code,
                        base::OnceClosure on_confirm,
                        base::OnceClosure on_cancel) {
  auto delegate = std::make_unique<SmsInfoBarDelegate>(
      origin_list, one_time_code, std::move(on_confirm), std::move(on_cancel));
  auto infobar =
      std::make_unique<SmsInfoBar>(web_contents, std::move(delegate));
  manager->AddInfoBar(std::move(infobar));
}

SmsInfoBar::SmsInfoBar(content::WebContents* web_contents,
                       std::unique_ptr<SmsInfoBarDelegate> delegate)
    : infobars::ConfirmInfoBar(std::move(delegate)),
      web_contents_(web_contents) {}

SmsInfoBar::~SmsInfoBar() = default;

ScopedJavaLocalRef<jobject> SmsInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  SmsInfoBarDelegate* delegate =
      static_cast<SmsInfoBarDelegate*>(GetDelegate());

  auto title = ConvertUTF16ToJavaString(env, delegate->GetTitle());
  auto message = ConvertUTF16ToJavaString(env, delegate->GetMessageText());
  auto button = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));

  base::android::ScopedJavaLocalRef<jobject> window_android =
      web_contents_->GetNativeView()->GetWindowAndroid()->GetJavaObject();

  return Java_WebOTPServiceInfoBar_create(
      env, window_android, resource_id_mapper.Run(delegate->GetIconId()), title,
      message, button);
}

}  // namespace sms
