// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JS_OPTIMIZATION_JS_OPTIMIZATIONS_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_JS_OPTIMIZATION_JS_OPTIMIZATIONS_INFOBAR_DELEGATE_H_

#include <string>

#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
}

// This class configures an infobar shown when Javascript optimizations are
// disabled for a site and the user chooses to re-enable them. The user is shown
// a message indicating that a reload of the page is required for the changes to
// take effect, and presented a button to perform the reload right from the
// infobar.
class JsOptimizationsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  JsOptimizationsInfoBarDelegate();
  ~JsOptimizationsInfoBarDelegate() override;

  // Creates a confirmation infobar and delegate and adds the infobar to
  // `infobar_manager`.
  static void Create(infobars::ContentInfoBarManager* infobar_manager);

  JsOptimizationsInfoBarDelegate(const JsOptimizationsInfoBarDelegate&) =
      delete;
  JsOptimizationsInfoBarDelegate& operator=(
      const JsOptimizationsInfoBarDelegate&) = delete;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;

 private:
  // ConfirmInfoBarDelegate:
  const gfx::VectorIcon& GetVectorIcon() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_JS_OPTIMIZATION_JS_OPTIMIZATIONS_INFOBAR_DELEGATE_H_
