// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_view.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_class_properties.h"

AIModeSignInPromoControllerBase::AIModeSignInPromoControllerBase(
    content::WebContents* web_contents,
    signin_metrics::AccessPoint access_point)
    : web_contents_(web_contents->GetWeakPtr()), access_point_(access_point) {}

AIModeSignInPromoControllerBase::~AIModeSignInPromoControllerBase() = default;

bool AIModeSignInPromoControllerBase::MaybeShowPromo(
    BrowserView* browser_view) {
  CHECK(!promo_view_);
  CHECK(browser_view);
  CHECK(web_contents_);

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!CanShowPromo(*profile)) {
    // This may result in destroying the caller and this object.
    OnPromoIneligible();
    return false;
  }

  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();
  CHECK(avatar_button);

  UpdateAvatarButtonState(*avatar_button);

  auto promo_view =
      CreatePromoView(avatar_button->GetBubbleAnchor(*browser_view->browser()));
  promo_view_ = promo_view.get();

  views::BubbleDialogDelegateView::CreateBubble(std::move(promo_view));
  promo_view_->ShowForReason(LocationBarBubbleDelegateView::USER_GESTURE);
  return true;
}

void AIModeSignInPromoControllerBase::OnViewIsDeleting() {
  promo_view_ = nullptr;
  OnViewDeleted();
}

void AIModeSignInPromoControllerBase::HandlePromoClosing(
    views::Widget::ClosedReason closed_reason) {
  if (closed_reason != views::Widget::ClosedReason::kAcceptButtonClicked) {
    // This may result in destroying the caller and this object.
    OnPromoDismissedOrAborted();
  }
}

// SearchAIModeSignInPromoController -------------------------------------------

SearchAIModeSignInPromoController::SearchAIModeSignInPromoController(
    content::WebContents* web_contents)
    : AIModeSignInPromoControllerBase(
          web_contents,
          signin_metrics::AccessPoint::kSearchAIModeBubble) {}

SearchAIModeSignInPromoController::~SearchAIModeSignInPromoController() =
    default;

void SearchAIModeSignInPromoController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SearchAIModeSignInPromoController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool SearchAIModeSignInPromoController::CanShowPromo(Profile& profile) {
  CHECK(base::FeatureList::IsEnabled(switches::kEnableSearchAIModeSigninPromo));
  return signin::ShouldShowSearchAIModeSignInPromo(profile);
}

void SearchAIModeSignInPromoController::UpdateAvatarButtonState(
    AvatarToolbarButtonInterface& avatar_button) {
  avatar_pill_closure_runner_ = avatar_button.SetExplicitButtonState(
      l10n_util::GetStringUTF16(IDS_AI_SIGNIN_PROMO_AVATAR_PILL_TEXT),
      // TODO(crbug.com/486858498): Check if an A11y label is needed.
      /*accessibility_label=*/std::nullopt, /*explicit_action=*/std::nullopt);
}

void SearchAIModeSignInPromoController::OnViewDeleted() {
  avatar_pill_closure_runner_.RunAndReset();
}

void SearchAIModeSignInPromoController::OnPromoIneligible() {
  observers_.Notify(&Observer::OnFlowAborted);
}

void SearchAIModeSignInPromoController::OnPromoDismissedOrAborted() {
  observers_.Notify(&Observer::OnFlowAborted);
}

std::unique_ptr<AIModeSignInPromoViewBase>
SearchAIModeSignInPromoController::CreatePromoView(views::BubbleAnchor anchor) {
  return std::make_unique<SearchAIModeSignInPromoView>(anchor, web_contents(),
                                                       GetWeakPtr());
}

// ComposeboxDriveSignInPromoController ---------------------------------------

ComposeboxDriveSignInPromoController::ComposeboxDriveSignInPromoController(
    content::WebContents* web_contents)
    : AIModeSignInPromoControllerBase(
          web_contents,
          signin_metrics::AccessPoint::
              kComposeboxDriveContextMenuOptionBubble) {}

ComposeboxDriveSignInPromoController::~ComposeboxDriveSignInPromoController() =
    default;

bool ComposeboxDriveSignInPromoController::CanShowPromo(Profile& profile) {
  CHECK(base::FeatureList::IsEnabled(
      omnibox::kComposeboxDriveContextMenuOptionSigninPromo));
  return signin::ShouldShowComposeboxDriveContextMenuOptionSignInPromo(profile);
}

bool ComposeboxDriveSignInPromoController::MaybeShowPromo(
    BrowserWindowInterface* browser_window_interface) {
  if (!browser_window_interface) {
    return false;
  }
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_window_interface);
  if (!browser_view) {
    return false;
  }
  return AIModeSignInPromoControllerBase::MaybeShowPromo(browser_view);
}

std::unique_ptr<AIModeSignInPromoViewBase>
ComposeboxDriveSignInPromoController::CreatePromoView(
    views::BubbleAnchor anchor) {
  return std::make_unique<ComposeboxDriveSignInPromoView>(
      anchor, web_contents(), GetWeakPtr());
}
