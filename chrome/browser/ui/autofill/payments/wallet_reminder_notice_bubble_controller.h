// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WALLET_REMINDER_NOTICE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WALLET_REMINDER_NOTICE_BUBBLE_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace autofill {

// Controller class that exposes functionality to Wallet reminder notice
// bubbles. Owned by TabFeatures.
class WalletReminderNoticeBubbleController
    : public AutofillBubbleControllerBase {
 public:
  DECLARE_USER_DATA(WalletReminderNoticeBubbleController);

  WalletReminderNoticeBubbleController(tabs::TabInterface& tab_interface,
                                       content::WebContents* web_contents);
  WalletReminderNoticeBubbleController(
      const WalletReminderNoticeBubbleController&) = delete;
  WalletReminderNoticeBubbleController& operator=(
      const WalletReminderNoticeBubbleController&) = delete;
  ~WalletReminderNoticeBubbleController() override;

  static WalletReminderNoticeBubbleController* From(
      tabs::TabInterface& tab_interface);

  // Shows the bubble when it first opens automatically (e.g. without a user
  // gesture).
  void Show(LegalMessageLines legal_message_lines);
  // Re-opens the bubble when the user clicks the page action icon (e.g. after
  // the bubble was dismissed by clicking outside the bubble).
  void ReshowBubble();
  std::u16string GetWindowTitle() const;
  const LegalMessageLines& GetLegalMessageLines() const;
  AutofillBubbleBase* GetBubbleView() const;
  base::WeakPtr<WalletReminderNoticeBubbleController> GetWeakPtr();

  // BubbleControllerBase:
  void OnBubbleDiscarded() override {}
  BubbleType GetBubbleType() const override;
  base::WeakPtr<BubbleControllerBase> GetBubbleControllerBaseWeakPtr() override;

 protected:
  void DoShowBubble() override;

 private:
  const raw_ref<tabs::TabInterface> tab_interface_;

  ui::ScopedUnownedUserData<WalletReminderNoticeBubbleController>
      scoped_unowned_user_data_;

  // Whether the bubble is shown as a re-show. When false, the bubble is shown
  // as an alert without stealing input focus from the webpage. Clicking the
  // bubble or its icon again sets this to true, which focuses the bubble.
  bool is_reshow_ = false;

  // The legal message lines with links displayed in the notice bubble.
  LegalMessageLines legal_message_lines_;

  base::WeakPtrFactory<WalletReminderNoticeBubbleController> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WALLET_REMINDER_NOTICE_BUBBLE_CONTROLLER_H_
