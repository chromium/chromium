// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_FRICTION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_FRICTION_DIALOG_VIEW_H_

#include <string>

#include "base/callback.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace content {
class PageNavigator;
}

namespace views {
class StyledLabel;
}

// Modal dialog shown to Enhanced Safe Browsing users before the extension
// install dialog if the extension is not included in the Safe Browsing CRX
// allowlist.
class ExtensionInstallFrictionDialogView
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(ExtensionInstallFrictionDialogView);

  // `navigator` is owned by the caller, `callback` will be invoked with `true
  // if the user accepts or `false` if the user cancels.
  ExtensionInstallFrictionDialogView(content::PageNavigator* navigator,
                                     base::OnceCallback<void(bool)> callback);
  ~ExtensionInstallFrictionDialogView() override;
  ExtensionInstallFrictionDialogView(
      const ExtensionInstallFrictionDialogView&) = delete;
  ExtensionInstallFrictionDialogView& operator=(
      const ExtensionInstallFrictionDialogView&) = delete;

  gfx::ImageSkia GetWindowIcon() override;

 private:
  std::unique_ptr<views::StyledLabel> CreateWarningLabel();
  void OnLearnMoreLinkClicked();

  content::PageNavigator* navigator_;
  base::OnceCallback<void(bool)> callback_;
  bool accepted_ = false;
  bool learn_more_clicked_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_FRICTION_DIALOG_VIEW_H_
