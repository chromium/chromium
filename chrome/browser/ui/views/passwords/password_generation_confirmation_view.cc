// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_generation_confirmation_view.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

PasswordGenerationConfirmationView::PasswordGenerationConfirmationView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason reason)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*auto_dismissable=*/false),
      controller_(
          PasswordsModelDelegateFromWebContents(web_contents),
          reason == AUTOMATIC
              ? PasswordBubbleControllerBase::DisplayReason::kAutomatic
              : PasswordBubbleControllerBase::DisplayReason::kUserAction) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetButtons(ui::DIALOG_BUTTON_NONE);

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(controller_.save_confirmation_text());
  label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  auto link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PasswordGenerationConfirmationView::StyledLabelLinkClicked,
          base::Unretained(this)));
  link_style.disable_line_wrapping = false;
  label->AddStyleRange(controller_.save_confirmation_link_range(), link_style);

  AddChildView(label.release());
}

PasswordGenerationConfirmationView::~PasswordGenerationConfirmationView() =
    default;

PasswordBubbleControllerBase*
PasswordGenerationConfirmationView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase*
PasswordGenerationConfirmationView::GetController() const {
  return &controller_;
}

void PasswordGenerationConfirmationView::StyledLabelLinkClicked() {
  controller_.OnNavigateToPasswordManagerAccountDashboardLinkClicked(
      password_manager::ManagePasswordsReferrer::
          kPasswordGenerationConfirmation);
  CloseBubble();
}
