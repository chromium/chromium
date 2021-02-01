// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_UPI_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_UPI_BUBBLE_CONTROLLER_IMPL_H_

#include <string>

#include "chrome/browser/ui/autofill/payments/save_upi_bubble.h"
#include "chrome/browser/ui/autofill/payments/save_upi_bubble_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class SaveUPIBubbleControllerImpl
    : SaveUPIBubbleController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<SaveUPIBubbleControllerImpl> {
 public:
  ~SaveUPIBubbleControllerImpl() override;

  // Will use a bubble to ask the user if they want Chrome to remember the
  // |upi_id|.
  void OfferUpiIdLocalSave(
      const std::string& upi_id,
      base::OnceCallback<void(bool accept)> save_upi_prompt_callback);

  // autofill::SaveUPIBubbleController:
  base::string16 GetUpiId() const override;
  void OnAccept() override;
  void OnBubbleClosed() override;

 protected:
  explicit SaveUPIBubbleControllerImpl(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<SaveUPIBubbleControllerImpl>;

  void ShowBubble();

  // Weak reference. Will be nullptr if no bubble is currently shown.
  SaveUPIBubble* save_upi_bubble_ = nullptr;

  base::OnceCallback<void(bool accept)> save_upi_prompt_callback_;

  // The UPI ID (Virtual Payment Address) which we ask to save.
  std::string upi_id_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_UPI_BUBBLE_CONTROLLER_IMPL_H_
