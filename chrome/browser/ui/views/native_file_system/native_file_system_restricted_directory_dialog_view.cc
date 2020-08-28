// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_restricted_directory_dialog_view.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/native_file_system/native_file_system_ui_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

using HandleType = content::NativeFileSystemPermissionContext::HandleType;

NativeFileSystemRestrictedDirectoryDialogView::
    ~NativeFileSystemRestrictedDirectoryDialogView() {
  // Make sure the dialog ends up calling the callback no matter what.
  if (!callback_.is_null())
    Accept();
}

// static
views::Widget* NativeFileSystemRestrictedDirectoryDialogView::ShowDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    content::NativeFileSystemPermissionContext::HandleType handle_type,
    base::OnceCallback<void(SensitiveDirectoryResult)> callback,
    content::WebContents* web_contents) {
  auto delegate =
      base::WrapUnique(new NativeFileSystemRestrictedDirectoryDialogView(
          origin, path, handle_type, std::move(callback)));
  return constrained_window::ShowWebModalDialogViews(delegate.release(),
                                                     web_contents);
}

base::string16 NativeFileSystemRestrictedDirectoryDialogView::GetWindowTitle()
    const {
  return l10n_util::GetStringUTF16(
      handle_type_ == HandleType::kDirectory
          ? IDS_NATIVE_FILE_SYSTEM_RESTRICTED_DIRECTORY_TITLE
          : IDS_NATIVE_FILE_SYSTEM_RESTRICTED_FILE_TITLE);
}

bool NativeFileSystemRestrictedDirectoryDialogView::ShouldShowCloseButton()
    const {
  return false;
}

gfx::Size
NativeFileSystemRestrictedDirectoryDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType NativeFileSystemRestrictedDirectoryDialogView::GetModalType()
    const {
  return ui::MODAL_TYPE_CHILD;
}

NativeFileSystemRestrictedDirectoryDialogView::
    NativeFileSystemRestrictedDirectoryDialogView(
        const url::Origin& origin,
        const base::FilePath& path,
        content::NativeFileSystemPermissionContext::HandleType handle_type,
        base::OnceCallback<void(SensitiveDirectoryResult)> callback)
    : handle_type_(handle_type), callback_(std::move(callback)) {
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     handle_type_ == HandleType::kDirectory
                         ? IDS_NATIVE_FILE_SYSTEM_RESTRICTED_DIRECTORY_BUTTON
                         : IDS_NATIVE_FILE_SYSTEM_RESTRICTED_FILE_BUTTON));

  auto run_callback = [](NativeFileSystemRestrictedDirectoryDialogView* dialog,
                         SensitiveDirectoryResult result) {
    std::move(dialog->callback_).Run(result);
  };
  SetAcceptCallback(base::BindOnce(run_callback, base::Unretained(this),
                                   SensitiveDirectoryResult::kTryAgain));
  SetCancelCallback(base::BindOnce(run_callback, base::Unretained(this),
                                   SensitiveDirectoryResult::kAbort));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));

  AddChildView(native_file_system_ui_helper::CreateOriginLabel(
      handle_type_ == HandleType::kDirectory
          ? IDS_NATIVE_FILE_SYSTEM_RESTRICTED_DIRECTORY_TEXT
          : IDS_NATIVE_FILE_SYSTEM_RESTRICTED_FILE_TEXT,
      origin, views::style::CONTEXT_DIALOG_BODY_TEXT, /*show_emphasis=*/true));
}

void ShowNativeFileSystemRestrictedDirectoryDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    content::NativeFileSystemPermissionContext::HandleType handle_type,
    base::OnceCallback<void(
        content::NativeFileSystemPermissionContext::SensitiveDirectoryResult)>
        callback,
    content::WebContents* web_contents) {
  NativeFileSystemRestrictedDirectoryDialogView::ShowDialog(
      origin, path, handle_type, std::move(callback), web_contents);
}
