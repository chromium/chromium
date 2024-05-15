// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class AutofillVirtualCardEnrollmentInfoBarDelegateMobile;

// Bridge for the virtual card enrollment bottom sheet on Android.
class AutofillVCNEnrollBottomSheetBridge {
 public:
  AutofillVCNEnrollBottomSheetBridge();

  AutofillVCNEnrollBottomSheetBridge(
      const AutofillVCNEnrollBottomSheetBridge&) = delete;
  AutofillVCNEnrollBottomSheetBridge& operator=(
      const AutofillVCNEnrollBottomSheetBridge&) = delete;

  virtual ~AutofillVCNEnrollBottomSheetBridge();

  // Requests to show the virtual card enrollment bottom sheet.
  // Returns true if the bottom sheet was shown.
  bool RequestShowContent(
      content::WebContents* web_contents,
      std::unique_ptr<AutofillVirtualCardEnrollmentInfoBarDelegateMobile>
          delegate);

  // Hides the virtual card enrollment bottom sheet.
  virtual void Hide();

  void OnAccept(JNIEnv* env);
  void OnCancel(JNIEnv* env);
  void OnDismiss(JNIEnv* env);
  void RecordLinkClickMetric(JNIEnv* env, int link_type);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
  std::unique_ptr<AutofillVirtualCardEnrollmentInfoBarDelegateMobile> delegate_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE_H_
