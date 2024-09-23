// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/add_supervision/confirm_signout_dialog.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_handler_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

namespace {
// The width of the text body in the dialog.
const int kDialogBodyTextWidth = 250;
}  // namespace

ConfirmSignoutDialog::ConfirmSignoutDialog() {
  DialogDelegate::SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(
          IDS_ADD_SUPERVISION_EXIT_DIALOG_SIGNOUT_BUTTON_LABEL));
  DialogDelegate::SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(
          IDS_ADD_SUPERVISION_EXIT_DIALOG_CANCEL_BUTTON_LABEL));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText)));

  // |body| will be owned by the views system.
  views::Label* body = new views::Label;
  body->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  body->SetMultiLine(true);
  body->SetText(
      l10n_util::GetStringUTF16(IDS_ADD_SUPERVISION_EXIT_DIALOG_BODY));
  body->SizeToFit(kDialogBodyTextWidth);
  AddChildView(body);
}

ConfirmSignoutDialog::~ConfirmSignoutDialog() {
  ConfirmSignoutDialog::current_instance_ = nullptr;
}

ui::mojom::ModalType ConfirmSignoutDialog::GetModalType() const {
  return ui::mojom::ModalType::kSystem;
}

std::u16string ConfirmSignoutDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_ADD_SUPERVISION_EXIT_DIALOG_TITLE);
}

bool ConfirmSignoutDialog::Accept() {
  LogOutHelper();
  return true;
}

// static
views::Widget* ConfirmSignoutDialog::current_instance_ = nullptr;

// static
void ConfirmSignoutDialog::Show() {
  // Ownership of the ConfirmSignoutDialog is passed to the views system.
  // Dialog is system-modal, so no parent window is needed.
  ConfirmSignoutDialog::current_instance_ =
      constrained_window::CreateBrowserModalDialogViews(
          new ConfirmSignoutDialog(), nullptr /* parent window */);
  current_instance_->Show();
}

// static
bool ConfirmSignoutDialog::IsShowing() {
  return ConfirmSignoutDialog::current_instance_ != nullptr;
}

BEGIN_METADATA(ConfirmSignoutDialog)
END_METADATA

}  // namespace ash
