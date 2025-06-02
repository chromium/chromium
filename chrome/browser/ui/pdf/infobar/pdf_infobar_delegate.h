// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_INFOBAR_PDF_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_PDF_INFOBAR_PDF_INFOBAR_DELEGATE_H_

#include <string>

#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
class InfoBar;
}  // namespace infobars

namespace pdf::infobar {

// Potential user interactions with the PDF infobar.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Exposed for testing.
enum class PdfInfoBarUserInteraction {
  kAccepted = 0,
  kDismissed = 1,
  kIgnored = 2,
  kMaxValue = kIgnored,
};

// The PDF infobar offers to set Chrome as the default PDF viewer. This class
// customizes its appearance and behavior.
class PdfInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  ~PdfInfoBarDelegate() override;

  // Creates a `PdfInfoBarDelegate` instance and adds it to `infobar_manager`.
  static infobars::InfoBar* Create(
      infobars::ContentInfoBarManager* infobar_manager);

  // InfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

  // ConfirmInfoBarDelegate:
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  void InfoBarDismissed() override;

 private:
  // Indicates whether the user interacted with the infobar, in order to detect
  // when the infobar was ignored.
  bool action_taken_ = false;
};

}  // namespace pdf::infobar

#endif  // CHROME_BROWSER_UI_PDF_INFOBAR_PDF_INFOBAR_DELEGATE_H_
