// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ADD_NEW_ADDRESS_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ADD_NEW_ADDRESS_BUBBLE_CONTROLLER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/address_bubble_controller_delegate.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/image_model.h"

namespace autofill {

// The controller used by `AddNewAddressBubbleView` to get content for
// the interface and communicate back user actions via the delegate. It is
// created outside the view but passed to it for ownership.
class AddNewAddressBubbleController : public content::WebContentsObserver {
 public:
  AddNewAddressBubbleController(
      content::WebContents* web_contents,
      base::WeakPtr<AddressBubbleControllerDelegate> delegate);
  AddNewAddressBubbleController(const AddNewAddressBubbleController&) = delete;
  AddNewAddressBubbleController& operator=(
      const AddNewAddressBubbleController&) = delete;
  ~AddNewAddressBubbleController() override;

  virtual std::u16string GetBodyText() const;
  virtual std::u16string GetFooterMessage() const;

  // Called by the view when the user made their decision. It can be done
  // explicitly (e.g. by pressing the cancel button) or implicitly (e.g. by
  // ignoring the bubble and eventually closing the tab).
  virtual void OnUserDecision(
      AutofillClient::AddressPromptUserDecision decision);

  // Called by the view when the prompt is accepted, the user presses
  // the "Add address"  button for it.
  virtual void OnAddButtonClicked();

  // Called by the view when the bubble window is being closed and the bubble
  // itself is about to be deleted.
  virtual void OnBubbleClosed();

 private:
  // The delegate is used to return the user decision, either accept or
  // reject/ignore the prompt.
  base::WeakPtr<AddressBubbleControllerDelegate> delegate_;

  // The country code that the new address will be initially assigned. Can be
  // changed in the editor later.
  const AddressCountryCode country_code_;

  // Whether the address profile will be saved in user's account.
  const bool is_eligible_for_account_storage_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_ADD_NEW_ADDRESS_BUBBLE_CONTROLLER_H_
