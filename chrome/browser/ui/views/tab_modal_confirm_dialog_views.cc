// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_modal_confirm_dialog_views.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/chrome_switches.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"

// static
TabModalConfirmDialog* TabModalConfirmDialog::Create(
    std::unique_ptr<TabModalConfirmDialogDelegate> delegate,
    content::WebContents* web_contents) {
  return new TabModalConfirmDialogViews(std::move(delegate), web_contents);
}

TabModalConfirmDialogViews::TabModalConfirmDialogViews(
    std::unique_ptr<TabModalConfirmDialogDelegate> delegate,
    content::WebContents* web_contents)
    : delegate_(std::move(delegate)) {
  SetButtons(delegate_->GetDialogButtons());
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 delegate_->GetAcceptButtonTitle());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 delegate_->GetCancelButtonTitle());

  SetAcceptCallback(base::BindOnce(&TabModalConfirmDialogDelegate::Accept,
                                   base::Unretained(delegate_.get())));
  SetCancelCallback(base::BindOnce(&TabModalConfirmDialogDelegate::Cancel,
                                   base::Unretained(delegate_.get())));
  SetCloseCallback(base::BindOnce(&TabModalConfirmDialogDelegate::Close,
                                  base::Unretained(delegate_.get())));
  SetModalType(ui::mojom::ModalType::kChild);
  SetOwnedByWidget(true);

  std::optional<int> default_button = delegate_->GetDefaultDialogButton();
  if (bool(default_button))
    SetDefaultButton(*default_button);

  message_box_view_ = new views::MessageBoxView(delegate_->GetDialogMessage());
  message_box_view_->SetInterRowVerticalSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  std::u16string link_text(delegate_->GetLinkText());
  if (!link_text.empty()) {
    message_box_view_->SetLink(
        link_text, base::BindRepeating(&TabModalConfirmDialogViews::LinkClicked,
                                       base::Unretained(this)));
  }

  constrained_window::ShowWebModalDialogViews(this, web_contents);
  delegate_->set_close_delegate(this);
}

std::u16string TabModalConfirmDialogViews::GetWindowTitle() const {
  return delegate_->GetTitle();
}

// Tab-modal confirmation dialogs should not show an "X" close button in the top
// right corner. They should only have yes/no buttons.
bool TabModalConfirmDialogViews::ShouldShowCloseButton() const {
  return false;
}

views::View* TabModalConfirmDialogViews::GetContentsView() {
  return message_box_view_;
}

views::Widget* TabModalConfirmDialogViews::GetWidget() {
  return message_box_view_ ? message_box_view_->GetWidget() : nullptr;
}

const views::Widget* TabModalConfirmDialogViews::GetWidget() const {
  return message_box_view_ ? message_box_view_->GetWidget() : nullptr;
}

TabModalConfirmDialogViews::~TabModalConfirmDialogViews() = default;

void TabModalConfirmDialogViews::AcceptTabModalDialog() {
  AcceptDialog();
}

void TabModalConfirmDialogViews::CancelTabModalDialog() {
  CancelDialog();
}

void TabModalConfirmDialogViews::CloseDialog() {
  GetWidget()->Close();
}

void TabModalConfirmDialogViews::LinkClicked(const ui::Event& event) {
  delegate_->LinkClicked(ui::DispositionFromEventFlags(event.flags()));
}

views::View* TabModalConfirmDialogViews::GetInitiallyFocusedView() {
  std::optional<int> focused_button = delegate_->GetInitiallyFocusedButton();
  if (!focused_button) {
    return DialogDelegate::GetInitiallyFocusedView();
  }

  if (*focused_button == static_cast<int>(ui::mojom::DialogButton::kOk)) {
    return GetOkButton();
  }
  if (*focused_button == static_cast<int>(ui::mojom::DialogButton::kCancel)) {
    return GetCancelButton();
  }
  return nullptr;
}
