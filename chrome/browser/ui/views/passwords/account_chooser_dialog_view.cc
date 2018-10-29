// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/account_chooser_dialog_view.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace {

// Maximum height of the credential list. The unit is one row's height.
constexpr double kMaxHeightAccounts = 3.5;

// Creates a list view of credentials in |forms|.
views::ScrollView* CreateCredentialsView(
    const PasswordDialogController::FormsVector& forms,
    views::ButtonListener* button_listener,
    network::mojom::URLLoaderFactory* loader_factory) {
  views::View* list_view = new views::View;
  list_view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  int item_height = 0;
  for (const auto& form : forms) {
    std::pair<base::string16, base::string16> titles =
        GetCredentialLabelsForAccountChooser(*form);
    CredentialsItemView* credential_view =
        new CredentialsItemView(button_listener, titles.first, titles.second,
                                kButtonHoverColor, form.get(), loader_factory);
    credential_view->SetLowerLabelColor(kAutoSigninTextColor);
    ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
    gfx::Insets insets =
        layout_provider->GetInsetsMetric(views::INSETS_DIALOG_SUBSECTION);
    const int vertical_padding = layout_provider->GetDistanceMetric(
        views::DISTANCE_RELATED_CONTROL_VERTICAL);
    credential_view->SetBorder(views::CreateEmptyBorder(
        vertical_padding, insets.left(), vertical_padding, insets.right()));
    item_height = std::max(item_height, credential_view->GetPreferredHeight());
    list_view->AddChildView(credential_view);
  }
  views::ScrollView* scroll_view = new views::ScrollView;
  scroll_view->ClipHeightTo(0, kMaxHeightAccounts * item_height);
  scroll_view->SetContents(list_view);
  return scroll_view;
}

}  // namespace

AccountChooserDialogView::AccountChooserDialogView(
    PasswordDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller),
      web_contents_(web_contents),
      show_signin_button_(false) {
  DCHECK(controller);
  DCHECK(web_contents);
  set_close_on_deactivate(false);
  SetArrow(views::BubbleBorder::NONE);
  set_margins(gfx::Insets(margins().top(), 0, margins().bottom(), 0));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::ACCOUNT_CHOOSER);
}

AccountChooserDialogView::~AccountChooserDialogView() = default;

void AccountChooserDialogView::ShowAccountChooser() {
  show_signin_button_ = controller_->ShouldShowSignInButton();
  InitWindow();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void AccountChooserDialogView::ControllerGone() {
  // During Widget::Close() phase some accessibility event may occur. Thus,
  // |controller_| should be kept around.
  GetWidget()->Close();
  controller_ = nullptr;
}

ui::ModalType AccountChooserDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 AccountChooserDialogView::GetWindowTitle() const {
  return controller_->GetAccoutChooserTitle();
}

bool AccountChooserDialogView::ShouldShowCloseButton() const {
  return false;
}

void AccountChooserDialogView::WindowClosing() {
  if (controller_)
    controller_->OnCloseDialog();
}

bool AccountChooserDialogView::Accept() {
  DCHECK(show_signin_button_);
  DCHECK(controller_);
  controller_->OnSignInClicked();
  // The dialog is closed by the controller.
  return false;
}

int AccountChooserDialogView::GetDialogButtons() const {
  if (show_signin_button_)
    return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 AccountChooserDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  int message_id = 0;
  if (button == ui::DIALOG_BUTTON_OK)
    message_id = IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN;
  else if (button == ui::DIALOG_BUTTON_CANCEL)
    message_id = IDS_APP_CANCEL;
  else
    NOTREACHED();
  return l10n_util::GetStringUTF16(message_id);
}

views::View* AccountChooserDialogView::CreateFootnoteView() {
  if (!controller_->ShouldShowFooter())
    return nullptr;
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD_FOOTER),
      ChromeTextContext::CONTEXT_BODY_TEXT_SMALL, STYLE_SECONDARY);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

void AccountChooserDialogView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  CredentialsItemView* view = static_cast<CredentialsItemView*>(sender);
  // On Mac the button click event may be dispatched after the dialog was
  // hidden. Thus, the controller can be NULL.
  if (controller_) {
    controller_->OnChooseCredentials(
        *view->form(),
        password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  }
}

void AccountChooserDialogView::InitWindow() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(CreateCredentialsView(
      controller_->GetLocalForms(), this,
      content::BrowserContext::GetDefaultStoragePartition(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()))
          ->GetURLLoaderFactoryForBrowserProcess()
          .get()));
}

AccountChooserPrompt* CreateAccountChooserPromptView(
    PasswordDialogController* controller, content::WebContents* web_contents) {
  return new AccountChooserDialogView(controller, web_contents);
}
