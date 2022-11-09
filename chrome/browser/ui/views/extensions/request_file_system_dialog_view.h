// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_REQUEST_FILE_SYSTEM_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_REQUEST_FILE_SYSTEM_DIALOG_VIEW_H_

#include <string>

#include "base/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
  METADATA_HEADER(RequestFileSystemDialogView);
  RequestFileSystemDialogView(const RequestFileSystemDialogView&) = delete;
  RequestFileSystemDialogView& operator=(const RequestFileSystemDialogView&) =
      delete;
  ~RequestFileSystemDialogView() override;

  // Shows the dialog and calls |callback| on completion.
  static void ShowDialog(content::WebContents* web_contents,
                         const std::string& extension_name,
                         const std::string& volume_label,
                         bool writable,
                         base::OnceCallback<void(ui::DialogButton)> callback);

  // views::DialogDelegate overrides:
  gfx::Size CalculatePreferredSize() const override;

 private:
  RequestFileSystemDialogView(
      const std::string& extension_name,
      const std::string& volume_label,
      bool writable,
      base::OnceCallback<void(ui::DialogButton)> callback);

  base::OnceCallback<void(ui::DialogButton)> callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_REQUEST_FILE_SYSTEM_DIALOG_VIEW_H_
