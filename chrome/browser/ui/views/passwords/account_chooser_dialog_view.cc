// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/account_chooser_dialog_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/passwords/credential_manager_dialog_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/password_manager/core/browser/password_form.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
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

AccountChooserDialogView::AccountChooserDialogView(
    CredentialManagerDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  DCHECK(controller);
  DCHECK(web_contents);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN));
  set_close_on_deactivate(false);
  SetModalType(ui::mojom::ModalType::kChild);
  if (controller_->ShouldShowFooter()) {
    auto* label = SetFootnoteView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD_FOOTER),
        ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
        views::style::STYLE_SECONDARY));
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
  SetArrow(views::BubbleBorder::NONE);
  set_margins(gfx::Insets::TLBR(margins().top(), 0, margins().bottom(), 0));
}

AccountChooserDialogView::~AccountChooserDialogView() = default;

void AccountChooserDialogView::ShowAccountChooser() {
  // It isn't known until after the creation of this dialog whether the sign-in
  // button should be shown, so always reset the button state here.
  SetButtons(controller_->ShouldShowSignInButton()
                 ? static_cast<int>(ui::mojom::DialogButton::kOk) |
                       static_cast<int>(ui::mojom::DialogButton::kCancel)
                 : static_cast<int>(ui::mojom::DialogButton::kCancel));
  DialogModelChanged();
  InitWindow();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void AccountChooserDialogView::ControllerGone() {
  // During Widget::Close() phase some accessibility event may occur. Thus,
  // |controller_| should be kept around.
  GetWidget()->Close();
  controller_ = nullptr;
}

std::u16string AccountChooserDialogView::GetWindowTitle() const {
  return controller_->GetAccountChooserTitle();
}

bool AccountChooserDialogView::ShouldShowCloseButton() const {
  return false;
}

void AccountChooserDialogView::WindowClosing() {
  if (controller_) {
    controller_->OnCloseDialog();
  }
}

bool AccountChooserDialogView::Accept() {
  DCHECK(controller_);
  controller_->OnSignInClicked();
  // The dialog is closed by the controller.
  return false;
}

void AccountChooserDialogView::InitWindow() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  views::ScrollView* scroll_view =
      AddChildView(std::make_unique<views::ScrollView>());
  auto* list_view = scroll_view->SetContents(std::make_unique<views::View>());
  list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  int item_height = 0;
  for (const auto& form : controller_->GetLocalForms()) {
    const auto titles = GetCredentialLabelsForAccountChooser(*form);
    auto* credential_view =
        list_view->AddChildView(std::make_unique<CredentialsItemView>(
            base::BindRepeating(
                &AccountChooserDialogView::CredentialsItemPressed,
                base::Unretained(this), base::Unretained(form.get())),
            titles.first, titles.second, form.get(),
            GetURLLoaderForMainFrame(web_contents_).get(),
            web_contents_->GetPrimaryMainFrame()->GetLastCommittedOrigin()));
    ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
    gfx::Insets insets =
        layout_provider->GetInsetsMetric(views::INSETS_DIALOG_SUBSECTION);
    const int vertical_padding = layout_provider->GetDistanceMetric(
        views::DISTANCE_RELATED_CONTROL_VERTICAL);
    credential_view->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        vertical_padding, insets.left(), vertical_padding, insets.right())));
    item_height = std::max(item_height, credential_view->GetPreferredHeight());
  }
  constexpr float kMaxVisibleItems = 3.5;
  scroll_view->ClipHeightTo(0, kMaxVisibleItems * item_height);
}

void AccountChooserDialogView::CredentialsItemPressed(
    const password_manager::PasswordForm* form) {
  // On Mac the button click event may be dispatched after the dialog was
  // hidden. Thus, the controller can be null.
  if (controller_) {
    controller_->OnChooseCredentials(
        *form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  }
}

BEGIN_METADATA(AccountChooserDialogView)
END_METADATA

AccountChooserPrompt* CreateAccountChooserPromptView(
    CredentialManagerDialogController* controller,
    content::WebContents* web_contents) {
  return new AccountChooserDialogView(controller, web_contents);
}
