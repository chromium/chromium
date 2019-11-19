// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_REQUEST_FILE_SYSTEM_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_REQUEST_FILE_SYSTEM_DIALOG_VIEW_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

// Represents a dialog shown to a user for granting access to a file system.
class RequestFileSystemDialogView : public views::DialogDelegateView {
 public:
  ~RequestFileSystemDialogView() override;

  // Shows the dialog and calls |callback| on completion.
  static void ShowDialog(
      content::WebContents* web_contents,
      const std::string& extension_name,
      const std::string& volume_label,
      bool writable,
      const base::Callback<void(ui::DialogButton)>& callback);

  // views::DialogDelegate overrides:
  base::string16 GetAccessibleWindowTitle() const override;
  ui::ModalType GetModalType() const override;
  bool Cancel() override;
  bool Accept() override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  RequestFileSystemDialogView(
      const std::string& extension_name,
      const std::string& volume_label,
      bool writable,
      const base::Callback<void(ui::DialogButton)>& callback);

  const base::Callback<void(ui::DialogButton)> callback_;

  DISALLOW_COPY_AND_ASSIGN(RequestFileSystemDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_REQUEST_FILE_SYSTEM_DIALOG_VIEW_H_
