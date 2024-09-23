// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_ADDRESS_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_ADDRESS_BUBBLE_CONTROLLER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/address_bubble_controller_delegate.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/image_model.h"

namespace autofill {

// The controller used by `SaveAddressProfileView` to get content for
// the interface and communicate back user input via the delegate. It is created
// outside the view but passed to it for ownership.
class SaveAddressBubbleController : public content::WebContentsObserver {
 public:
  struct HeaderImages {
    ui::ImageModel light;
    ui::ImageModel dark;
  };

  SaveAddressBubbleController(
      base::WeakPtr<AddressBubbleControllerDelegate> delegate,
      content::WebContents* web_contents,
      const AutofillProfile& address_profile,
      bool is_migration_to_account);
  SaveAddressBubbleController(const SaveAddressBubbleController&) = delete;
  SaveAddressBubbleController& operator=(const SaveAddressBubbleController&) =
      delete;
  ~SaveAddressBubbleController() override;

  virtual std::u16string GetWindowTitle() const;
  virtual std::optional<HeaderImages> GetHeaderImages() const;
  virtual std::u16string GetBodyText() const;
  virtual std::u16string GetAddressSummary() const;
  virtual std::u16string GetProfileEmail() const;
  virtual std::u16string GetProfilePhone() const;
  virtual std::u16string GetOkButtonLabel() const;
  // The value returned by the cancel button callback depends on whether
  // the address is to be saved into user's account. Different values are needed
  // to have different logic for the popup reappearence eligibility.
  virtual AutofillClient::AddressPromptUserDecision GetCancelCallbackValue()
      const;
  virtual std::u16string GetFooterMessage() const;
  virtual std::u16string GetEditorFooterMessage() const;

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
  // The delegate is used to return the user decision or notify about events
  // important for higher level processes, e.g. saving the address with editing.
  base::WeakPtr<AddressBubbleControllerDelegate> delegate_;

  // Contains the details of the address profile that will be saved if the user
  // accepts.
  const AutofillProfile address_profile_;

  // Whether the bubble prompts to save (migrate) the profile into account.
  const bool is_migration_to_account_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_ADDRESS_BUBBLE_CONTROLLER_H_
