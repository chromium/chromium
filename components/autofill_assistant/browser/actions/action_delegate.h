// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

class GURL;

namespace autofill {
class CreditCard;
class PersonalDataManager;
}  // namespace autofill

namespace content {
class WebContents;
}  // namespace content

namespace autofill_assistant {
class ClientMemory;
class DetailsProto;
struct PaymentInformation;

// Action delegate called when processing actions.
class ActionDelegate {
 public:
  virtual ~ActionDelegate() = default;

  // Show status message on the bottom bar.
  virtual void ShowStatusMessage(const std::string& message) = 0;

  // Create a helper for checking for multiple element existence or field
  // values.
  virtual std::unique_ptr<BatchElementChecker> CreateBatchElementChecker() = 0;

  // Wait for a short time for a given selector to appear.
  //
  // Most actions should call this before issuing a command on an element, to
  // account for timing issues with needed elements not showing up right away.
  //
  // Longer-time waiting should still be controlled explicitly, using
  // WaitForDom.
  //
  // TODO(crbug.com/806868): Consider embedding that wait right into
  // WebController and eliminate double-lookup.
  virtual void WaitForElement(const std::vector<std::string>& selectors,
                              base::OnceCallback<void(bool)> callback) = 0;

  // Click the element given by |selectors| on the web page.
  virtual void ClickElement(const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Ask user to choose an address in personal data manager. GUID of the chosen
  // address will be returned through callback, otherwise empty string if the
  // user chose to continue manually.
  virtual void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Asks the user to provide the data used by UseAddressAction and
  // UseCreditCardAction.
  virtual void GetPaymentInformation(
      payments::mojom::PaymentOptionsPtr payment_options,
      base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
      const std::string& title) = 0;

  // Fill the address form given by |selectors| with the given address |guid| in
  // personal data manager.
  virtual void FillAddressForm(const std::string& guid,
                               const std::vector<std::string>& selectors,
                               base::OnceCallback<void(bool)> callback) = 0;

  // Ask user to choose a card in personal data manager. GUID of the chosen card
  // will be returned through callback, otherwise empty string if the user chose
  // to continue manually.
  virtual void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Fill the card form given by |selectors| with the given |card| and its
  // |cvc|. Return result asynchronously through |callback|.
  virtual void FillCardForm(std::unique_ptr<autofill::CreditCard> card,
                            const base::string16& cvc,
                            const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Select the option given by |selectors| and the value of the option to be
  // picked.
  virtual void SelectOption(const std::vector<std::string>& selectors,
                            const std::string& selected_option,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Focus on the element given by |selectors|.
  virtual void FocusElement(const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Highlight the element given by |selectors|.
  virtual void HighlightElement(const std::vector<std::string>& selectors,
                                base::OnceCallback<void(bool)> callback) = 0;

  // Set the |value| of field |selectors| and return the result through
  // |callback|. If |simulate_key_presses| is true, the value will be set by
  // clicking the field and then simulating key presses, otherwise the `value`
  // attribute will be set directly.
  virtual void SetFieldValue(const std::vector<std::string>& selectors,
                             const std::string& value,
                             bool simulate_key_presses,
                             base::OnceCallback<void(bool)> callback) = 0;

  // Return the outerHTML of an element given by |selectors|.
  virtual void GetOuterHtml(
      const std::vector<std::string>& selectors,
      base::OnceCallback<void(bool, const std::string&)> callback) = 0;

  // Load |url| in the current tab. Returns immediately, before the new page has
  // been loaded.
  virtual void LoadURL(const GURL& url) = 0;

  // Shut down Autofill Assistant at the end of the current script.
  virtual void Shutdown() = 0;

  // Restart Autofill Assistant at the end of the current script with a cleared
  // state.
  virtual void Restart() = 0;

  // Stop the current script and show |message| if it is not empty .
  virtual void StopCurrentScript(const std::string& message) = 0;

  // Return the current ClientMemory.
  virtual ClientMemory* GetClientMemory() = 0;

  // Get current personal data manager.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;

  // Get associated web contents.
  virtual content::WebContents* GetWebContents() = 0;

  // Hide contextual information.
  virtual void HideDetails() = 0;

  // Show contextual information.
  virtual void ShowDetails(const DetailsProto& details) = 0;

  // Show the progress bar with |message| and set it at |progress|%.
  virtual void ShowProgressBar(int progress, const std::string& message) = 0;

  // Hide the progress bar.
  virtual void HideProgressBar() = 0;

  // Show the overlay.
  virtual void ShowOverlay() = 0;

  // Hide the overlay.
  virtual void HideOverlay() = 0;

 protected:
  ActionDelegate() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
