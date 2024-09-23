// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/external_protocol_dialog.h"

#include <utility>

#include "base/strings/string_util.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using content::WebContents;

namespace {

std::u16string GetMessageTextForOrigin(
    const std::optional<url::Origin>& origin) {
  if (!origin || origin->opaque())
    return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_MESSAGE);
  return l10n_util::GetStringFUTF16(
      IDS_EXTERNAL_PROTOCOL_MESSAGE_WITH_INITIATING_ORIGIN,
      url_formatter::FormatOriginForSecurityDisplay(*origin));
}

}  // namespace

#if !BUILDFLAG(IS_CHROMEOS)
// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url,
    WebContents* web_contents,
    ui::PageTransition ignored_page_transition,
    bool ignored_has_user_gesture,
    bool ignored_is_in_fenced_frame_tree,
    const std::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr initiator_document,
    const std::u16string& program_name) {
  DCHECK(web_contents);

  if (program_name.empty()) {
    // ShellExecute won't do anything. Don't bother warning the user.
    return;
  }

  // Windowing system takes ownership.
  new ExternalProtocolDialog(web_contents, url, program_name, initiating_origin,
                             std::move(initiator_document));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

ExternalProtocolDialog::ExternalProtocolDialog(
    WebContents* web_contents,
    const GURL& url,
    const std::u16string& program_name,
    const std::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr initiator_document)
    : web_contents_(web_contents->GetWeakPtr()),
      url_(url),
      program_name_(program_name),
      initiating_origin_(initiating_origin),
      initiator_document_(std::move(initiator_document)) {
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringFUTF16(
                     IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT, program_name_));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_CANCEL_BUTTON_TEXT));

  SetAcceptCallback(base::BindOnce(&ExternalProtocolDialog::OnDialogAccepted,
                                   base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      &ExternalProtocolHandler::RecordHandleStateMetrics,
      false /* checkbox_selected */, ExternalProtocolHandler::BLOCK));
  SetCloseCallback(base::BindOnce(
      &ExternalProtocolHandler::RecordHandleStateMetrics,
      false /* checkbox_selected */, ExternalProtocolHandler::BLOCK));
  SetModalType(ui::mojom::ModalType::kChild);

  message_box_view_ = AddChildView(std::make_unique<views::MessageBoxView>(
      GetMessageTextForOrigin(initiating_origin_)));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  gfx::Insets dialog_insets = provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText);
  dialog_insets.set_left(0);
  set_margins(dialog_insets);

  SetUseDefaultFillLayout(true);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // The checkbox allows the user to opt-in to relaxed security
  // (i.e. skipping future prompts) for the combination of the
  // protocol and the origin of the page initiating this external
  // protocol launch. The checkbox is offered so long as the
  // group policy to show the checkbox is not explicitly disabled
  // and there is a trustworthy initiating origin.
  bool show_remember_selection_checkbox =
      profile->GetPrefs()->GetBoolean(
          prefs::kExternalProtocolDialogShowAlwaysOpenCheckbox) &&
      ExternalProtocolHandler::MayRememberAllowDecisionsForThisOrigin(
          base::OptionalToPtr(initiating_origin_));

  if (show_remember_selection_checkbox) {
    message_box_view_->SetCheckBoxLabel(l10n_util::GetStringFUTF16(
        IDS_EXTERNAL_PROTOCOL_CHECKBOX_PER_ORIGIN_TEXT,
        url_formatter::FormatOriginForSecurityDisplay(
            initiating_origin_.value(),
            /*scheme_display = */ url_formatter::SchemeDisplay::
                OMIT_CRYPTOGRAPHIC)));
  }

  constrained_window::ShowWebModalDialogViews(this, web_contents);
}

ExternalProtocolDialog::~ExternalProtocolDialog() = default;

bool ExternalProtocolDialog::ShouldShowCloseButton() const {
  return false;
}

std::u16string ExternalProtocolDialog::GetWindowTitle() const {
  constexpr int kMaxCommandCharsToDisplay = 32;
  std::u16string elided;
  gfx::ElideString(program_name_, kMaxCommandCharsToDisplay, &elided);
  return l10n_util::GetStringFUTF16(IDS_EXTERNAL_PROTOCOL_TITLE, elided);
}

void ExternalProtocolDialog::OnDialogAccepted() {
  const bool remember = message_box_view_->IsCheckBoxSelected();
  ExternalProtocolHandler::RecordHandleStateMetrics(
      remember, ExternalProtocolHandler::DONT_BLOCK);

  if (!web_contents_) {
    // Dialog outlasted the WebContents.
    return;
  }

  if (remember) {
    DCHECK(initiating_origin_);
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());

    ExternalProtocolHandler::SetBlockState(url_.scheme(), *initiating_origin_,
                                           ExternalProtocolHandler::DONT_BLOCK,
                                           profile);
  }

  ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(
      url_, web_contents_.get(), initiator_document_);
}

void ExternalProtocolDialog::SetRememberSelectionCheckboxCheckedForTesting(
    bool checked) {
  message_box_view_->SetCheckBoxSelected(checked);
}

BEGIN_METADATA(ExternalProtocolDialog)
END_METADATA
