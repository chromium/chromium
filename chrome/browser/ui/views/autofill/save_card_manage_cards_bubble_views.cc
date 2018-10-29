// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_card_manage_cards_bubble_views.h"

#include <memory>

#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/sync/bubble_sync_promo_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#endif

namespace autofill {

SaveCardManageCardsBubbleViews::SaveCardManageCardsBubbleViews(
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    content::WebContents* web_contents,
    SaveCardBubbleController* controller)
    : SaveCardBubbleViews(anchor_view, anchor_point, web_contents, controller) {
}

views::View* SaveCardManageCardsBubbleViews::CreateFootnoteView() {
  if (!controller()->ShouldShowSignInPromo())
    return nullptr;

  views::View* promo_view = nullptr;

  Profile* profile = controller()->GetProfile();
  sync_promo_delegate_ =
      std::make_unique<SaveCardManageCardsBubbleViews::SyncPromoDelegate>(
          controller(),
          signin_metrics::AccessPoint::ACCESS_POINT_MANAGE_CARDS_BUBBLE);
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    promo_view = new DiceBubbleSyncPromoView(
        profile, sync_promo_delegate_.get(),
        signin_metrics::AccessPoint::ACCESS_POINT_MANAGE_CARDS_BUBBLE,
        IDS_AUTOFILL_SIGNIN_PROMO_MESSAGE, IDS_AUTOFILL_SYNC_PROMO_MESSAGE,
        /*prominent=*/false, ChromeTextStyle::STYLE_SECONDARY);
#else
    NOTREACHED();
#endif
  } else {
    promo_view = new BubbleSyncPromoView(
        sync_promo_delegate_.get(),
        signin_metrics::AccessPoint::ACCESS_POINT_MANAGE_CARDS_BUBBLE,
        IDS_AUTOFILL_SIGNIN_PROMO_LINK_DICE_DISABLED,
        IDS_AUTOFILL_SIGNIN_PROMO_MESSAGE_DICE_DISABLED);
  }

  InitFootnoteView(promo_view);
  return promo_view;
}

views::View* SaveCardManageCardsBubbleViews::CreateExtraView() {
  views::View* manage_cards_button =
      views::MdTextButton::CreateSecondaryUiButton(
          this, l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_CARDS));
  manage_cards_button->set_id(DialogViewId::MANAGE_CARDS_BUTTON);
  return manage_cards_button;
}

int SaveCardManageCardsBubbleViews::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 SaveCardManageCardsBubbleViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DONE);
}

SaveCardManageCardsBubbleViews::~SaveCardManageCardsBubbleViews() {}

std::unique_ptr<views::View>
SaveCardManageCardsBubbleViews::CreateMainContentView() {
  std::unique_ptr<views::View> view =
      SaveCardBubbleViews::CreateMainContentView();
  view->set_id(DialogViewId::MANAGE_CARDS_VIEW);
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
