// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/get_payment_information_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace autofill_assistant {

GetPaymentInformationAction::GetPaymentInformationAction(
    const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_get_payment_information());
}

GetPaymentInformationAction::~GetPaymentInformationAction() {}

void GetPaymentInformationAction::InternalProcessAction(
    ActionDelegate* delegate,
    ProcessActionCallback callback) {
  const GetPaymentInformationProto& get_payment_information =
      proto_.get_payment_information();

  payments::mojom::PaymentOptionsPtr payment_options =
      payments::mojom::PaymentOptions::New();
  bool ask_for_payment = get_payment_information.ask_for_payment();
  payment_options->request_payer_email = ask_for_payment;
  payment_options->request_payer_name = ask_for_payment;
  payment_options->request_payer_phone = ask_for_payment;

  payment_options->request_shipping =
      !get_payment_information.shipping_address_name().empty();

  delegate->GetPaymentInformation(
      std::move(payment_options),
      base::BindOnce(&GetPaymentInformationAction::OnGetPaymentInformation,
                     weak_ptr_factory_.GetWeakPtr(), delegate,
                     std::move(get_payment_information), std::move(callback)),
      get_payment_information.prompt());
}

void GetPaymentInformationAction::OnGetPaymentInformation(
    ActionDelegate* delegate,
    const GetPaymentInformationProto& get_payment_information,
    ProcessActionCallback callback,
    std::unique_ptr<PaymentInformation> payment_information) {
  bool succeed = payment_information->succeed;

  if (succeed) {
    if (get_payment_information.ask_for_payment()) {
      DCHECK(!payment_information->card_guid.empty());
      delegate->GetClientMemory()->set_selected_card(
          payment_information->card_guid);
      processed_action_proto_->set_card_issuer_network(
          payment_information->card_issuer_network);
    }

    if (!get_payment_information.shipping_address_name().empty()) {
      DCHECK(!payment_information->address_guid.empty());
      delegate->GetClientMemory()->set_selected_address(
          get_payment_information.shipping_address_name(),
          payment_information->address_guid);
    }
  }

  UpdateProcessedAction(succeed ? ACTION_APPLIED : PAYMENT_REQUEST_ERROR);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
