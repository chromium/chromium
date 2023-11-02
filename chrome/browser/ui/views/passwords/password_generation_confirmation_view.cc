// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_generation_confirmation_view.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

constexpr base::TimeDelta kCloseTimeout = base::Seconds(30);

PasswordGenerationConfirmationView::PasswordGenerationConfirmationView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason reason)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(
          PasswordsModelDelegateFromWebContents(web_contents),
          reason == AUTOMATIC
              ? PasswordBubbleControllerBase::DisplayReason::kAutomatic
              : PasswordBubbleControllerBase::DisplayReason::kUserAction) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetShowIcon(true);

  AddChildView(CreateGooglePasswordManagerLabel(
      /*text_message_id=*/
      IDS_PASSWORD_GENERATION_CONFIRMATION_GOOGLE_PASSWORD_MANAGER,
      /*link_message_id=*/
      IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
      base::BindRepeating(
          &PasswordGenerationConfirmationView::StyledLabelLinkClicked,
          base::Unretained(this))));

  if (reason == AUTOMATIC) {
    // Unretained() is safe because |timer_| is owned by |this|.
    timer_.Start(
        FROM_HERE, kCloseTimeout,
        base::BindOnce(&PasswordGenerationConfirmationView::CloseBubble,
                       base::Unretained(this)));
  }
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

ui::ImageModel PasswordGenerationConfirmationView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

void PasswordGenerationConfirmationView::StyledLabelLinkClicked() {
  controller_.OnNavigateToPasswordManagerAccountDashboardLinkClicked(
      password_manager::ManagePasswordsReferrer::
          kPasswordGenerationConfirmation);
  CloseBubble();
}
