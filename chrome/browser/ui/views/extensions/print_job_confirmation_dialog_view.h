// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PRINT_JOB_CONFIRMATION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PRINT_JOB_CONFIRMATION_DIALOG_VIEW_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

class ToolbarActionView;

// The dialog's view, owned by the views framework.
class PrintJobConfirmationDialogView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(PrintJobConfirmationDialogView);

  static void Show(gfx::NativeWindow parent,
                   const std::string& extension_id,
                   const std::u16string& extension_name,
                   const gfx::ImageSkia& extension_icon,
                   const std::u16string& print_job_title,
                   const std::u16string& printer_name,
                   base::OnceCallback<void(bool)> callback);

  PrintJobConfirmationDialogView(ToolbarActionView* anchor_view,
                                 const std::u16string& extension_name,
                                 const gfx::ImageSkia& extension_icon,
                                 const std::u16string& print_job_title,
                                 const std::u16string& printer_name,
                                 base::OnceCallback<void(bool)> callback);
  PrintJobConfirmationDialogView(const PrintJobConfirmationDialogView&) =
      delete;
  PrintJobConfirmationDialogView& operator=(
      const PrintJobConfirmationDialogView&) = delete;
  ~PrintJobConfirmationDialogView() override;

 private:
  // The name of the extension we are showing the dialog for.
  const std::u16string extension_name_;

  // Callback to call after the dialog is accepted or rejected.
  base::OnceCallback<void(bool)> callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PRINT_JOB_CONFIRMATION_DIALOG_VIEW_H_
