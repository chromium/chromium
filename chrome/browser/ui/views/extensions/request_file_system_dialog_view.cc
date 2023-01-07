// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/request_file_system_dialog_view.h"

#include <stddef.h>

#include <cstdlib>

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/font.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"

namespace {

// Maximum width of the dialog in pixels.
const int kDialogMaxWidth = 320;

}  // namespace

// static
void RequestFileSystemDialogView::ShowDialog(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const std::string& volume_label,
    bool writable,
    base::OnceCallback<void(ui::DialogButton)> callback) {
  constrained_window::ShowWebModalDialogViews(
      new RequestFileSystemDialogView(extension_name, volume_label, writable,
                                      std::move(callback)),
      web_contents);
}

RequestFileSystemDialogView::~RequestFileSystemDialogView() {
  // Ensure that `callback_` is invoked when dialog is destroyed by means beyond
  // its UI elements, e.g., by App closing.
  if (callback_)
    std::move(callback_).Run(ui::DIALOG_BUTTON_CANCEL);
}

gfx::Size RequestFileSystemDialogView::CalculatePreferredSize() const {
  return gfx::Size(kDialogMaxWidth,
                   children().front()->GetHeightForWidth(kDialogMaxWidth));
}

RequestFileSystemDialogView::RequestFileSystemDialogView(
    const std::string& extension_name,
    const std::string& volume_label,
    bool writable,
    base::OnceCallback<void(ui::DialogButton)> callback)
    : callback_(std::move(callback)) {
  SetAccessibleTitle(l10n_util::GetStringUTF16(
      IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_TITLE));
  SetDefaultButton(ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_ALLOW_BUTTON));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(
                     IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_DENY_BUTTON));

  auto run_callback = [](RequestFileSystemDialogView* dialog,
                         ui::DialogButton button) {
    std::move(dialog->callback_).Run(button);
  };
  SetAcceptCallback(base::BindOnce(run_callback, base::Unretained(this),
                                   ui::DIALOG_BUTTON_OK));
  SetCancelCallback(base::BindOnce(run_callback, base::Unretained(this),
                                   ui::DIALOG_BUTTON_CANCEL));
  SetCloseCallback(base::BindOnce(run_callback, base::Unretained(this),
                                  ui::DIALOG_BUTTON_CANCEL));
  SetModalType(ui::MODAL_TYPE_CHILD);

  DCHECK(callback_);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));

  const std::u16string app_name = base::UTF8ToUTF16(extension_name);
  // TODO(mtomasz): Improve the dialog contents, so it's easier for the user
  // to understand what device is being requested.
  const std::u16string volume_name = base::UTF8ToUTF16(volume_label);

  std::vector<size_t> placeholder_offsets;
  const std::u16string message = l10n_util::GetStringFUTF16(
      writable ? IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_WRITABLE_MESSAGE
               : IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_MESSAGE,
      app_name, volume_name, &placeholder_offsets);

  views::StyledLabel* const label =
      AddChildView(std::make_unique<views::StyledLabel>());
  label->SetText(message);
  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.text_style = views::style::STYLE_EMPHASIZED;

  DCHECK_EQ(2u, placeholder_offsets.size());
  label->AddStyleRange(gfx::Range(placeholder_offsets[0],
                                  placeholder_offsets[0] + app_name.length()),
                       bold_style);
  label->AddStyleRange(
      gfx::Range(placeholder_offsets[1],
                 placeholder_offsets[1] + volume_name.length()),
      bold_style);

  SetLayoutManager(std::make_unique<views::FillLayout>());
}

BEGIN_METADATA(RequestFileSystemDialogView, views::DialogDelegateView)
END_METADATA
