// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_FRICTION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_FRICTION_DIALOG_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace content {
class WebContents;
}

namespace views {
class StyledLabel;
}

class Profile;

// Modal dialog shown to Enhanced Safe Browsing users before the extension
// install dialog if the extension is not included in the Safe Browsing CRX
// allowlist.
class ExtensionInstallFrictionDialogView
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(ExtensionInstallFrictionDialogView,
                  views::BubbleDialogDelegateView)

 public:
  // `web_contents` ownership is not passed, `callback` will be invoked with
  // `true` if the user accepts or `false` if the user cancels.
  ExtensionInstallFrictionDialogView(content::WebContents* web_contents,
                                     base::OnceCallback<void(bool)> callback);
  ~ExtensionInstallFrictionDialogView() override;
  ExtensionInstallFrictionDialogView(
      const ExtensionInstallFrictionDialogView&) = delete;
  ExtensionInstallFrictionDialogView& operator=(
      const ExtensionInstallFrictionDialogView&) = delete;

  ui::ImageModel GetWindowIcon() override;

  // Returns the parent web contents for the dialog. Returns nullptr if the web
  // contents have been destroyed.
  content::WebContents* parent_web_contents() {
    return parent_web_contents_.get();
  }

  void ClickLearnMoreLinkForTesting();

 private:
  std::unique_ptr<views::StyledLabel> CreateWarningLabel();
  void OnLearnMoreLinkClicked();

  raw_ptr<Profile> profile_ = nullptr;
  base::WeakPtr<content::WebContents> parent_web_contents_;
  base::OnceCallback<void(bool)> callback_;

  bool accepted_ = false;
  bool learn_more_clicked_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_FRICTION_DIALOG_VIEW_H_
