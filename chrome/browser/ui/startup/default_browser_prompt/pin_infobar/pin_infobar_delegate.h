// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_DELEGATE_H_

#include <string>

#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
class InfoBar;
}  // namespace infobars

namespace default_browser {

// Potential user interactions with the pin-to-taskbar infobar.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Exposed for testing.
enum class PinInfoBarUserInteraction {
  kAccepted = 0,
  kDismissed = 1,
  kIgnored = 2,
  kMaxValue = kIgnored,
};

// The pin-to-taskbar infobar offers to pin Chrome to the taskbar. This class
// customizes its appearance and behavior.
class PinInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  ~PinInfoBarDelegate() override;

  // Creates a `PinInfoBarDelegate` instance and adds it to
  // `infobar_manager`.
  static infobars::InfoBar* Create(
      infobars::ContentInfoBarManager* infobar_manager);

  // InfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

  // ConfirmInfoBarDelegate:
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  int GetButtons() const override;
  bool Accept() override;
  void InfoBarDismissed() override;

 private:
  // Indicates whether the user interacted with the infobar, in order to detect
  // if the infobar was ignored.
  bool action_taken_ = false;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_DELEGATE_H_
