// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_BUBBLE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"
#include "components/autofill/core/browser/ui/payments/save_payment_method_and_virtual_card_enroll_confirmation_ui_params.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}

namespace autofill {

inline constexpr int kMillisecondsUntilConfirmationBubbleIsShown = 1000;

// The experiment arm assigned to the user for the resurrecting churned users
// experiment. This experiment arm affects the bubble UI.
enum class AutofillEnableResurrectingPaymentsUsersTreatmentArm {
  // Security-focused experiment arm, resulting in a bubble that emphasizes the
  // security benefits of turning on payments autofill to the user.
  kSecurity = 0,
  // Convenience-focused experiment arm, resulting in a bubble that emphasizes
  // the convenience and speed benefits of turning on payments autofill to the
  // user.
  kConvenience = 1,
};

// Controller responsible for managing the payments churned user bubble, which
// is a bubble that prompts the user to turn payments autofill on if they have
// turned it off. Owned by TabFeatures.
class PaymentsChurnedUsersBubbleController
    : public AutofillBubbleControllerBase {
 public:
  DECLARE_USER_DATA(PaymentsChurnedUsersBubbleController);

  explicit PaymentsChurnedUsersBubbleController(
      tabs::TabInterface& tab_interface,
      content::WebContents* web_contents);
  PaymentsChurnedUsersBubbleController(
      const PaymentsChurnedUsersBubbleController&) = delete;
  PaymentsChurnedUsersBubbleController& operator=(
      const PaymentsChurnedUsersBubbleController&) = delete;
  ~PaymentsChurnedUsersBubbleController() override;

  static PaymentsChurnedUsersBubbleController* From(
      tabs::TabInterface& tab_interface);

  virtual void Show(base::OnceClosure accept_callback,
                    base::OnceClosure cancel_callback,
                    base::OnceClosure closed_callback,
                    AccountInfo account_info);
  void ReshowBubble();
  void OnBubbleClosed(PaymentsUiClosedReason closed_reason);
  AutofillBubbleBase* GetBubbleViewForTesting() { return bubble_view(); }
  AutofillEnableResurrectingPaymentsUsersTreatmentArm
  GetAutofillEnableResurrectingPaymentsUsersTreatmentArm() const;
  const AccountInfo& GetAccountInfo() const;
  void OnAcceptButton();
  void ShowConfirmationBubbleView();
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams
  GetConfirmationUiParams() const;
  base::OnceCallback<void(PaymentsUiClosedReason)> GetOnBubbleClosedCallback();
  base::WeakPtr<PaymentsChurnedUsersBubbleController> GetWeakPtr();

  // AutofillBubbleControllerBase:
  void OnBubbleDiscarded() override;
  bool CanBeReshown() const override;
  BubbleType GetBubbleType() const override;
  base::WeakPtr<BubbleControllerBase> GetBubbleControllerBaseWeakPtr() override;

 protected:
  // AutofillBubbleControllerBase:
  void DoShowBubble() override;

#if !BUILDFLAG(IS_ANDROID)
  std::optional<actions::ActionId> GetActionIdForPageAction() override;
  bool ShouldShowPageAction() override;
#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  ui::ScopedUnownedUserData<PaymentsChurnedUsersBubbleController>
      scoped_unowned_user_data_;

  bool is_reshow_ = false;
  AccountInfo account_info_;

  // Set to false if a user accepts, cancels, or closes the bubble. This ensures
  // the page action icon goes away so that the user isn't stuck with the icon
  // and re-showing the bubble in the omnibox.
  bool should_show_icon_ = true;

  // Denotes whether the user has accepted the churned users bubble. Since
  // there is a loading state that triggers after accepting the churned users
  // bubble, this variable is used to signal that an acceptance has occurred so
  // that the bubble is treated as accepted on close.
  bool is_accepted_ = false;

  base::OnceClosure accept_callback_;
  base::OnceClosure cancel_callback_;
  base::OnceClosure closed_callback_;

  base::WeakPtrFactory<PaymentsChurnedUsersBubbleController> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_BUBBLE_CONTROLLER_H_
