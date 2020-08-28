// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_permission_view.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/native_file_system/native_file_system_ui_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/permissions/permission_util.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/native_file_system_permission_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {
using AccessType = NativeFileSystemPermissionRequestManager::Access;
using HandleType = content::NativeFileSystemPermissionContext::HandleType;

int GetMessageText(const NativeFileSystemPermissionView::Request& request) {
  switch (request.access) {
    case AccessType::kRead:
      return request.handle_type == HandleType::kDirectory
                 ? IDS_NATIVE_FILE_SYSTEM_ORIGIN_SCOPED_READ_PERMISSION_DIRECTORY_TEXT
                 : IDS_NATIVE_FILE_SYSTEM_ORIGIN_SCOPED_READ_PERMISSION_FILE_TEXT;
    case AccessType::kWrite:
    case AccessType::kReadWrite:
      // Only difference between write and read-write access dialog is in button
      // label and dialog title.
      return request.handle_type == HandleType::kDirectory
                 ? IDS_NATIVE_FILE_SYSTEM_ORIGIN_SCOPED_WRITE_PERMISSION_DIRECTORY_TEXT
                 : IDS_NATIVE_FILE_SYSTEM_ORIGIN_SCOPED_WRITE_PERMISSION_FILE_TEXT;
  }
  NOTREACHED();
}

int GetButtonLabel(const NativeFileSystemPermissionView::Request& request) {
  switch (request.access) {
    case AccessType::kRead:
      return request.handle_type == HandleType::kDirectory
                 ? IDS_NATIVE_FILE_SYSTEM_VIEW_DIRECTORY_PERMISSION_ALLOW_TEXT
                 : IDS_NATIVE_FILE_SYSTEM_VIEW_FILE_PERMISSION_ALLOW_TEXT;
    case AccessType::kWrite:
      return IDS_NATIVE_FILE_SYSTEM_WRITE_PERMISSION_ALLOW_TEXT;
    case AccessType::kReadWrite:
      return request.handle_type == HandleType::kDirectory
                 ? IDS_NATIVE_FILE_SYSTEM_EDIT_DIRECTORY_PERMISSION_ALLOW_TEXT
                 : IDS_NATIVE_FILE_SYSTEM_EDIT_FILE_PERMISSION_ALLOW_TEXT;
  }
  NOTREACHED();
}

}  // namespace

NativeFileSystemPermissionView::NativeFileSystemPermissionView(
    const Request& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback)
    : request_(request), callback_(std::move(callback)) {
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(GetButtonLabel(request_)));

  auto run_callback = [](NativeFileSystemPermissionView* dialog,
                         permissions::PermissionAction result) {
    std::move(dialog->callback_).Run(result);
  };
  SetAcceptCallback(base::BindOnce(run_callback, base::Unretained(this),
                                   permissions::PermissionAction::GRANTED));
  SetCancelCallback(base::BindOnce(run_callback, base::Unretained(this),
                                   permissions::PermissionAction::DISMISSED));
  SetCloseCallback(base::BindOnce(run_callback, base::Unretained(this),
                                  permissions::PermissionAction::DISMISSED));

  const views::LayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  AddChildView(native_file_system_ui_helper::CreateOriginPathLabel(
      GetMessageText(request_), request_.origin, request_.path,
      CONTEXT_DIALOG_BODY_TEXT_SMALL,
      /*show_emphasis=*/true));
}

NativeFileSystemPermissionView::~NativeFileSystemPermissionView() {
  // Make sure the dialog ends up calling the callback no matter what.
  if (!callback_.is_null())
    Close();
}

views::Widget* NativeFileSystemPermissionView::ShowDialog(
    const Request& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents) {
  auto delegate = base::WrapUnique(
      new NativeFileSystemPermissionView(request, std::move(callback)));
  return constrained_window::ShowWebModalDialogViews(delegate.release(),
                                                     web_contents);
}

base::string16 NativeFileSystemPermissionView::GetWindowTitle() const {
  switch (request_.access) {
    case AccessType::kRead:
      if (request_.handle_type == HandleType::kDirectory) {
        return l10n_util::GetStringUTF16(
            IDS_NATIVE_FILE_SYSTEM_READ_DIRECTORY_PERMISSION_TITLE);
      } else {
        return l10n_util::GetStringFUTF16(
            IDS_NATIVE_FILE_SYSTEM_READ_FILE_PERMISSION_TITLE,
            request_.path.BaseName().LossyDisplayName());
      }
    case AccessType::kWrite:
      return l10n_util::GetStringFUTF16(
          IDS_NATIVE_FILE_SYSTEM_WRITE_PERMISSION_TITLE,
          request_.path.BaseName().LossyDisplayName());
    case AccessType::kReadWrite:
      if (request_.handle_type == HandleType::kDirectory) {
        return l10n_util::GetStringUTF16(
            IDS_NATIVE_FILE_SYSTEM_EDIT_DIRECTORY_PERMISSION_TITLE);
      } else {
        return l10n_util::GetStringFUTF16(
            IDS_NATIVE_FILE_SYSTEM_EDIT_FILE_PERMISSION_TITLE,
            request_.path.BaseName().LossyDisplayName());
      }
  }
  NOTREACHED();
}

bool NativeFileSystemPermissionView::ShouldShowCloseButton() const {
  return false;
}

gfx::Size NativeFileSystemPermissionView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType NativeFileSystemPermissionView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

views::View* NativeFileSystemPermissionView::GetInitiallyFocusedView() {
  return GetCancelButton();
}

void ShowNativeFileSystemPermissionDialog(
    const NativeFileSystemPermissionView::Request& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents) {
  NativeFileSystemPermissionView::ShowDialog(request, std::move(callback),
                                             web_contents);
}
