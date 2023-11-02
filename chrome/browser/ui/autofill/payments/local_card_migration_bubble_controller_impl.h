// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_CONTROLLER_IMPL_H_

#include "base/observer_list.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_controller_observer.h"
#include "components/autofill/core/browser/ui/payments/local_card_migration_bubble_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Implementation of per-tab class to control the local card migration bubble
// and Omnibox icon.
class LocalCardMigrationBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public LocalCardMigrationBubbleController,
      public content::WebContentsUserData<
          LocalCardMigrationBubbleControllerImpl> {
 public:
  LocalCardMigrationBubbleControllerImpl(
      const LocalCardMigrationBubbleControllerImpl&) = delete;
  LocalCardMigrationBubbleControllerImpl& operator=(
      const LocalCardMigrationBubbleControllerImpl&) = delete;
  ~LocalCardMigrationBubbleControllerImpl() override;

  // Shows the prompt that offers local credit card migration.
  // |local_card_migration_bubble_closure| is run upon acceptance.
  void ShowBubble(base::OnceClosure local_card_migration_bubble_closure);

  // Invoked when local card migration icon is clicked.
  void ReshowBubble();

  void AddObserver(LocalCardMigrationControllerObserver* observer);

  // Returns nullptr if no bubble is currently shown.
  AutofillBubbleBase* local_card_migration_bubble_view() const;

  // LocalCardMigrationBubbleController:
  void OnConfirmButtonClicked() override;
  void OnCancelButtonClicked() override;
  void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) override;

 protected:
  explicit LocalCardMigrationBubbleControllerImpl(
      content::WebContents* web_contents);

  // AutofillBubbleControllerBase::
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<
      LocalCardMigrationBubbleControllerImpl>;

  friend class LocalCardMigrationBrowserTest;

  void UpdateLocalCardMigrationIcon();

  // Add strikes for local card migration, to be called on user closing the
  // promo bubble.
  void AddStrikesForBubbleClose();

  // Callback to run if user presses Save button in the offer-to-migrate bubble.
  base::OnceClosure local_card_migration_bubble_closure_;

  // Boolean to determine if bubble is called from ReshowBubble().
  bool is_reshow_ = false;

  // Boolean to determine if strikes should be added when bubble is closed. They
  // should be added only once and only if the bubble isn't closed due to
  // clicking the Continue button.
  bool should_add_strikes_on_bubble_close_ = true;

  base::ObserverList<LocalCardMigrationControllerObserver>::Unchecked
      observer_list_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_CONTROLLER_IMPL_H_
