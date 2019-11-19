// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_card_manage_cards_bubble_views.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/sync/bubble_sync_promo_view_util.h"
#endif

namespace {

std::unique_ptr<views::View> CreateManageCardsButton(
    views::ButtonListener* listener) {
  auto manage_cards_button = views::MdTextButton::CreateSecondaryUiButton(
      listener, l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_CARDS));
  manage_cards_button->SetID(autofill::DialogViewId::MANAGE_CARDS_BUTTON);
  return manage_cards_button;
}

}  // namespace

namespace autofill {

SaveCardManageCardsBubbleViews::SaveCardManageCardsBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveCardBubbleController* controller)
    : SaveCardBubbleViews(anchor_view, web_contents, controller) {
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_OK);
  DialogDelegate::SetExtraView(CreateManageCardsButton(this));
}

std::unique_ptr<views::View>
SaveCardManageCardsBubbleViews::CreateFootnoteView() {
#if defined(OS_CHROMEOS)
  // ChromeOS does not show the signin promo.
  return nullptr;
#else
  if (!controller()->ShouldShowSignInPromo())
    return nullptr;

  sync_promo_delegate_ =
      std::make_unique<SaveCardManageCardsBubbleViews::SyncPromoDelegate>(
          controller(),
          signin_metrics::AccessPoint::ACCESS_POINT_MANAGE_CARDS_BUBBLE);

  BubbleSyncPromoViewParams params;
  params.link_text_resource_id = IDS_AUTOFILL_SIGNIN_PROMO_LINK_DICE_DISABLED;
  params.message_text_resource_id =
      IDS_AUTOFILL_SIGNIN_PROMO_MESSAGE_DICE_DISABLED;
  params.dice_no_accounts_promo_message_resource_id =
      IDS_AUTOFILL_SIGNIN_PROMO_MESSAGE;
  params.dice_accounts_promo_message_resource_id =
      IDS_AUTOFILL_SYNC_PROMO_MESSAGE;
  params.dice_signin_button_prominent = false;
  params.dice_text_style = views::style::STYLE_SECONDARY;

  auto promo_view = CreateBubbleSyncPromoView(
      controller()->GetProfile(), sync_promo_delegate_.get(),
      signin_metrics::AccessPoint::ACCESS_POINT_MANAGE_CARDS_BUBBLE, params);

  DCHECK(promo_view);
  InitFootnoteView(promo_view.get());
  return promo_view;
#endif
}

SaveCardManageCardsBubbleViews::~SaveCardManageCardsBubbleViews() {}

std::unique_ptr<views::View>
SaveCardManageCardsBubbleViews::CreateMainContentView() {
  std::unique_ptr<views::View> view =
      SaveCardBubbleViews::CreateMainContentView();
  view->SetID(DialogViewId::MANAGE_CARDS_VIEW);
  return view;
}

void SaveCardManageCardsBubbleViews::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  if (sender->GetViewByID(DialogViewId::MANAGE_CARDS_BUTTON)) {
    controller()->OnManageCardsClicked();
    CloseBubble();
  }
}

}  // namespace autofill
