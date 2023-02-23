// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_IBAN_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_IBAN_BUBBLE_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/payments/iban_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/save_iban_ui.h"
#include "chrome/browser/ui/autofill/payments/save_payment_icon_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

enum class IbanBubbleType;

// Implementation of per-tab class to control the IBAN save bubble, manage saved
// IBAN bubble, and Omnibox icon.
class IbanBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public IbanBubbleController,
      public SavePaymentIconController,
      public content::WebContentsUserData<IbanBubbleControllerImpl> {
 public:
  // An observer class used by browsertests that gets notified whenever
  // particular actions occur.
  class ObserverForTest {
   public:
    virtual void OnBubbleShown() = 0;
    virtual void OnIconShown() = 0;
  };

  IbanBubbleControllerImpl(const IbanBubbleControllerImpl&) = delete;
  IbanBubbleControllerImpl& operator=(const IbanBubbleControllerImpl&) = delete;
  ~IbanBubbleControllerImpl() override;

  // Sets up the controller and offers to save the `iban` locally.
  // `save_iban_prompt_callback` will be invoked once the user makes a decision
  // with respect to the offer-to-save prompt.
  void OfferLocalSave(
      const IBAN& iban,
      bool should_show_prompt,
      AutofillClient::LocalSaveIBANPromptCallback save_iban_prompt_callback);

  // No-op if the bubble is already shown, otherwise, shows the bubble.
  void ReshowBubble();

  // IbanBubbleController:
  std::u16string GetWindowTitle() const override;
  std::u16string GetAcceptButtonText() const override;
  std::u16string GetDeclineButtonText() const override;
  const IBAN& GetIBAN() const override;

  void OnAcceptButton(const std::u16string& nickname) override;
  void OnCancelButton() override;
  void OnManageSavedIbanExtraButtonClicked() override;
  void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) override;
  IbanBubbleType GetBubbleType() const override;

  // SavePaymentIconController:
  std::u16string GetSavePaymentIconTooltipText() const override;
  bool ShouldShowSavingPaymentAnimation() const override;
  bool ShouldShowPaymentSavedLabelAnimation() const override;
  bool ShouldShowSaveFailureBadge() const override;
  void OnAnimationEnded() override;
  bool IsIconVisible() const override;
  AutofillBubbleBase* GetPaymentBubbleView() const override;
  PaymentBubbleType GetPaymentBubbleType() const override;

  // For testing.
  void SetEventObserverForTesting(ObserverForTest* observer) {
    observer_for_testing_ = observer;
  }

 protected:
  explicit IbanBubbleControllerImpl(content::WebContents* web_contents);

  // AutofillBubbleControllerBase:
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<IbanBubbleControllerImpl>;

  // Displays omnibox icon only.
  void ShowIconOnly();

  // Should outlive this object.
  raw_ptr<PersonalDataManager> personal_data_manager_;

  // Observer for when a bubble is created. Initialized only during tests.
  raw_ptr<ObserverForTest> observer_for_testing_ = nullptr;

  // Note: Below fields are set when IBAN save is offered.
  //
  // Is true only if the [IBAN saved] label animation should be shown.
  bool should_show_iban_saved_label_animation_ = false;

  // The type of bubble that is either currently being shown or would
  // be shown when the IBAN save icon is clicked.
  IbanBubbleType current_bubble_type_ = IbanBubbleType::kInactive;

  // Callback to run once the user makes a decision with respect to the local
  // IBAN offer-to-save prompt.
  AutofillClient::LocalSaveIBANPromptCallback local_save_iban_prompt_callback_;

  // Whether the bubble is shown after user interacted with the omnibox icon.
  bool is_reshow_ = false;

  // Contains the details of the IBAN that will be saved if the user accepts.
  IBAN iban_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_IBAN_BUBBLE_CONTROLLER_IMPL_H_
