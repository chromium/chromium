// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COOKIE_CONTROLS_ROLL_BACK_MODE_B_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_COOKIE_CONTROLS_ROLL_BACK_MODE_B_INFOBAR_DELEGATE_H_

#include "chrome/browser/privacy_sandbox/roll_back_3pcd_notice_action.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"

namespace infobars {
class ContentInfoBarManager;
}

// Rollback infobar notifying users that they are no longer in Mode B.
class RollBackModeBInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  RollBackModeBInfoBarDelegate(const RollBackModeBInfoBarDelegate&) = delete;
  RollBackModeBInfoBarDelegate& operator=(const RollBackModeBInfoBarDelegate&) =
      delete;

  // Creates a `RollBackModeBInfoBarDelegate` and adds it to `infobar_manager`.
  // Returns a pointer to the infobar if it was successfully added (or nullptr
  // if not).
  static infobars::InfoBar* Create(
      infobars::ContentInfoBarManager* infobar_manager);

 private:
  RollBackModeBInfoBarDelegate();
  ~RollBackModeBInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  bool Cancel() override;
  bool Accept() override;
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  // InfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool IsCloseable() const override;

  bool user_action_ = false;
};

#endif  // CHROME_BROWSER_UI_COOKIE_CONTROLS_ROLL_BACK_MODE_B_INFOBAR_DELEGATE_H_
