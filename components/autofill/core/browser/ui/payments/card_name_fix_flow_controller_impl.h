// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_CONTROLLER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller.h"

namespace autofill {

class CardNameFixFlowView;

class CardNameFixFlowControllerImpl : public CardNameFixFlowController {
 public:
  CardNameFixFlowControllerImpl();
  ~CardNameFixFlowControllerImpl() override;

  void Show(CardNameFixFlowView* card_name_fix_flow_view,
            const base::string16& inferred_cardholder_name,
            base::OnceCallback<void(const base::string16&)> name_callback);

  // CardNameFixFlowController implementation.
  void OnConfirmNameDialogClosed() override;
  void OnNameAccepted(const base::string16& name) override;
  void OnDismissed() override;
  int GetIconId() const override;
  base::string16 GetCancelButtonLabel() const override;
  base::string16 GetInferredCardholderName() const override;
  base::string16 GetInferredNameTooltipText() const override;
  base::string16 GetInputLabel() const override;
  base::string16 GetInputPlaceholderText() const override;
  base::string16 GetSaveButtonLabel() const override;
  base::string16 GetTitleText() const override;

 private:
  // Inferred cardholder name from Gaia account.
  base::string16 inferred_cardholder_name_;

  // View that displays the fix flow prompt.
  CardNameFixFlowView* card_name_fix_flow_view_ = nullptr;

  // The callback to call once user confirms their name through the fix flow.
  base::OnceCallback<void(const base::string16&)> name_accepted_callback_;

  // Whether the prompt was shown to the user.
  bool shown_ = false;

  // Whether the user explicitly accepted or dismissed this prompt.
  bool had_user_interaction_ = false;

  DISALLOW_COPY_AND_ASSIGN(CardNameFixFlowControllerImpl);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_CONTROLLER_IMPL_H_
