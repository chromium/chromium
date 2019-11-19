// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/chooser_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/extensions/chrome_extension_chooser_dialog.h"
#include "chrome/browser/extensions/device_permissions_dialog_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/device_chooser_content_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

ChooserDialogView::ChooserDialogView(
    std::unique_ptr<ChooserController> chooser_controller) {
  // ------------------------------------
  // | Chooser dialog title             |
  // | -------------------------------- |
  // | | option 0                     | |
  // | | option 1                     | |
  // | | option 2                     | |
  // | |                              | |
  // | |                              | |
  // | |                              | |
  // | -------------------------------- |
  // |           [ Connect ] [ Cancel ] |
  // |----------------------------------|
  // | Get help                         |
  // ------------------------------------

  DCHECK(chooser_controller);

  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                   chooser_controller->GetOkButtonLabel());
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   chooser_controller->GetCancelButtonLabel());

  device_chooser_content_view_ =
      new DeviceChooserContentView(this, std::move(chooser_controller));
  device_chooser_content_view_->SetBorder(views::CreateEmptyBorder(
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::CONTROL, views::CONTROL)));

  DialogDelegate::SetExtraView(device_chooser_content_view_->CreateExtraView());

  chrome::RecordDialogCreation(chrome::DialogIdentifier::CHOOSER);
}

ChooserDialogView::~ChooserDialogView() {}

base::string16 ChooserDialogView::GetWindowTitle() const {
  return device_chooser_content_view_->GetWindowTitle();
}

bool ChooserDialogView::ShouldShowCloseButton() const {
  return false;
}

ui::ModalType ChooserDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool ChooserDialogView::IsDialogButtonEnabled(ui::DialogButton button) const {
  return device_chooser_content_view_->IsDialogButtonEnabled(button);
}

views::View* ChooserDialogView::GetInitiallyFocusedView() {
  return GetCancelButton();
}

bool ChooserDialogView::Accept() {
  device_chooser_content_view_->Accept();
  return true;
}

bool ChooserDialogView::Cancel() {
  device_chooser_content_view_->Cancel();
  return true;
}

bool ChooserDialogView::Close() {
  device_chooser_content_view_->Close();
  return true;
}

views::View* ChooserDialogView::GetContentsView() {
  return device_chooser_content_view_;
}

views::Widget* ChooserDialogView::GetWidget() {
  return device_chooser_content_view_->GetWidget();
}

const views::Widget* ChooserDialogView::GetWidget() const {
  return device_chooser_content_view_->GetWidget();
}

void ChooserDialogView::OnSelectionChanged() {
  DialogModelChanged();
}

DeviceChooserContentView*
ChooserDialogView::device_chooser_content_view_for_test() const {
  return device_chooser_content_view_;
}

void ChromeExtensionChooserDialog::ShowDialogImpl(
    std::unique_ptr<ChooserController> chooser_controller) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(chooser_controller);

  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_);
  if (manager) {
    constrained_window::ShowWebModalDialogViews(
        new ChooserDialogView(std::move(chooser_controller)), web_contents_);
  }
}

void ChromeDevicePermissionsPrompt::ShowDialogViews() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<ChooserController> chooser_controller(
      new DevicePermissionsDialogController(web_contents()->GetMainFrame(),
                                            prompt()));

  constrained_window::ShowWebModalDialogViews(
      new ChooserDialogView(std::move(chooser_controller)), web_contents());
}
