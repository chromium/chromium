// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/virtual_card_enrollment_view_android.h"

#include <jni.h>
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/VirtualCardEnrollmentDialogViewBridge_jni.h"
#include "chrome/browser/ui/android/autofill/virtual_card_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

namespace content {
class WebContents;
}

namespace autofill {

VirtualCardEnrollmentViewAndroid::VirtualCardEnrollmentViewAndroid() = default;

VirtualCardEnrollmentViewAndroid::~VirtualCardEnrollmentViewAndroid() = default;

AutofillBubbleBase* VirtualCardEnrollmentViewAndroid::CreateAndShow(
    content::WebContents* web_contents,
    VirtualCardEnrollBubbleController* controller) {
  VirtualCardEnrollmentViewAndroid* view =
      new VirtualCardEnrollmentViewAndroid();
  if (view->Show(web_contents, controller)) {
    return view;
  }
  delete view;
  return nullptr;
}

void VirtualCardEnrollmentViewAndroid::Hide() {
  if (java_view_bridge_) {
    Java_VirtualCardEnrollmentDialogViewBridge_dismiss(
        base::android::AttachCurrentThread(), java_view_bridge_);
  }
}

bool VirtualCardEnrollmentViewAndroid::Show(
    content::WebContents* web_contents,
    VirtualCardEnrollBubbleController* controller) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (web_contents->GetNativeView() == nullptr ||
      web_contents->GetNativeView()->GetWindowAndroid() == nullptr) {
    return false;  // No window attached (yet or anymore).
  }
  VirtualCardEnrollmentFields fields =
      controller->GetVirtualCardEnrollmentFields();
  ui::ViewAndroid* view_android = web_contents->GetNativeView();
  if (java_view_bridge_) {
    Hide();
  }
  java_view_bridge_.Reset(Java_VirtualCardEnrollmentDialogViewBridge_create(
      env,
      base::android::ConvertUTF16ToJavaString(
          env, controller->GetAcceptButtonText()),
      base::android::ConvertUTF16ToJavaString(
          env, controller->GetDeclineButtonText()),
      controller->GetOrCreateJavaDelegate(),
      autofill::CreateVirtualCardEnrollmentFieldsJavaObject(&fields),
      view_android->GetWindowAndroid()->GetJavaObject()));
  if (java_view_bridge_) {
    Java_VirtualCardEnrollmentDialogViewBridge_showDialog(env,
                                                          java_view_bridge_);
    return true;
  }
  return false;
}

}  // namespace autofill
