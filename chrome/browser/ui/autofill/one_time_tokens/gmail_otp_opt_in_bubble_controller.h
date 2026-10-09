// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_BUBBLE_CONTROLLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"
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
class GmailOtpOptInBubbleController : public BubbleControllerBase {
 public:
  DECLARE_USER_DATA(GmailOtpOptInBubbleController);

  using ResultCallback = base::OnceCallback<void(GmailOtpOptInResult)>;
  using BubbleViewFactory = base::RepeatingCallback<AutofillBubbleBase*(
      content::WebContents* web_contents,
      GmailOtpOptInBubbleController* controller)>;

  explicit GmailOtpOptInBubbleController(tabs::TabInterface& tab);
  GmailOtpOptInBubbleController(const GmailOtpOptInBubbleController&) = delete;
  GmailOtpOptInBubbleController& operator=(
      const GmailOtpOptInBubbleController&) = delete;
  ~GmailOtpOptInBubbleController() override;

  static GmailOtpOptInBubbleController* From(tabs::TabInterface& tab);

  // Configures the bubble with `account_email` and `callback`, and requests
  // `BubbleManager` to show or queue it with `force_show = false`.
  void SetUpAndShowBubble(const std::u16string& account_email,
                          ResultCallback callback);

  const std::u16string& account_email() const { return account_email_; }
  tabs::TabInterface& tab() const { return *tab_; }

  // Overrides the factory used to create and show the bubble view in tests so
  // the controller can be unit-tested without instantiating `views` widgets.
  void SetBubbleViewFactoryForTesting(BubbleViewFactory factory);

  // BubbleControllerBase:
  void ShowBubble() override;
  void HideBubble(bool initiated_by_bubble_manager) override;
  void OnBubbleDiscarded() override;
  BubbleType GetBubbleType() const override;
  bool IsShowingBubble() const override;
  bool IsMouseHovered() const override;
  bool CanBeReshown() const override;
  bool ShouldReshowOnTabVisible() const override;
  base::WeakPtr<BubbleControllerBase> GetBubbleControllerBaseWeakPtr() override;

 protected:
  void SetBubbleView(AutofillBubbleBase* bubble_view);
  void ResetBubbleViewAndInformBubbleManager(bool initiated_by_bubble_manager);
  content::WebContents* web_contents() const;

 private:
  const raw_ref<tabs::TabInterface> tab_;

  ui::ScopedUnownedUserData<GmailOtpOptInBubbleController>
      scoped_unowned_user_data_;

  // Weak reference to the active bubble view, or nullptr when hidden.
  raw_ptr<AutofillBubbleBase> bubble_view_ = nullptr;

  std::u16string account_email_;
  ResultCallback callback_;
  BubbleViewFactory bubble_view_factory_;

  base::WeakPtrFactory<GmailOtpOptInBubbleController> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_BUBBLE_CONTROLLER_H_
