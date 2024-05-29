// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/security_state/content/android/security_state_client.h"
#include "components/security_state/content/android/security_state_model_delegate.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/security_state/content/android/jni_headers/SecurityStateModel_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;

// static
jint JNI_SecurityStateModel_GetSecurityLevelForWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  security_state::SecurityStateClient* security_state_client =
      security_state::GetSecurityStateClient();

  // At this time, the usage of the client isn't mandatory as it is used only
  // for optional overriding of default behavior.
  if (security_state_client) {
    std::unique_ptr<SecurityStateModelDelegate> security_state_model_delegate =
        security_state_client->MaybeCreateSecurityStateModelDelegate();

    if (security_state_model_delegate)
      return security_state_model_delegate->GetSecurityLevel(web_contents);
  }

  return security_state::GetSecurityLevel(
      *security_state::GetVisibleSecurityState(web_contents),
      /* used_policy_installed_certificate= */ false);
}
