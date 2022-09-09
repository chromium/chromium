// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_VIEW_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_bubble_controller.h"

namespace content {
class WebContents;
}

namespace autofill {

// Implements AutofillBubbleBase for displaying the Android view. This view
// class is created when the user info is to be shown in the virtual card
// enrollment flow, and is destroyed when the Android view gets dismissed either
// by the user or by the native controller.
class VirtualCardEnrollmentViewAndroid final : public AutofillBubbleBase {
 public:
  ~VirtualCardEnrollmentViewAndroid();

  // AutofillBubbleBase:
  void Hide() override;

  // Factory function for creating and showing the view.
  static AutofillBubbleBase* CreateAndShow(
      content::WebContents* web_contents,
      VirtualCardEnrollBubbleController* controller);

 private:
  VirtualCardEnrollmentViewAndroid();

  bool Show(content::WebContents* web_contents,
            VirtualCardEnrollBubbleController* controller);

  // This class's corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_view_bridge_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_VIEW_ANDROID_H_
