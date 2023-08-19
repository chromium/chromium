// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_BUBBLE_CONTROLLER_IMPL_H_

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller.h"
#include "content/public/browser/web_contents_user_data.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/mandatory_reauth/android/mandatory_reauth_opt_in_view_android.h"
#endif

namespace autofill {

class MandatoryReauthBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public MandatoryReauthBubbleController,
      public content::WebContentsUserData<MandatoryReauthBubbleControllerImpl> {
 public:
  MandatoryReauthBubbleControllerImpl(
      const MandatoryReauthBubbleControllerImpl&) = delete;
  MandatoryReauthBubbleControllerImpl& operator=(
      const MandatoryReauthBubbleControllerImpl&) = delete;
  ~MandatoryReauthBubbleControllerImpl() override;

  void ShowBubble(base::OnceClosure accept_mandatory_reauth_callback,
                  base::OnceClosure cancel_mandatory_reauth_callback,
                  base::RepeatingClosure close_mandatory_reauth_callback);
  void ReshowBubble();

  // MandatoryReauthBubbleController:
  std::u16string GetWindowTitle() const override;
  std::u16string GetAcceptButtonText() const override;
  std::u16string GetCancelButtonText() const override;
  std::u16string GetExplanationText() const override;
  void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) override;
#if BUILDFLAG(IS_ANDROID)
  void OnClosed(JNIEnv* env, jint closed_reason);
#endif
  AutofillBubbleBase* GetBubbleView() override;
  bool IsIconVisible() override;
  MandatoryReauthBubbleType GetBubbleType() const override;

 protected:
  explicit MandatoryReauthBubbleControllerImpl(
      content::WebContents* web_contents);

  // AutofillBubbleControllerBase:
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<
      MandatoryReauthBubbleControllerImpl>;

  base::OnceClosure accept_mandatory_reauth_callback_;
  base::OnceClosure cancel_mandatory_reauth_callback_;
  base::RepeatingClosure close_mandatory_reauth_callback_;

  // The type of bubble currently displayed to the user.
  MandatoryReauthBubbleType current_bubble_type_ =
      MandatoryReauthBubbleType::kInactive;

#if BUILDFLAG(IS_ANDROID)
  // Handles Android view's lifecycle. The Desktop view is handled by the base
  // class `AutofillBubbleControllerBase`.
  std::unique_ptr<MandatoryReauthOptInViewAndroid> view_android_;

  // This class's corresponding Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_controller_bridge_;

  base::android::ScopedJavaLocalRef<jobject> GetJavaControllerBridge() override;
#endif

  // Whether the bubble is shown after user interacted with omnibox icon.
  bool is_reshow_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_BUBBLE_CONTROLLER_IMPL_H_
