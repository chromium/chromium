// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/autofill_assistant/browser/payment_information.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace autofill_assistant {
struct ScriptHandle;
class DetailsProto;

// Controller to control autofill assistant UI.
class UiController {
 public:
  virtual ~UiController() = default;

  // Set assistant UI delegate called by assistant UI controller.
  virtual void SetUiDelegate(UiDelegate* ui_delegate) = 0;

  // Show status message on the bottom bar.
  virtual void ShowStatusMessage(const std::string& message) = 0;

  // Show the overlay.
  virtual void ShowOverlay() = 0;

  // Hide the overlay.
  virtual void HideOverlay() = 0;

  // Shuts down Autofill Assistant: hide the UI and frees any associated state.
  //
  // Warning: this indirectly deletes the caller.
  virtual void Shutdown() = 0;

  // Update the list of scripts in the UI.
  virtual void UpdateScripts(const std::vector<ScriptHandle>& scripts) = 0;

  // Show UI to ask user to choose an address in personal data manager. GUID of
  // the chosen address will be returned through callback, otherwise empty
  // string if the user chose to continue manually.
  // TODO(806868): Return full address object instead of GUID (the GUID can
  // change after synchronization with the server).
  virtual void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Show UI to ask user to choose a card in personal data manager. GUID of the
  // chosen card will be returned through callback, otherwise empty string if
  // the user chose to continue manually.
  // TODO(806868): Return full card object instead of GUID (the GUID can change
  // after synchronization with the server).
  virtual void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Get payment information (through similar to payment request UX) to fill
  // forms.
  virtual void GetPaymentInformation(
      payments::mojom::PaymentOptionsPtr payment_options,
      base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
      const std::string& title) = 0;

  // Hide contextual information.
  virtual void HideDetails() = 0;

  // Show contextual information.
  virtual void ShowDetails(const DetailsProto& details) = 0;

  // Show the progress bar with |message| and set it at |progress|%.
  virtual void ShowProgressBar(int progress, const std::string& message) = 0;

  // Hide the progress bar.
  virtual void HideProgressBar() = 0;

 protected:
  UiController() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_
