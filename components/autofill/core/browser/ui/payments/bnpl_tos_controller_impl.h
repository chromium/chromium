// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_IMPL_H_

#include "base/functional/callback.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace autofill {

class AutofillClient;
class BnplTosView;

class BnplTosControllerImpl : public BnplTosController {
 public:
  explicit BnplTosControllerImpl(AutofillClient* client);

  BnplTosControllerImpl(const BnplTosControllerImpl&) = delete;
  BnplTosControllerImpl& operator=(const BnplTosControllerImpl&) = delete;

  ~BnplTosControllerImpl() override;

  // BnplTosController:
  void OnUserAccepted() override;
  void OnUserCancelled() override;
  std::u16string GetOkButtonLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetTitle() const override;
  std::u16string GetReviewText() const override;
  std::u16string GetApproveText() const override;
  TextWithLink GetLinkText() const override;
  const LegalMessageLines& GetLegalMessageLines() const override;
  AccountInfo GetAccountInfo() const override;
  BnplIssuer::IssuerId GetIssuerId() const override;
  base::WeakPtr<BnplTosController> GetWeakPtr() override;

  // Show the BNPL ToS view. `create_and_show_view_callback` will be invoked
  // immediately to create the view. `model` contains the information needed to
  // populate the data in the view.
  void Show(base::OnceCallback<std::unique_ptr<BnplTosView>()>
                create_and_show_view_callback,
            BnplTosModel model,
            base::OnceClosure accept_callback,
            base::OnceClosure cancel_callback);

  // Dismiss the BNPL ToS view.
  void Dismiss();

 private:
  friend class BnplTosControllerImplTest;
  std::unique_ptr<BnplTosView> view_;

  BnplTosModel model_;

  base::OnceClosure accept_callback_;
  base::OnceClosure cancel_callback_;

  const raw_ref<AutofillClient> client_;

  base::WeakPtrFactory<BnplTosControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_IMPL_H_
