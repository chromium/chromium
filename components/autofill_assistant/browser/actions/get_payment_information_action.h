// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GET_PAYMENT_INFORMATION_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GET_PAYMENT_INFORMATION_ACTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/payment_information.h"

namespace autofill_assistant {

// Triggers PaymentRequest to collect user data.
class GetPaymentInformationAction : public Action {
 public:
  explicit GetPaymentInformationAction(const ActionProto& proto);
  ~GetPaymentInformationAction() override;

 private:
  void InternalProcessAction(ActionDelegate* delegate,
                             ProcessActionCallback callback) override;

  void OnGetPaymentInformation(
      ActionDelegate* delegate,
      const GetPaymentInformationProto& get_payment_information,
      ProcessActionCallback callback,
      std::unique_ptr<PaymentInformation> payment_information);

  base::WeakPtrFactory<GetPaymentInformationAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GetPaymentInformationAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GET_PAYMENT_INFORMATION_ACTION_H_
