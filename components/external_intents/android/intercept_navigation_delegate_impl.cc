// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/external_intents/android/jni_headers/InterceptNavigationDelegateImpl_jni.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace external_intents {

static void JNI_InterceptNavigationDelegateImpl_AssociateWithWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jdelegate,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  navigation_interception::InterceptNavigationDelegate::Associate(
      web_contents,
      std::make_unique<navigation_interception::InterceptNavigationDelegate>(
          env, jdelegate, /*escape_external_handler_value=*/true));
}

}  // namespace external_intents
