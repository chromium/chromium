// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/sign_in_check_bubble_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_info_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"

namespace {
std::unique_ptr<views::Label> CreateBodyText(const std::u16string& text) {
  auto body_text = std::make_unique<views::Label>(
      text, views::style::CONTEXT_DIALOG_BODY_TEXT);
  body_text->SetMultiLine(true);
  body_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return body_text;
}
}  // namespace

SignInCheckBubbleView::SignInCheckBubbleView(
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
                              DISTANCE_CONTROL_LIST_VERTICAL),
                          0));

  // TODO(crbug.com/381053962): Get the current origit from controller.
  AddChildView(CreateBodyText(u"demo.com"));
  AddChildView(CreateBodyText(l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_SIGN_IN_CHECK_DETAILS)));

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CANCEL));
  SetCancelCallback(
      base::BindOnce(&PasswordChangeInfoBubbleController::CancelPasswordChange,
                     base::Unretained(&controller_)));
}

SignInCheckBubbleView::~SignInCheckBubbleView() = default;

PasswordBubbleControllerBase* SignInCheckBubbleView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* SignInCheckBubbleView::GetController()
    const {
  return &controller_;
}

BEGIN_METADATA(SignInCheckBubbleView)
END_METADATA
