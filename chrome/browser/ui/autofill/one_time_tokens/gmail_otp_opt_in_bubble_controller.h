// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_BUBBLE_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "ui/actions/action_id.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace autofill {

class AutofillBubbleBase;
enum class GmailOtpOptInResult;

// Controller for the bubble asking the user to opt in to fetching one-time
// verification codes from Gmail.
//
// Owned by `tabs::TabFeatures` (one instance per tab) and retrieved via
// `GmailOtpOptInBubbleController::From(tab_interface)`.
class GmailOtpOptInBubbleController : public AutofillBubbleControllerBase {
 public:
  DECLARE_USER_DATA(GmailOtpOptInBubbleController);

  using ResultCallback = base::OnceCallback<void(GmailOtpOptInResult)>;
  using BubbleViewFactory = base::RepeatingCallback<AutofillBubbleBase*(
      content::WebContents* web_contents,
      GmailOtpOptInBubbleController* controller)>;

  GmailOtpOptInBubbleController(tabs::TabInterface& tab,
                                content::WebContents* web_contents);
  GmailOtpOptInBubbleController(const GmailOtpOptInBubbleController&) = delete;
  GmailOtpOptInBubbleController& operator=(
      const GmailOtpOptInBubbleController&) = delete;
  ~GmailOtpOptInBubbleController() override;

  static GmailOtpOptInBubbleController* From(tabs::TabInterface& tab);

  // Configures the bubble with `account_email` and `callback`, and requests
  // `BubbleManager` to show or queue it with `force_show = false`.
  void SetUpAndShowBubble(const std::u16string& account_email,
                          ResultCallback callback);

  // Called by the bubble view when its widget is closing.
  void OnBubbleClosed(GmailOtpOptInResult result);

  const std::u16string& account_email() const { return account_email_; }
  tabs::TabInterface& tab() const { return *tab_; }
  using AutofillBubbleControllerBase::bubble_view;

  // Overrides the factory used to create and show the bubble view in tests so
  // the controller can be unit-tested without instantiating `views` widgets.
  void SetBubbleViewFactoryForTesting(BubbleViewFactory factory);
  void SetAccountEmailForTesting(const std::u16string& account_email);

  base::WeakPtr<GmailOtpOptInBubbleController> GetWeakPtr();

  // AutofillBubbleControllerBase:
  void HideBubble(bool initiated_by_bubble_manager) override;
  void OnBubbleDiscarded() override;
  BubbleType GetBubbleType() const override;
  bool ShouldReshowOnTabVisible() const override;
  base::WeakPtr<BubbleControllerBase> GetBubbleControllerBaseWeakPtr() override;

 protected:
  // AutofillBubbleControllerBase:
  void DoShowBubble() override;
  std::optional<actions::ActionId> GetActionIdForPageAction() override;

 private:
  const raw_ref<tabs::TabInterface> tab_;

  ui::ScopedUnownedUserData<GmailOtpOptInBubbleController>
      scoped_unowned_user_data_;

  std::u16string account_email_;
  ResultCallback callback_;
  BubbleViewFactory bubble_view_factory_;

  base::WeakPtrFactory<GmailOtpOptInBubbleController> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_BUBBLE_CONTROLLER_H_
