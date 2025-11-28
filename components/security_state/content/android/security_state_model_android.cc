// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/content/android/security_state_client.h"
#include "components/security_state/content/android/security_state_model_delegate.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"

// This generated header with the static declaration for the JNI function.
#include "components/security_state/content/android/jni_headers/SecurityStateModel_jni.h"

using security_state::GetSecurityStateClient;
using security_state::MaliciousContentStatus;
using security_state::SecurityLevel;
using security_state::SecurityStateClient;

namespace security_state::internal {

SecurityStateModelDelegate* CreateSecurityStateModelDelegate() {
  SecurityStateClient* security_state_client = GetSecurityStateClient();
  if (!security_state_client) {
    return nullptr;
  }
  // Transfer ownership to caller which should manage memory for the created
  // security state client.
  return security_state_client->MaybeCreateSecurityStateModelDelegate()
      .release();
}

// This function is testable from the unit test file.
MaliciousContentStatus GetMaliciousContentStatusForWebContentsInternal(
    content::WebContents* web_contents,
    SecurityStateModelDelegate* delegate) {
  if (!web_contents) {
    return MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_NONE;
  }

  if (!delegate) {
    return MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_NONE;
  }
  return delegate->GetMaliciousContentStatus(web_contents);
}

// This function is testable from the unit test file.
SecurityLevel GetSecurityLevelForWebContentsInternal(
    content::WebContents* web_contents,
    SecurityStateModelDelegate* delegate) {
  if (!web_contents) {
    return SecurityLevel::NONE;
  }

  if (!delegate) {
    return security_state::GetSecurityLevel(
        *security_state::GetVisibleSecurityState(web_contents));
  }

  return delegate->GetSecurityLevel(web_contents);
}

// Provides thread-safe, on-demand access to the SecurityStateModelDelegate
// instance. Returns nullptr if the delegate cannot be created.
SecurityStateModelDelegate* GetSecurityStateModelDelegate() {
  // Function-local static pointer initialized exactly once (thread-safe since
  // C++11). This pointer once initialized is maintained in memory till the
  // process terminates.
  static SecurityStateModelDelegate* const delegate =
      CreateSecurityStateModelDelegate();

  return delegate;
}

}  // namespace security_state::internal

// The actual JNI function, now a thin wrapper.
static jint JNI_SecurityStateModel_GetMaliciousContentStatusForWebContents(
    JNIEnv* env,
    content::WebContents* web_contents) {
  return security_state::internal::
      GetMaliciousContentStatusForWebContentsInternal(
          web_contents,
          security_state::internal::GetSecurityStateModelDelegate());
}

// The actual JNI function, now a thin wrapper.
static jint JNI_SecurityStateModel_GetSecurityLevelForWebContents(
    JNIEnv* env,
    content::WebContents* web_contents) {
  return security_state::internal::GetSecurityLevelForWebContentsInternal(
      web_contents, security_state::internal::GetSecurityStateModelDelegate());
}

DEFINE_JNI(SecurityStateModel)
