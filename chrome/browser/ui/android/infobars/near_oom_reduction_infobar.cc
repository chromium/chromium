// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/near_oom_reduction_infobar.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/android/chrome_jni_headers/NearOomReductionInfoBar_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/interventions/intervention_delegate.h"
#include "chrome/browser/ui/interventions/intervention_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"

NearOomReductionInfoBar::NearOomReductionInfoBar(InterventionDelegate* delegate)
    : InfoBarAndroid(std::make_unique<InterventionInfoBarDelegate>(
          infobars::InfoBarDelegate::InfoBarIdentifier::
              NEAR_OOM_REDUCTION_INFOBAR_ANDROID,
          delegate)),
      delegate_(delegate) {
  DCHECK(delegate_);
}

NearOomReductionInfoBar::~NearOomReductionInfoBar() = default;

void NearOomReductionInfoBar::ProcessButton(int action) {
  NOTREACHED();  // No button on this infobar.
}

base::android::ScopedJavaLocalRef<jobject>
NearOomReductionInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  return Java_NearOomReductionInfoBar_create(env);
}

void NearOomReductionInfoBar::OnLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  delegate_->DeclineInterventionWithReload();
  RemoveSelf();
}

// static
void NearOomReductionInfoBar::Show(content::WebContents* web_contents,
                                   InterventionDelegate* delegate) {
  InfoBarService* service = InfoBarService::FromWebContents(web_contents);
  service->AddInfoBar(base::WrapUnique(new NearOomReductionInfoBar(delegate)));
}
