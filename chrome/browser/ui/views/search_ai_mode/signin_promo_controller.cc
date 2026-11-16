// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_view.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
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
  CHECK(!is_pending_promo_);
  CHECK(browser_view);
  CHECK(web_contents_);

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!CanShowPromo(*profile)) {
    // This may result in destroying the caller and this object.
    OnPromoIneligible();
    return false;
  }
  if (web_contents_->GetVisibility() == content::Visibility::HIDDEN) {
    // This may result in destroying the caller and this object.
    OnPromoDismissedOrAborted();
    return false;
  }

  AvatarToolbarButtonInterface* avatar_button =
      browser_view->toolbar_button_provider()
          ->GetAvatarToolbarButtonInterface();
  CHECK(avatar_button);

  // Set any explicit avatar pill state before `RunWhenVisible()` so the button
  // pops out and lays out at its expanded width before its anchor is resolved.
  avatar_button_state_runner_ = UpdateAvatarButtonState(*avatar_button);

  base::WeakPtr<AIModeSignInPromoControllerBase> weak_this = GetBaseWeakPtr();
  is_pending_promo_ = true;
  avatar_button->RunWhenVisible(base::BindOnce(
      &AIModeSignInPromoControllerBase::ShowPromoWithAnchor, weak_this,
      browser_view->GetAsWeakPtr(),
      web_contents_->GetPrimaryMainFrame()->GetWeakDocumentPtr()));
  return weak_this && (is_pending_promo_ || promo_view_ != nullptr);
}

base::ScopedClosureRunner
AIModeSignInPromoControllerBase::UpdateAvatarButtonState(
    AvatarToolbarButtonInterface& avatar_button) {
  return base::ScopedClosureRunner();
}

void AIModeSignInPromoControllerBase::ShowPromoWithAnchor(
    base::WeakPtr<BrowserView> browser_view,
    content::WeakDocumentPtr weak_document,
    views::BubbleAnchor anchor,
    base::ScopedClosureRunner keep_visible) {
  is_pending_promo_ = false;
  tabs::TabInterface* tab =
      web_contents_
          ? tabs::TabInterface::MaybeGetFromContents(web_contents_.get())
          : nullptr;
  if (!browser_view || !web_contents_ ||
      weak_document.AsRenderFrameHostIfValid() !=
          web_contents_->GetPrimaryMainFrame() ||
      (tab && tab->GetBrowserWindowInterface() != browser_view->browser()) ||
      web_contents_->GetVisibility() == content::Visibility::HIDDEN ||
      !CanShowPromo(
          *Profile::FromBrowserContext(web_contents_->GetBrowserContext())) ||
      anchor.IsNull()) {
    keep_visible.RunAndReset();
    avatar_button_state_runner_.RunAndReset();
    // This may result in destroying the caller and this object.
    OnPromoDismissedOrAborted();
    return;
  }

  auto promo_view = CreatePromoView(anchor);
  promo_view->SetKeepVisible(std::move(keep_visible));
  promo_view_ = promo_view.get();

  views::BubbleDialogDelegateView::CreateBubble(std::move(promo_view));
  promo_view_->ShowForReason(LocationBarBubbleDelegateView::USER_GESTURE);
}

void AIModeSignInPromoControllerBase::OnViewIsDeleting() {
  promo_view_ = nullptr;
  avatar_button_state_runner_.RunAndReset();
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

base::WeakPtr<AIModeSignInPromoControllerBase>
SearchAIModeSignInPromoController::GetBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool SearchAIModeSignInPromoController::CanShowPromo(Profile& profile) {
  CHECK(base::FeatureList::IsEnabled(switches::kEnableSearchAIModeSigninPromo));
  return signin::ShouldShowSearchAIModeSignInPromo(profile);
}

base::ScopedClosureRunner
SearchAIModeSignInPromoController::UpdateAvatarButtonState(
    AvatarToolbarButtonInterface& avatar_button) {
  return avatar_button.SetExplicitButtonState(
      l10n_util::GetStringUTF16(IDS_AI_SIGNIN_PROMO_AVATAR_PILL_TEXT),
      // TODO(crbug.com/486858498): Check if an A11y label is needed.
      /*accessibility_label=*/std::nullopt, /*explicit_action=*/std::nullopt);
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

base::WeakPtr<AIModeSignInPromoControllerBase>
ComposeboxDriveSignInPromoController::GetBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

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
