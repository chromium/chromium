// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_BLOCKED_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_BLOCKED_DIALOG_VIEW_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

// Modal dialog that shows when the user attempts to install an extension but
// blocked by policy.
class ExtensionInstallBlockedDialogView
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(ExtensionInstallBlockedDialogView);
  ExtensionInstallBlockedDialogView(const std::string& extension_name,
                                    const std::u16string& custom_error_message,
                                    const gfx::ImageSkia& icon,
                                    base::OnceClosure done_callback);
  ExtensionInstallBlockedDialogView(const ExtensionInstallBlockedDialogView&) =
      delete;
  ExtensionInstallBlockedDialogView operator=(
      const ExtensionInstallBlockedDialogView&) = delete;
  ~ExtensionInstallBlockedDialogView() override;

 private:
  // Creates the contents area that contains custom error message that is set by
  // administrator.
  void AddCustomMessageContents(const std::u16string& custom_error_message);

  base::OnceClosure done_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALL_BLOCKED_DIALOG_VIEW_H_
