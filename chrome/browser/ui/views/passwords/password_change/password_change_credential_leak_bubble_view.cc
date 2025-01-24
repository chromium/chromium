// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/password_change_credential_leak_bubble_view.h"

#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_credential_leak_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/vector_icons.h"

PasswordChangeCredentialLeakBubbleView::PasswordChangeCredentialLeakBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    base::WeakPtr<PasswordsLeakDialogDelegate> leak_dialog_delegate,
    password_manager::LeakedPasswordDetails details)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents),
                  leak_dialog_delegate,
                  std::move(details)) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CHANGE_PASSWORD));
  SetAcceptCallback(base::BindOnce(
      &PasswordChangeCredentialLeakBubbleController::ChangePassword,
      base::Unretained(&controller_)));
}

PasswordChangeCredentialLeakBubbleView::
    ~PasswordChangeCredentialLeakBubbleView() = default;

PasswordBubbleControllerBase*
PasswordChangeCredentialLeakBubbleView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase*
PasswordChangeCredentialLeakBubbleView::GetController() const {
  return &controller_;
}

void PasswordChangeCredentialLeakBubbleView::OnWidgetInitialized() {
  PasswordBubbleViewBase::OnWidgetInitialized();
  GetOkButton()->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(views::kPasswordChangeIcon,
                                     ui::kColorIconSecondary,
                                     GetLayoutConstant(PAGE_INFO_ICON_SIZE)));
  GetOkButton()->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetBubbleHeader(IDR_SAVE_PASSWORD, IDR_SAVE_PASSWORD_DARK);
}

BEGIN_METADATA(PasswordChangeCredentialLeakBubbleView)
END_METADATA
