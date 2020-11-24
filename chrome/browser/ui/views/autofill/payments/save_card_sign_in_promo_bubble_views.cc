// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_card_sign_in_promo_bubble_views.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#endif

namespace autofill {

SaveCardSignInPromoBubbleViews::SaveCardSignInPromoBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveCardBubbleController* controller)
    : SaveCardBubbleViews(anchor_view, web_contents, controller) {
  // The BubbleSyncPromoView takes care of buttons.
  SetButtons(ui::DIALOG_BUTTON_NONE);
}

SaveCardSignInPromoBubbleViews::~SaveCardSignInPromoBubbleViews() = default;

std::unique_ptr<views::View>
SaveCardSignInPromoBubbleViews::CreateMainContentView() {
  auto view = std::make_unique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  view->SetID(DialogViewId::SIGN_IN_PROMO_VIEW);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  sync_promo_delegate_ =
      std::make_unique<SaveCardSignInPromoBubbleViews::SyncPromoDelegate>(
          controller(),
          signin_metrics::AccessPoint::ACCESS_POINT_SAVE_CARD_BUBBLE);
  std::unique_ptr<views::View> signin_view =
      std::make_unique<DiceBubbleSyncPromoView>(
          controller()->GetProfile(), sync_promo_delegate_.get(),
          signin_metrics::AccessPoint::ACCESS_POINT_SAVE_CARD_BUBBLE,
          /*dice_accounts_promo_message_resource_id=*/0,
          /*dice_signin_button_prominent=*/true);
  signin_view->SetID(DialogViewId::SIGN_IN_VIEW);
  view->AddChildView(signin_view.release());
#endif

  return view;
}

}  // namespace autofill
