// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_ADDRESS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_ADDRESS_ACTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/required_fields_fallback_handler.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace autofill_assistant {
class ClientStatus;

// An action to autofill a form using a local address.
class UseAddressAction : public Action {
 public:
  explicit UseAddressAction(ActionDelegate* delegate, const ActionProto& proto);
  ~UseAddressAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& final_status,
                 const base::Optional<ClientStatus>& optional_details_status =
                     base::nullopt);

  // Fill the form using data in client memory. Return whether filling succeeded
  // or not through OnFormFilled.
  void FillFormWithData();
  void OnWaitForElement(const ClientStatus& element_status);

  // Called when the address has been filled.
  void OnFormFilled(std::unique_ptr<RequiredFieldsFallbackHandler::FallbackData>
                        fallback_data,
                    const ClientStatus& status);

  // Create fallback data.
  std::unique_ptr<RequiredFieldsFallbackHandler::FallbackData>
  CreateFallbackData(const autofill::AutofillProfile& profile);

  // Usage of the autofilled address.
  std::string name_;
  std::string prompt_;
  Selector selector_;

  std::unique_ptr<RequiredFieldsFallbackHandler>
      required_fields_fallback_handler_;

  ProcessActionCallback process_action_callback_;
  base::WeakPtrFactory<UseAddressAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UseAddressAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_AUTOFILL_ACTION_H_
