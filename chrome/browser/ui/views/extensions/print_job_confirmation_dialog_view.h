// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PRINT_JOB_CONFIRMATION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PRINT_JOB_CONFIRMATION_DIALOG_VIEW_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class ToolbarActionView;

// The dialog's view, owned by the views framework.
class PrintJobConfirmationDialogView : public views::BubbleDialogDelegateView {
 public:
  static void Show(gfx::NativeWindow parent,
                   const std::string& extension_id,
                   const base::string16& extension_name,
                   const gfx::ImageSkia& extension_icon,
                   const base::string16& print_job_title,
                   const base::string16& printer_name,
                   base::OnceCallback<void(bool)> callback);

  PrintJobConfirmationDialogView(ToolbarActionView* anchor_view,
                                 const base::string16& extension_name,
                                 const gfx::ImageSkia& extension_icon,
                                 const base::string16& print_job_title,
                                 const base::string16& printer_name,
                                 base::OnceCallback<void(bool)> callback);

  ~PrintJobConfirmationDialogView() override;

  PrintJobConfirmationDialogView(const PrintJobConfirmationDialogView&) =
      delete;
  PrintJobConfirmationDialogView& operator=(
      const PrintJobConfirmationDialogView&) = delete;

 private:
  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;

  // The name of the extension we are showing the dialog for.
  const base::string16 extension_name_;

  // Callback to call after the dialog is accepted or rejected.
  base::OnceCallback<void(bool)> callback_;

  // TODO(pbos): Find a more direct way of determining if there's a bubble than
  // checking |anchor_view|.
  const bool dialog_is_bubble_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PRINT_JOB_CONFIRMATION_DIALOG_VIEW_H_
