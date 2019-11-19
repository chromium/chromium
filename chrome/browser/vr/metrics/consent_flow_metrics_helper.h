// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_METRICS_CONSENT_FLOW_METRICS_HELPER_H_
#define CHROME_BROWSER_VR_METRICS_CONSENT_FLOW_METRICS_HELPER_H_

#include <map>
#include <string>

#include "build/build_config.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "content/public/browser/web_contents_user_data.h"

namespace vr {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Adding a value at the end is okay.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class ConsentDialogAction : int {
  // The user gave permission to enter an immersive presentation.
  kUserAllowed = 0,
  // The user denied permission to enter an immersive presentation.
  kUserDenied = 1,
  // The user aborted the consent flow by clicking on the permission
  // dialog's 'X' system button.
  kUserAbortedConsentFlow = 2,
  // The user initially aborted consent flow or denied explicitly, only to
  // retry and allow this time.
  kUserAllowedAfterBounce = 3,
  // To insert a new enum, assign the next numeric value to it, and replace
  // the value of of this enum with the value of the added enum.
  kMaxValue = 3,
};

class VR_BASE_EXPORT ConsentFlowMetricsHelper
    : public content::WebContentsUserData<ConsentFlowMetricsHelper> {
 public:
  static ConsentFlowMetricsHelper* InitFromWebContents(
      content::WebContents* contents);

  ConsentFlowMetricsHelper();
  ~ConsentFlowMetricsHelper() override;

  void OnShowDialog();

  void OnDialogClosedWithConsent(const std::string& url, bool is_granted);
  void LogUserAction(ConsentDialogAction action);
  void LogConsentFlowDurationWhenConsentGranted();
  void LogConsentFlowDurationWhenConsentNotGranted();
  void LogConsentFlowDurationWhenUserAborted();

#if defined(OS_ANDROID)
  void OnDialogClosedWithConsent(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& url,
      jboolean is_granted);
  void LogUserAction(JNIEnv* env,
                     jint action);
  void LogConsentFlowDurationWhenConsentGranted(JNIEnv* env);
  void LogConsentFlowDurationWhenConsentNotGranted(JNIEnv* env);
  void LogConsentFlowDurationWhenUserAborted(JNIEnv* env);
#endif

 private:
  explicit ConsentFlowMetricsHelper(content::WebContents* contents);

  friend class content::WebContentsUserData<ConsentFlowMetricsHelper>;

  // This is to note that the user has either denied or aborted the consent
  // flow earlier, only to retry immediately and allow this time.
  void LogConsentFlowUserBounceAction();

  base::TimeTicks dialog_presented_at_;

  base::Optional<bool> previous_consent_;
  base::Optional<base::Time> previous_consent_flow_end_time_;

  std::string last_visited_url_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ConsentFlowMetricsHelper);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_METRICS_CONSENT_FLOW_METRICS_HELPER_H_
