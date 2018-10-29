// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_card_sign_in_promo_bubble_views.h"

#include <memory>

#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/sync/bubble_sync_promo_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#endif

namespace autofill {

SaveCardSignInPromoBubbleViews::SaveCardSignInPromoBubbleViews(
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    content::WebContents* web_contents,
    SaveCardBubbleController* controller)
    : SaveCardBubbleViews(anchor_view, anchor_point, web_contents, controller) {
}

int SaveCardSignInPromoBubbleViews::GetDialogButtons() const {
  // The BubbleSyncPromoView takes care of buttons.
  return ui::DIALOG_BUTTON_NONE;
}

SaveCardSignInPromoBubbleViews::~SaveCardSignInPromoBubbleViews() = default;

std::unique_ptr<views::View>
SaveCardSignInPromoBubbleViews::CreateMainContentView() {
  auto view = std::make_unique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  view->set_id(DialogViewId::SIGN_IN_PROMO_VIEW);

  std::unique_ptr<views::View> signin_view;
  Profile* profile = controller()->GetProfile();
  sync_promo_delegate_ =
      std::make_unique<SaveCardSignInPromoBubbleViews::SyncPromoDelegate>(
          controller(),
          signin_metrics::AccessPoint::ACCESS_POINT_SAVE_CARD_BUBBLE);
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    signin_view = std::make_unique<DiceBubbleSyncPromoView>(
        profile, sync_promo_delegate_.get(),
        signin_metrics::AccessPoint::ACCESS_POINT_SAVE_CARD_BUBBLE);
#else
    NOTREACHED();
#endif
  } else {
    signin_view = std::make_unique<BubbleSyncPromoView>(
        sync_promo_delegate_.get(),
        signin_metrics::AccessPoint::ACCESS_POINT_SAVE_CARD_BUBBLE,
        IDS_AUTOFILL_SIGNIN_PROMO_LINK_DICE_DISABLED,
        IDS_AUTOFILL_SIGNIN_PROMO_MESSAGE_DICE_DISABLED);
  }
  signin_view->set_id(DialogViewId::SIGN_IN_VIEW);
  view->AddChildView(signin_view.release());
  return view;
}

}  // namespace autofill
