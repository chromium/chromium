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

}  // namespace security_state::internal

// The actual JNI function, now a thin wrapper.
static int32_t JNI_SecurityStateModel_GetMaliciousContentStatusForWebContents(
    JNIEnv* env,
    content::WebContents* web_contents) {
  return security_state::internal::
      GetMaliciousContentStatusForWebContentsInternal(
          web_contents, security_state::GetSecurityStateModelDelegate());
}

// The actual JNI function, now a thin wrapper.
static int32_t JNI_SecurityStateModel_GetSecurityLevelForWebContents(
    JNIEnv* env,
    content::WebContents* web_contents) {
  return security_state::internal::GetSecurityLevelForWebContentsInternal(
      web_contents, security_state::GetSecurityStateModelDelegate());
}

static bool JNI_SecurityStateModel_IsHttpsOnlyModeUpgradedForWebContents(
    JNIEnv* env,
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  SecurityStateModelDelegate* delegate =
      security_state::GetSecurityStateModelDelegate();
  if (!delegate) {
    // Embedders without a delegate (e.g. WebView) never upgrade navigations
    // via HTTPS-Only Mode.
    return false;
  }
  return delegate->GetVisibleSecurityState(web_contents)
      ->is_https_only_mode_upgraded;
}

DEFINE_JNI(SecurityStateModel)
