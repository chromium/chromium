// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ADDRESS_BUBBLE_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_ADDRESS_BUBBLE_CONTROLLER_DELEGATE_H_

#include <string>

#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"

namespace autofill {

// This delegate defines the way the address bubbles (for example, see
// `SaveAddressBubbleController`) communicate to `AddressBubblesController`.
class AddressBubbleControllerDelegate {
 public:
  // Requests to open the address editor bubble with the address presented in
  // the prompt bubble, it can be a new address to edit before saving or
  // an existing one to be updated. Called by an explicit user action, such as
  // a click on the edit button in the update bubble.
  virtual void ShowEditor(const AutofillProfile& address_profile,
                          const std::u16string& title_override,
                          const std::u16string& editor_footer_message,
                          bool is_editing_existing_address) = 0;

  virtual void OnUserDecision(
      AutofillClient::AddressPromptUserDecision decision,
      base::optional_ref<const AutofillProfile> profile) = 0;
  virtual void OnBubbleClosed() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_ADDRESS_BUBBLE_CONTROLLER_DELEGATE_H_
