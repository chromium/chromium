// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_AI_ATTRIBUTION_VERIFICATION_SHEET_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_AI_ATTRIBUTION_VERIFICATION_SHEET_BRIDGE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "third_party/jni_zero/jni_zero.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// Bridge class providing a C++ entry point to trigger the Autofill AI
// attribution verification bottom sheet on Android.
class AutofillAiAttributionVerificationSheetBridge {
 public:
  explicit AutofillAiAttributionVerificationSheetBridge(
      content::WebContents* web_contents);

  AutofillAiAttributionVerificationSheetBridge(
      const AutofillAiAttributionVerificationSheetBridge&) = delete;
  AutofillAiAttributionVerificationSheetBridge& operator=(
      const AutofillAiAttributionVerificationSheetBridge&) = delete;

  virtual ~AutofillAiAttributionVerificationSheetBridge();

  // Requests showing the attribution verification bottom sheet for the window
  // hosting `web_contents_`. If an earlier sheet session is still active, its
  // callback is resolved with `kDismiss` before showing the new sheet.
  virtual void RequestShowContent(
      AutofillClient::AutofillAiVerificationDecisionCallback decision_cb);

  // -- JNI calls bridged from Java --
  void OnDismissed();

 private:
  friend class AutofillAiAttributionVerificationSheetBridgeTestApi;

  void ResetJavaBridge();

  const raw_ref<content::WebContents> web_contents_;
  jni_zero::ScopedJavaGlobalRef<jobject> java_bridge_;
  AutofillClient::AutofillAiVerificationDecisionCallback decision_cb_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_AI_ATTRIBUTION_VERIFICATION_SHEET_BRIDGE_H_
