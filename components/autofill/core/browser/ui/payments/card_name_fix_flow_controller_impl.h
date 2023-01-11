// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_CONTROLLER_IMPL_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller.h"

namespace autofill {

class CardNameFixFlowView;

class CardNameFixFlowControllerImpl : public CardNameFixFlowController {
 public:
  CardNameFixFlowControllerImpl();

  CardNameFixFlowControllerImpl(const CardNameFixFlowControllerImpl&) = delete;
  CardNameFixFlowControllerImpl& operator=(
      const CardNameFixFlowControllerImpl&) = delete;

  ~CardNameFixFlowControllerImpl() override;

  // Show the cardholder name fix flow view. If another view is triggered
  // when the current view has not been dismissed by user yet, the current
  // view will be destroyed.
  void Show(CardNameFixFlowView* card_name_fix_flow_view,
            const std::u16string& inferred_cardholder_name,
            base::OnceCallback<void(const std::u16string&)> name_callback);

  // CardNameFixFlowController implementation.
  void OnConfirmNameDialogClosed() override;
  void OnNameAccepted(const std::u16string& name) override;
  void OnDismissed() override;
  int GetIconId() const override;
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetInferredCardholderName() const override;
  std::u16string GetInferredNameTooltipText() const override;
  std::u16string GetInputLabel() const override;
  std::u16string GetInputPlaceholderText() const override;
  std::u16string GetSaveButtonLabel() const override;
  std::u16string GetTitleText() const override;

 private:
  // Inferred cardholder name from Gaia account.
  std::u16string inferred_cardholder_name_;

  // View that displays the fix flow prompt.
  raw_ptr<CardNameFixFlowView> card_name_fix_flow_view_ = nullptr;

  // The callback to call once user confirms their name through the fix flow.
  base::OnceCallback<void(const std::u16string&)> name_accepted_callback_;

  // Whether the prompt was shown to the user.
  bool shown_ = false;

  // Whether the user explicitly accepted or dismissed this prompt.
  bool had_user_interaction_ = false;

  // Destroy |card_name_fix_flow_view_| if it is valid. |controller_gone|
  // is true if |this| controller is being destroyed.
  void MaybeDestroyCardNameFixFlowView(bool controller_gone);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_CONTROLLER_IMPL_H_
