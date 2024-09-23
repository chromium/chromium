// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/print_preview_cros/print_preview_cros_dialog.h"

#include <string>

#include "ash/webui/print_preview_cros/url_constants.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "content/public/browser/web_ui.h"

namespace ash::printing::print_preview {

namespace {

// TODO(jimmyxgong): localize title.
constexpr char16_t kDialogTitle[] = u"PrintPreviewCros";

}  // namespace

// static:
PrintPreviewCrosDialog* PrintPreviewCrosDialog::ShowDialog(
    base::UnguessableToken token) {
  SystemWebDialogDelegate* existing_dialog =
      SystemWebDialogDelegate::FindInstance(token.ToString());
  if (existing_dialog) {
    existing_dialog->Focus();
    return (PrintPreviewCrosDialog*)existing_dialog;
  }

  // Closed by `SystemWebDialogDelegate::OnDialogClosed`.
  PrintPreviewCrosDialog* dialog = new PrintPreviewCrosDialog(token);

  // Attach dialog to parent window and show.
  dialog->ShowSystemDialog();

  return dialog;
}

PrintPreviewCrosDialog::PrintPreviewCrosDialog(base::UnguessableToken token)
    : SystemWebDialogDelegate(GURL(ash::kChromeUIPrintPreviewCrosURL),
                              kDialogTitle),
      dialog_id_(token) {
  set_dialog_args(token.ToString());
}

PrintPreviewCrosDialog::~PrintPreviewCrosDialog() = default;

void PrintPreviewCrosDialog::AddObserver(
    PrintPreviewCrosDialogObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PrintPreviewCrosDialog::RemoveObserver(
    PrintPreviewCrosDialogObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void PrintPreviewCrosDialog::OnDialogShown(content::WebUI* webui) {
  // TODO(jimmyxgong): Call on crosapi to establish bindings.
  return SystemWebDialogDelegate::OnDialogShown(webui);
}

void PrintPreviewCrosDialog::OnDialogClosed(const std::string& json_retval) {
  for (auto& observer : observer_list_) {
    observer.OnDialogClosed(dialog_id_);
  }

  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

gfx::NativeWindow PrintPreviewCrosDialog::GetDialogWindowForTesting() {
  return dialog_window();
}

std::string PrintPreviewCrosDialog::Id() {
  return dialog_id_.ToString();
}

}  // namespace ash::printing::print_preview
