// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_card_manage_cards_bubble_views.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#endif

namespace autofill {

SaveCardManageCardsBubbleViews::SaveCardManageCardsBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveCardBubbleController* controller)
    : SaveCardBubbleViews(anchor_view, web_contents, controller) {
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetExtraView(std::make_unique<views::MdTextButton>(
                   base::BindRepeating(
                       [](SaveCardManageCardsBubbleViews* bubble) {
                         bubble->controller()->OnManageCardsClicked();
                         bubble->CloseBubble();
                       },
                       base::Unretained(this)),
                   l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_CARDS)))
      ->SetID(autofill::DialogViewId::MANAGE_CARDS_BUTTON);
  SetFootnoteView(CreateSigninPromoView());
}

std::unique_ptr<views::View>
SaveCardManageCardsBubbleViews::CreateMainContentView() {
  std::unique_ptr<views::View> view =
      SaveCardBubbleViews::CreateMainContentView();
  view->SetID(DialogViewId::MANAGE_CARDS_VIEW);
  return view;
}

std::unique_ptr<views::View>
SaveCardManageCardsBubbleViews::CreateSigninPromoView() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS does not show the signin promo.
  return nullptr;
#else
  if (!controller()->ShouldShowSignInPromo())
    return nullptr;
  sync_promo_delegate_ =
      std::make_unique<SaveCardManageCardsBubbleViews::SyncPromoDelegate>(
          controller(),
          signin_metrics::AccessPoint::ACCESS_POINT_MANAGE_CARDS_BUBBLE);
  std::unique_ptr<views::View> promo_view =
      std::make_unique<DiceBubbleSyncPromoView>(
          controller()->GetProfile(), sync_promo_delegate_.get(),
          signin_metrics::AccessPoint::ACCESS_POINT_MANAGE_CARDS_BUBBLE,
          IDS_AUTOFILL_SYNC_PROMO_MESSAGE,
          /*dice_signin_button_prominent=*/false,
          views::style::STYLE_SECONDARY);
  DCHECK(promo_view);
  InitFootnoteView(promo_view.get());
  return promo_view;
#endif
}

SaveCardManageCardsBubbleViews::~SaveCardManageCardsBubbleViews() = default;

}  // namespace autofill
