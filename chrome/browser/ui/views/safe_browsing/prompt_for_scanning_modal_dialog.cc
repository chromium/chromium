// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/prompt_for_scanning_modal_dialog.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/window/dialog_delegate.h"

namespace safe_browsing {

/*static*/
void PromptForScanningModalDialog::ShowForWebContents(
    content::WebContents* web_contents,
    const std::u16string& filename,
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
    const std::u16string& filename,
    base::OnceClosure accept_callback,
    base::OnceClosure open_now_callback)
    : open_now_callback_(std::move(open_now_callback)) {
  SetModalType(ui::mojom::ModalType::kChild);
  SetTitle(IDS_DEEP_SCANNING_INFO_DIALOG_TITLE);
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_INFO_DIALOG_ACCEPT_BUTTON));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_INFO_DIALOG_CANCEL_BUTTON));
  SetAcceptCallback(std::move(accept_callback));
  SetExtraView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          [](PromptForScanningModalDialog* dialog) {
            std::move(dialog->open_now_callback_).Run();
            dialog->CancelDialog();
          },
          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_DEEP_SCANNING_INFO_DIALOG_OPEN_NOW_BUTTON)));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetUseDefaultFillLayout(true);

  // Create the message label text.
  std::vector<size_t> offsets;
  std::u16string message_text = base::ReplaceStringPlaceholders(
      u"$1 $2",
      {l10n_util::GetStringFUTF16(IDS_DEEP_SCANNING_INFO_DIALOG_MESSAGE,
                                  filename),
       l10n_util::GetStringUTF16(IDS_LEARN_MORE)},
      &offsets);

  // Add the message label.
  auto* label = AddChildView(std::make_unique<views::StyledLabel>());
  label->SetText(message_text);
  label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  label->SetDefaultTextStyle(views::style::STYLE_PRIMARY);

  gfx::Range learn_more_range(offsets[1], message_text.length());
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          [](content::WebContents* web_contents, const ui::Event& event) {
            web_contents->OpenURL(
                content::OpenURLParams(
                    GURL(chrome::kAdvancedProtectionDownloadLearnMoreURL),
                    content::Referrer(),
                    ui::DispositionFromEventFlags(
                        event.flags(),
                        WindowOpenDisposition::NEW_FOREGROUND_TAB),
                    ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false),
                /*navigation_handle_callback=*/{});
          },
          web_contents));
  label->AddStyleRange(learn_more_range, link_style);

  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  constexpr int kMaxMessageWidth = 400;
  label->SizeToFit(kMaxMessageWidth);
}

PromptForScanningModalDialog::~PromptForScanningModalDialog() = default;

bool PromptForScanningModalDialog::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  return (button == ui::mojom::DialogButton::kOk ||
          button == ui::mojom::DialogButton::kCancel);
}

bool PromptForScanningModalDialog::ShouldShowCloseButton() const {
  return false;
}

BEGIN_METADATA(PromptForScanningModalDialog)
END_METADATA

}  // namespace safe_browsing
