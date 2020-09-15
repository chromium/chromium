// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/prompt_for_scanning_modal_dialog.h"

#include <memory>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/window/dialog_delegate.h"

namespace safe_browsing {

/*static*/
void PromptForScanningModalDialog::ShowForWebContents(
    content::WebContents* web_contents,
    const base::string16& filename,
    base::OnceClosure accept_callback,
    base::OnceClosure open_now_callback) {
  constrained_window::ShowWebModalDialogViews(
      new PromptForScanningModalDialog(web_contents, filename,
                                       std::move(accept_callback),
                                       std::move(open_now_callback)),
      web_contents);
}

PromptForScanningModalDialog::PromptForScanningModalDialog(
    content::WebContents* web_contents,
    const base::string16& filename,
    base::OnceClosure accept_callback,
    base::OnceClosure open_now_callback)
    : filename_(filename), open_now_callback_(std::move(open_now_callback)) {
  SetTitle(IDS_DEEP_SCANNING_INFO_DIALOG_TITLE);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_INFO_DIALOG_ACCEPT_BUTTON));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_INFO_DIALOG_CANCEL_BUTTON));
  SetAcceptCallback(std::move(accept_callback));
  open_now_button_ = SetExtraView(std::make_unique<views::MdTextButton>(
      this, l10n_util::GetStringUTF16(
                IDS_DEEP_SCANNING_INFO_DIALOG_OPEN_NOW_BUTTON)));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Use a fixed maximum message width, so longer messages will wrap.
  const int kMaxMessageWidth = 400;
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kFixed, kMaxMessageWidth, false);

  // Create the message label text.
  std::vector<size_t> offsets;
  base::string16 message_text = base::ReplaceStringPlaceholders(
      base::ASCIIToUTF16("$1 $2"),
      {l10n_util::GetStringFUTF16(IDS_DEEP_SCANNING_INFO_DIALOG_MESSAGE,
                                  filename_),
       l10n_util::GetStringUTF16(IDS_LEARN_MORE)},
      &offsets);

  // Add the message label.
  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(message_text);

  gfx::Range learn_more_range(offsets[1], message_text.length());
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          [](content::WebContents* web_contents, const ui::Event& event) {
            web_contents->OpenURL(content::OpenURLParams(
                GURL(chrome::kAdvancedProtectionDownloadLearnMoreURL),
                content::Referrer(),
                ui::DispositionFromEventFlags(
                    event.flags(), WindowOpenDisposition::NEW_FOREGROUND_TAB),
                ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false));
          },
          web_contents));
  label->AddStyleRange(learn_more_range, link_style);

  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SizeToFit(kMaxMessageWidth);
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(std::move(label));
}

PromptForScanningModalDialog::~PromptForScanningModalDialog() = default;

bool PromptForScanningModalDialog::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return (button == ui::DIALOG_BUTTON_OK || button == ui::DIALOG_BUTTON_CANCEL);
}

bool PromptForScanningModalDialog::ShouldShowCloseButton() const {
  return false;
}

ui::ModalType PromptForScanningModalDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void PromptForScanningModalDialog::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  if (sender == open_now_button_) {
    std::move(open_now_callback_).Run();
    CancelDialog();
  }
}

}  // namespace safe_browsing
