// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_ADDRESS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_ADDRESS_ACTION_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace autofill_assistant {
class ClientStatus;

// An action to autofill a form using a local address.
class UseAddressAction : public Action {
 public:
  explicit UseAddressAction(ActionDelegate* delegate, const ActionProto& proto);

  UseAddressAction(const UseAddressAction&) = delete;
  UseAddressAction& operator=(const UseAddressAction&) = delete;

  ~UseAddressAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  // Fill the form using |profile_|. Return whether filling succeeded or not
  // through OnFormFilled.
  void FillFormWithData();
  void OnWaitForElement(const ClientStatus& element_status);

  void InitFallbackHandler(const autofill::AutofillProfile& profile);

  // Called when the address has been filled.
  void ExecuteFallback(const ClientStatus& status);

  // Note: |fallback_handler_| must be a member, because checking for fallbacks
  // is asynchronous and the existence of the handler must be ensured.
  std::unique_ptr<RequiredFieldsFallbackHandler> fallback_handler_;
  std::unique_ptr<autofill::AutofillProfile> profile_;
  Selector selector_;

  ProcessActionCallback process_action_callback_;
  base::WeakPtrFactory<UseAddressAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_ADDRESS_ACTION_H_
