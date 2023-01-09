// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/chooser_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/extensions/chrome_extension_chooser_dialog.h"
#include "chrome/browser/extensions/device_permissions_dialog_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/device_chooser_content_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/permissions/chooser_controller.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

ChooserDialogView::ChooserDialogView(
    std::unique_ptr<permissions::ChooserController> chooser_controller) {
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

  SetButtonLabel(ui::DIALOG_BUTTON_OK, chooser_controller->GetOkButtonLabel());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 chooser_controller->GetCancelButtonLabel());

  device_chooser_content_view_ =
      new DeviceChooserContentView(this, std::move(chooser_controller));
  device_chooser_content_view_->SetBorder(views::CreateEmptyBorder(
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kControl,
          views::DialogContentType::kControl)));

  SetExtraView(device_chooser_content_view_->CreateExtraView());
  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  SetTitle(device_chooser_content_view_->GetWindowTitle());

  SetAcceptCallback(
      base::BindOnce(&DeviceChooserContentView::Accept,
                     base::Unretained(device_chooser_content_view_)));
  SetCancelCallback(
      base::BindOnce(&DeviceChooserContentView::Cancel,
                     base::Unretained(device_chooser_content_view_)));
  SetCloseCallback(
      base::BindOnce(&DeviceChooserContentView::Close,
                     base::Unretained(device_chooser_content_view_)));
}

ChooserDialogView::~ChooserDialogView() = default;

bool ChooserDialogView::IsDialogButtonEnabled(ui::DialogButton button) const {
  return device_chooser_content_view_->IsDialogButtonEnabled(button);
}

views::View* ChooserDialogView::GetInitiallyFocusedView() {
  return GetCancelButton();
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

BEGIN_METADATA(ChooserDialogView, views::DialogDelegateView)
END_METADATA

void ShowConstrainedDeviceChooserDialog(
    content::WebContents* web_contents,
    std::unique_ptr<permissions::ChooserController> controller) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(controller);

  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  if (manager) {
    constrained_window::ShowWebModalDialogViews(
        new ChooserDialogView(std::move(controller)), web_contents);
  }
}

void ChromeDevicePermissionsPrompt::ShowDialogViews() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<permissions::ChooserController> chooser_controller(
      new DevicePermissionsDialogController(
          web_contents()->GetPrimaryMainFrame(), prompt()));

  constrained_window::ShowWebModalDialogViews(
      new ChooserDialogView(std::move(chooser_controller)), web_contents());
}
