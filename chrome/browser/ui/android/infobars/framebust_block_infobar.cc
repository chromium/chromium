// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/framebust_block_infobar.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/android/chrome_jni_headers/FramebustBlockInfoBar_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/interventions/framebust_block_message_delegate_bridge.h"
#include "chrome/browser/ui/interventions/framebust_block_message_delegate.h"
#include "chrome/browser/ui/interventions/intervention_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"

FramebustBlockInfoBar::FramebustBlockInfoBar(
    std::unique_ptr<FramebustBlockMessageDelegate> message_delegate)
    : InfoBarAndroid(std::make_unique<InterventionInfoBarDelegate>(
          infobars::InfoBarDelegate::InfoBarIdentifier::
              FRAMEBUST_BLOCK_INFOBAR_ANDROID,
          message_delegate.get())),
      delegate_(std::move(message_delegate)) {
  DCHECK(delegate_);
}

FramebustBlockInfoBar::~FramebustBlockInfoBar() = default;

void FramebustBlockInfoBar::ProcessButton(int action) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  // Tapping the button means that the user wants to bypass the intervention in
  // a sticky way, e.g. via content settings.
  DCHECK_EQ(action, InfoBarAndroid::ACTION_OK);
  delegate_->DeclineInterventionSticky();
  RemoveSelf();
}

void FramebustBlockInfoBar::OnLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  // Tapping the link means that the user wants to bypass the intervention by
  // navigating to the blocked URL.
  delegate_->DeclineIntervention();
  RemoveSelf();
}

base::android::ScopedJavaLocalRef<jobject>
FramebustBlockInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  return Java_FramebustBlockInfoBar_create(
      env, base::android::ConvertUTF8ToJavaString(
               env, delegate_->GetBlockedUrl().spec()));
}

// static
void FramebustBlockInfoBar::Show(
    content::WebContents* web_contents,
    std::unique_ptr<FramebustBlockMessageDelegate> message_delegate) {
  InfoBarService* service = InfoBarService::FromWebContents(web_contents);
  service->AddInfoBar(
      base::WrapUnique(new FramebustBlockInfoBar(std::move(message_delegate))),
      /*replace_existing=*/true);
}
