// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_IMPL_H_

#include "base/functional/callback.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"

namespace autofill {

class BnplTosView;

class BnplTosControllerImpl : public BnplTosController {
 public:
  BnplTosControllerImpl();

  BnplTosControllerImpl(const BnplTosControllerImpl&) = delete;
  BnplTosControllerImpl& operator=(const BnplTosControllerImpl&) = delete;

  ~BnplTosControllerImpl() override;

  // BnplTosController:
  void OnViewClosing(bool user_accepted) override;
  std::u16string GetOkButtonLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetTitle() const override;
  std::u16string GetReviewText() const override;
  std::u16string GetApproveText() const override;
  TextWithLink GetLinkText() const override;
  const LegalMessageLines& GetLegalMessageLines() const override;
  const AccountInfo& GetAccountInfo() const override;
  const std::string& GetIssuerId() const override;
  base::WeakPtr<BnplTosController> GetWeakPtr() override;

  // Show the BNPL ToS view. `create_and_show_view_callback` will be invoked
  // immediately to create the view. `model` contains the information needed to
  // populate the data in the view.
  void Show(base::OnceCallback<std::unique_ptr<BnplTosView>()>
                create_and_show_view_callback,
            BnplTosModel model,
            base::OnceClosure accept_callback,
            base::OnceClosure cancel_callback);

 private:
  friend class BnplTosControllerImplTest;
  std::unique_ptr<BnplTosView> view_;

  BnplTosModel model_;

  base::OnceClosure accept_callback_;
  base::OnceClosure cancel_callback_;

  base::WeakPtrFactory<BnplTosControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_IMPL_H_
