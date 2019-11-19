// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_confirmation_view.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

PasswordSaveConfirmationView::PasswordSaveConfirmationView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason reason)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             reason,
                             /*auto_dismissable=*/false) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);

  auto label = std::make_unique<views::StyledLabel>(
      model()->save_confirmation_text(), this);
  label->SetTextContext(CONTEXT_BODY_TEXT_LARGE);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  auto link_style = views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.disable_line_wrapping = false;
  label->AddStyleRange(model()->save_confirmation_link_range(), link_style);

  AddChildView(label.release());
}

PasswordSaveConfirmationView::~PasswordSaveConfirmationView() = default;

bool PasswordSaveConfirmationView::ShouldShowCloseButton() const {
  return true;
}

void PasswordSaveConfirmationView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  DCHECK_EQ(range, model()->save_confirmation_link_range());
  model()->OnNavigateToPasswordManagerAccountDashboardLinkClicked(
      password_manager::ManagePasswordsReferrer::
          kPasswordGenerationConfirmation);
  CloseBubble();
}

gfx::Size PasswordSaveConfirmationView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}
