// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_directory_access_confirmation_view.h"

#include "base/files/file_path.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/native_file_system/native_file_system_ui_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/window/dialog_client_view.h"

NativeFileSystemDirectoryAccessConfirmationView::
    ~NativeFileSystemDirectoryAccessConfirmationView() {
  // Make sure the dialog ends up calling the callback no matter what.
  if (!callback_.is_null())
    Close();
}

// static
views::Widget* NativeFileSystemDirectoryAccessConfirmationView::ShowDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<void(PermissionAction result)> callback,
    content::WebContents* web_contents) {
  auto delegate =
      base::WrapUnique(new NativeFileSystemDirectoryAccessConfirmationView(
          origin, path, std::move(callback)));
  return constrained_window::ShowWebModalDialogViews(delegate.release(),
                                                     web_contents);
}

base::string16 NativeFileSystemDirectoryAccessConfirmationView::GetWindowTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_NATIVE_FILE_SYSTEM_DIRECTORY_ACCESS_CONFIRMATION_TITLE);
}

bool NativeFileSystemDirectoryAccessConfirmationView::ShouldShowCloseButton()
    const {
  return false;
}

bool NativeFileSystemDirectoryAccessConfirmationView::Accept() {
  std::move(callback_).Run(PermissionAction::GRANTED);
  return true;
}

bool NativeFileSystemDirectoryAccessConfirmationView::Cancel() {
  std::move(callback_).Run(PermissionAction::DISMISSED);
  return true;
}

gfx::Size
NativeFileSystemDirectoryAccessConfirmationView::CalculatePreferredSize()
    const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType NativeFileSystemDirectoryAccessConfirmationView::GetModalType()
    const {
  return ui::MODAL_TYPE_CHILD;
}

views::View*
NativeFileSystemDirectoryAccessConfirmationView::GetInitiallyFocusedView() {
  const views::DialogClientView* dcv = GetDialogClientView();
  return dcv ? dcv->cancel_button() : nullptr;
}

NativeFileSystemDirectoryAccessConfirmationView::
    NativeFileSystemDirectoryAccessConfirmationView(
        const url::Origin& origin,
        const base::FilePath& path,
        base::OnceCallback<void(PermissionAction result)> callback)
    : callback_(std::move(callback)) {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(
          IDS_NATIVE_FILE_SYSTEM_DIRECTORY_ACCESS_ALLOW_BUTTON));
  const views::LayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  AddChildView(native_file_system_ui_helper::CreateOriginPathLabel(
      IDS_NATIVE_FILE_SYSTEM_DIRECTORY_ACCESS_CONFIRMATION_TEXT, origin, path,
      CONTEXT_BODY_TEXT_SMALL, /*show_emphasis=*/true));
}

void ShowNativeFileSystemDirectoryAccessConfirmationDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<void(PermissionAction result)> callback,
    content::WebContents* web_contents) {
  NativeFileSystemDirectoryAccessConfirmationView::ShowDialog(
      origin, path, std::move(callback), web_contents);
}
