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

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_BUBBLE_CONTROLLER_IMPL_H_
