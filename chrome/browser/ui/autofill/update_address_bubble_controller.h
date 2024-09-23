// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_UPDATE_ADDRESS_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_UPDATE_ADDRESS_BUBBLE_CONTROLLER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents_observer.h"

namespace autofill {

class AddressBubbleControllerDelegate;

// The controller used by `UpdateAddressProfileView` to get content for
// the interface and communicate back user input via the delegate. It is created
// outside the view but passed to it for ownership.
class UpdateAddressBubbleController : public content::WebContentsObserver {
 public:
  UpdateAddressBubbleController(
      base::WeakPtr<AddressBubbleControllerDelegate> delegate,
      content::WebContents* web_contents,
      const AutofillProfile& profile_to_save,
      const AutofillProfile& original_profile);
  UpdateAddressBubbleController(const UpdateAddressBubbleController&) = delete;
  UpdateAddressBubbleController& operator=(
      const UpdateAddressBubbleController&) = delete;
  ~UpdateAddressBubbleController() override;

  virtual std::u16string GetWindowTitle() const;
  virtual std::u16string GetFooterMessage() const;
  virtual const AutofillProfile& GetProfileToSave() const;
  virtual const AutofillProfile& GetOriginalProfile() const;

  // Called by the view when the user made their decision. It can be done
  // explicitly (e.g. by pressing the cancel button) or implicitly (e.g. by
  // ignoring the bubble and eventually closing the tab).
  virtual void OnUserDecision(
      AutofillClient::AddressPromptUserDecision decision,
      base::optional_ref<const AutofillProfile> profile);
  // Called by the view when the address proposed for saving needs to be
  // modified, the user presses the edit button for it.
  virtual void OnEditButtonClicked();

  // Called by the view when the bubble window is being closed and the bubble
  // itself is about to be deleted.
  virtual void OnBubbleClosed();

 private:
  base::WeakPtr<AddressBubbleControllerDelegate> delegate_;

  // Contains the details of the address profile that will be saved if the user
  // accepts.
  const AutofillProfile profile_to_save_;

  // Contains the details of the address profile that will be updated if the
  // user accepts the prompt.
  const AutofillProfile original_profile_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_UPDATE_ADDRESS_BUBBLE_CONTROLLER_H_
