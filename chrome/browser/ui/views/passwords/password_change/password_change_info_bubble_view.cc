// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/password_change_info_bubble_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_info_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"

namespace {
std::unique_ptr<views::Label> CreateLabel(const std::u16string& text) {
  auto body_text = std::make_unique<views::Label>(
      text, views::style::CONTEXT_DIALOG_BODY_TEXT);
  body_text->SetMultiLine(true);
  body_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return body_text;
}

}  // namespace

PasswordChangeInfoBubbleView::PasswordChangeInfoBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    PasswordChangeDelegate::State state)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents), state) {
  views::FlexLayout* flex_layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_CONTROL_LIST_VERTICAL),
                          0));

  AddChildView(CreateLabel(controller_.GetDisplayOrigin()));
  std::unique_ptr<views::View> body_text = CreateBodyText(state);
  body_text->SetID(kChangingPasswordBodyText);
  AddChildView(std::move(body_text));

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CANCEL));
  SetCancelCallback(
      base::BindOnce(&PasswordChangeInfoBubbleController::CancelPasswordChange,
                     base::Unretained(&controller_)));
}

PasswordChangeInfoBubbleView::~PasswordChangeInfoBubbleView() = default;

PasswordBubbleControllerBase* PasswordChangeInfoBubbleView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase*
PasswordChangeInfoBubbleView::GetController() const {
  return &controller_;
}

std::unique_ptr<views::View> PasswordChangeInfoBubbleView::CreateBodyText(
    PasswordChangeDelegate::State state) {
  if (state == PasswordChangeDelegate::State::kWaitingForChangePasswordForm) {
    return CreateLabel(l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_UI_SIGN_IN_CHECK_DETAILS));
  }
  if (state == PasswordChangeDelegate::State::kChangingPassword) {
    base::RepeatingClosure open_password_manager_closure = base::BindRepeating(
        &PasswordChangeInfoBubbleController::OnGooglePasswordManagerLinkClicked,
        base::Unretained(&controller_));
    return CreateGooglePasswordManagerLabel(
        /*text_message_id=*/
        IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_INFO_BUBBLE_DETAILS,
        /*link_message_id=*/
        IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
        controller_.GetPrimaryAccountEmail(), open_password_manager_closure,
        CONTEXT_DIALOG_BODY_TEXT_SMALL, views::style::STYLE_PRIMARY);
  }
  NOTREACHED();
}

BEGIN_METADATA(PasswordChangeInfoBubbleView)
END_METADATA
