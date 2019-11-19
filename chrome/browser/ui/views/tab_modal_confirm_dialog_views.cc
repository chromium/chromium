// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_modal_confirm_dialog_views.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/chrome_switches.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

// static
TabModalConfirmDialog* TabModalConfirmDialog::Create(
    std::unique_ptr<TabModalConfirmDialogDelegate> delegate,
    content::WebContents* web_contents) {
  return new TabModalConfirmDialogViews(std::move(delegate), web_contents);
}

//////////////////////////////////////////////////////////////////////////////
// TabModalConfirmDialogViews, constructor & destructor:

TabModalConfirmDialogViews::TabModalConfirmDialogViews(
    std::unique_ptr<TabModalConfirmDialogDelegate> delegate,
    content::WebContents* web_contents)
    : delegate_(std::move(delegate)) {
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                   delegate_->GetAcceptButtonTitle());
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   delegate_->GetCancelButtonTitle());

  base::Optional<int> default_button = delegate_->GetDefaultDialogButton();
  if (bool(default_button))
    DialogDelegate::set_default_button(*default_button);

  views::MessageBoxView::InitParams init_params(delegate_->GetDialogMessage());
  init_params.inter_row_vertical_spacing =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  message_box_view_ = new views::MessageBoxView(init_params);

  base::string16 link_text(delegate_->GetLinkText());
  if (!link_text.empty())
    message_box_view_->SetLink(link_text, this);

  constrained_window::ShowWebModalDialogViews(this, web_contents);
  delegate_->set_close_delegate(this);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::TAB_MODAL_CONFIRM);
}

TabModalConfirmDialogViews::~TabModalConfirmDialogViews() {
}

void TabModalConfirmDialogViews::AcceptTabModalDialog() {
  GetDialogClientView()->AcceptWindow();
}

void TabModalConfirmDialogViews::CancelTabModalDialog() {
  GetDialogClientView()->CancelWindow();
}

void TabModalConfirmDialogViews::CloseDialog() {
  GetWidget()->Close();
}

//////////////////////////////////////////////////////////////////////////////
// TabModalConfirmDialogViews, views::LinkListener implementation:

void TabModalConfirmDialogViews::LinkClicked(views::Link* source,
                                             int event_flags) {
  delegate_->LinkClicked(ui::DispositionFromEventFlags(event_flags));
}

//////////////////////////////////////////////////////////////////////////////
// TabModalConfirmDialogViews, views::DialogDelegate implementation:

int TabModalConfirmDialogViews::GetDialogButtons() const {
  return delegate_->GetDialogButtons();
}

base::string16 TabModalConfirmDialogViews::GetWindowTitle() const {
  return delegate_->GetTitle();
}

bool TabModalConfirmDialogViews::Cancel() {
  delegate_->Cancel();
  return true;
}

bool TabModalConfirmDialogViews::Accept() {
  delegate_->Accept();
  return true;
}

bool TabModalConfirmDialogViews::Close() {
  delegate_->Close();
  return true;
}

// Tab-modal confirmation dialogs should not show an "X" close button in the top
// right corner. They should only have yes/no buttons.
bool TabModalConfirmDialogViews::ShouldShowCloseButton() const {
  return false;
}

views::View* TabModalConfirmDialogViews::GetInitiallyFocusedView() {
  base::Optional<int> focused_button = delegate_->GetInitiallyFocusedButton();
  if (!focused_button) {
    return DialogDelegate::GetInitiallyFocusedView();
  }

  const views::DialogClientView* dialog_client_view = GetDialogClientView();
  if (!dialog_client_view)
    return nullptr;
  if (*focused_button == ui::DIALOG_BUTTON_OK)
    return dialog_client_view->ok_button();
  if (*focused_button == ui::DIALOG_BUTTON_CANCEL)
    return dialog_client_view->cancel_button();
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// TabModalConfirmDialogViews, views::WidgetDelegate implementation:

views::View* TabModalConfirmDialogViews::GetContentsView() {
  return message_box_view_;
}

views::Widget* TabModalConfirmDialogViews::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* TabModalConfirmDialogViews::GetWidget() const {
  return message_box_view_->GetWidget();
}

void TabModalConfirmDialogViews::DeleteDelegate() {
  delete this;
}

ui::ModalType TabModalConfirmDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}
