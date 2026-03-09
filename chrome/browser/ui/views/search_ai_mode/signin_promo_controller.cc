// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

SearchAIModeSignInPromoController::SearchAIModeSignInPromoController(
    content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()) {}

SearchAIModeSignInPromoController::~SearchAIModeSignInPromoController() =
    default;

void SearchAIModeSignInPromoController::ShowPromo(BrowserView* browser_view) {
  // TODO(crbug.com/486858498): Implement a `ShouldShowSearchAIModeSignInPromo`
  // method to check conditions for showing the promo based on rate limits and
  // other criteria. Only if it return true invoke the present method.
  CHECK(base::FeatureList::IsEnabled(switches::kEnableSearchAIModeSigninPromo));
  if (promo_view_) {
    return;
  }
  if (!browser_view) {
    return;
  }

  AvatarToolbarButton* avatar_button =
      browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
  if (!avatar_button) {
    return;
  }

  avatar_pill_closure_runner_ = avatar_button->SetExplicitButtonState(
      l10n_util::GetStringUTF16(IDS_AI_SIGNIN_PROMO_AVATAR_PILL_TEXT),
      // TODO(crbug.com/486858498): Check if an A11y label is needed.
      /*accessibility_label=*/std::nullopt, /*explicit_action=*/std::nullopt);

  auto promo_view = std::make_unique<SearchAIModeSignInPromoView>(
      avatar_button, web_contents_.get(), GetWeakPtr());
  promo_view_ = promo_view.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(promo_view));
  // TODO(crbug.com/486858498): When we add the invoking flow we should decide
  // which `DisplayReason` is more appropriate. AUTOMATIC seems a better fit
  // because this bubble might be shown while navigations and page loads on the
  // linked web_contents_ can happen, which make take the focus from the bubble
  // and prevent it from showing.
  // Check the A11y impact on focusing an screen reader announcements.
  promo_view_->ShowForReason(LocationBarBubbleDelegateView::AUTOMATIC);
}

void SearchAIModeSignInPromoController::OnBubbleClosed() {
  promo_view_ = nullptr;
  avatar_pill_closure_runner_.RunAndReset();
}
