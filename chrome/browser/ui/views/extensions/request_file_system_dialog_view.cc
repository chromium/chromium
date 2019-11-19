// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/request_file_system_dialog_view.h"

#include <stddef.h>

#include <cstdlib>

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

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
    const base::Callback<void(ui::DialogButton)>& callback) {
  constrained_window::ShowWebModalDialogViews(
      new RequestFileSystemDialogView(extension_name, volume_label, writable,
                                      callback),
      web_contents);
}

RequestFileSystemDialogView::~RequestFileSystemDialogView() {}

base::string16 RequestFileSystemDialogView::GetAccessibleWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_TITLE);
}

ui::ModalType RequestFileSystemDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool RequestFileSystemDialogView::Cancel() {
  callback_.Run(ui::DIALOG_BUTTON_CANCEL);
  return true;
}

bool RequestFileSystemDialogView::Accept() {
  callback_.Run(ui::DIALOG_BUTTON_OK);
  return true;
}

gfx::Size RequestFileSystemDialogView::CalculatePreferredSize() const {
  return gfx::Size(kDialogMaxWidth,
                   children().front()->GetHeightForWidth(kDialogMaxWidth));
}

RequestFileSystemDialogView::RequestFileSystemDialogView(
    const std::string& extension_name,
    const std::string& volume_label,
    bool writable,
    const base::Callback<void(ui::DialogButton)>& callback)
    : callback_(callback) {
  DialogDelegate::set_default_button(ui::DIALOG_BUTTON_CANCEL);
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_ALLOW_BUTTON));
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_DENY_BUTTON));

  DCHECK(!callback_.is_null());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));

  const base::string16 app_name = base::UTF8ToUTF16(extension_name);
  // TODO(mtomasz): Improve the dialog contents, so it's easier for the user
  // to understand what device is being requested.
  const base::string16 volume_name = base::UTF8ToUTF16(volume_label);

  std::vector<size_t> placeholder_offsets;
  const base::string16 message = l10n_util::GetStringFUTF16(
      writable ? IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_WRITABLE_MESSAGE
               : IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_MESSAGE,
      app_name, volume_name, &placeholder_offsets);

  views::StyledLabel* const label = new views::StyledLabel(message, nullptr);
  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.text_style = STYLE_EMPHASIZED;

  DCHECK_EQ(2u, placeholder_offsets.size());
  label->AddStyleRange(gfx::Range(placeholder_offsets[0],
                                  placeholder_offsets[0] + app_name.length()),
                       bold_style);
  label->AddStyleRange(
      gfx::Range(placeholder_offsets[1],
                 placeholder_offsets[1] + volume_name.length()),
      bold_style);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  AddChildView(label);
}
