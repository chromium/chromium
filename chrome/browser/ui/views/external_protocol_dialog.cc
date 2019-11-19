// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/external_protocol_dialog.h"

#include <utility>

#include "base/strings/string_util.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using content::WebContents;

namespace {

base::string16 GetMessageTextForOrigin(
    const base::Optional<url::Origin>& origin) {
  if (!origin || origin->opaque())
    return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_MESSAGE);
  return l10n_util::GetStringFUTF16(
      IDS_EXTERNAL_PROTOCOL_MESSAGE_WITH_INITIATING_ORIGIN,
      url_formatter::FormatOriginForSecurityDisplay(*origin));
}

}  // namespace

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url,
    WebContents* web_contents,
    ui::PageTransition ignored_page_transition,
    bool ignored_has_user_gesture,
    const base::Optional<url::Origin>& initiating_origin) {
  DCHECK(web_contents);

  base::string16 program_name =
      shell_integration::GetApplicationNameForProtocol(url);
  if (program_name.empty()) {
    // ShellExecute won't do anything. Don't bother warning the user.
    return;
  }

  // Windowing system takes ownership.
  new ExternalProtocolDialog(web_contents, url, program_name,
                             initiating_origin);
}

ExternalProtocolDialog::~ExternalProtocolDialog() {}

gfx::Size ExternalProtocolDialog::CalculatePreferredSize() const {
  constexpr int kDialogContentWidth = 400;
  return gfx::Size(kDialogContentWidth, GetHeightForWidth(kDialogContentWidth));
}

bool ExternalProtocolDialog::ShouldShowCloseButton() const {
  return false;
}

base::string16 ExternalProtocolDialog::GetWindowTitle() const {
  constexpr int kMaxCommandCharsToDisplay = 32;
  base::string16 elided;
  gfx::ElideString(program_name_, kMaxCommandCharsToDisplay, &elided);
  return l10n_util::GetStringFUTF16(IDS_EXTERNAL_PROTOCOL_TITLE, elided);
}

bool ExternalProtocolDialog::Cancel() {
  ExternalProtocolHandler::RecordHandleStateMetrics(
      false /* checkbox_selected */, ExternalProtocolHandler::BLOCK);
  return true;
}

bool ExternalProtocolDialog::Accept() {
  const bool remember = message_box_view_->IsCheckBoxSelected();
  ExternalProtocolHandler::RecordHandleStateMetrics(
      remember, ExternalProtocolHandler::DONT_BLOCK);

  if (!web_contents()) {
    // Dialog outlasted the WebContents.
    return true;
  }

  if (remember) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());

    ExternalProtocolHandler::SetBlockState(
        url_.scheme(), ExternalProtocolHandler::DONT_BLOCK, profile);
  }

  ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(url_, web_contents());
  return true;
}

views::View* ExternalProtocolDialog::GetContentsView() {
  return message_box_view_;
}

ui::ModalType ExternalProtocolDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

views::Widget* ExternalProtocolDialog::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* ExternalProtocolDialog::GetWidget() const {
  return message_box_view_->GetWidget();
}

void ExternalProtocolDialog::ShowRememberSelectionCheckbox() {
  message_box_view_->SetCheckBoxLabel(
      l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_CHECKBOX_TEXT));
}

void ExternalProtocolDialog::SetRememberSelectionCheckboxCheckedForTesting(
    bool checked) {
  if (!message_box_view_->HasCheckBox())
    ShowRememberSelectionCheckbox();
  message_box_view_->SetCheckBoxSelected(checked);
}

ExternalProtocolDialog::ExternalProtocolDialog(
    WebContents* web_contents,
    const GURL& url,
    const base::string16& program_name,
    const base::Optional<url::Origin>& initiating_origin)
    : content::WebContentsObserver(web_contents),
      url_(url),
      program_name_(program_name),
      initiating_origin_(initiating_origin) {
  DialogDelegate::set_default_button(ui::DIALOG_BUTTON_CANCEL);
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringFUTF16(IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT,
                                 program_name_));
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_CANCEL_BUTTON_TEXT));

  views::MessageBoxView::InitParams params(
      GetMessageTextForOrigin(initiating_origin_));
  message_box_view_ = new views::MessageBoxView(params);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  set_margins(
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->GetPrefs()->GetBoolean(
          prefs::kExternalProtocolDialogShowAlwaysOpenCheckbox)) {
    ShowRememberSelectionCheckbox();
  }
  constrained_window::ShowWebModalDialogViews(this, web_contents);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::EXTERNAL_PROTOCOL);
}
