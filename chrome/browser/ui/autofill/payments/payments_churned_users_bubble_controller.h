// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_BUBBLE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}

namespace autofill {

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

  void Show();
  void ReshowBubble();

  // AutofillBubbleControllerBase:
  void OnBubbleDiscarded() override;
  void OnBubbleClosed();
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

  base::WeakPtrFactory<PaymentsChurnedUsersBubbleController> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_BUBBLE_CONTROLLER_H_
