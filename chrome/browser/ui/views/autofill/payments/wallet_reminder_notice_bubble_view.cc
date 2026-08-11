// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/wallet_reminder_notice_bubble_view.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace autofill {

WalletReminderNoticeBubbleView::WalletReminderNoticeBubbleView(
    views::BubbleAnchor anchor_view,
    content::WebContents* web_contents,
    WalletReminderNoticeBubbleController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller->GetWeakPtr()) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_WALLET_REMINDER_NOTICE_CONFIRM_BUTTON_LABEL));
  SetShowCloseButton(false);

  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetName(GetWindowTitle());

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

WalletReminderNoticeBubbleView::~WalletReminderNoticeBubbleView() = default;

void WalletReminderNoticeBubbleView::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void WalletReminderNoticeBubbleView::Hide() {
  CloseBubble();
  WindowClosing();
}

void WalletReminderNoticeBubbleView::AddedToWidget() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view =
      std::make_unique<views::ImageView>(bundle.GetThemedLottieImageNamed(
          IDR_AUTOFILL_WALLET_REMINDER_NOTICE_LOTTIE));
  image_view->GetViewAccessibility().SetIsInvisible(true);
  GetBubbleFrameView()->SetHeaderView(std::move(image_view));

  auto title_view = std::make_unique<views::Label>(
      GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
  title_view->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_view->SetMultiLine(true);
  GetBubbleFrameView()->SetTitleView(std::move(title_view));
}

std::u16string WalletReminderNoticeBubbleView::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void WalletReminderNoticeBubbleView::WindowClosing() {
  // TODO(crbug.com/543473467): Handle closing the bubble via the controller.
}

void WalletReminderNoticeBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  if (!controller_) {
    return;
  }
  const LegalMessageLines& legal_message_lines =
      controller_->GetLegalMessageLines();
  if (legal_message_lines.empty()) {
    return;
  }

  // TODO(crbug.com/543948117): Handle legal message line link clicked.
  AddChildView(CreateLegalMessageView(legal_message_lines,
                                      /*user_email=*/std::u16string(),
                                      /*user_avatar=*/ui::ImageModel(),
                                      /*callback=*/base::DoNothing()));
}

BEGIN_METADATA(WalletReminderNoticeBubbleView)
END_METADATA

}  // namespace autofill
