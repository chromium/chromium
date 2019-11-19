// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/feature_promos/reopen_tab_promo_controller.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/timer/timer.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/in_product_help/in_product_help.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help_factory.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_timeout.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace {

constexpr char kReopenTabPromoDismissedAtHistogram[] =
    "InProductHelp.Promos.IPH_ReopenTab.DismissedAt";

}  // namespace

ReopenTabPromoController::ReopenTabPromoController(BrowserView* browser_view)
    : iph_service_(ReopenTabInProductHelpFactory::GetForProfile(
          browser_view->browser()->profile())),
      browser_view_(browser_view) {
}

void ReopenTabPromoController::ShowPromo() {
  // This shouldn't be called more than once. Check that state is fresh.
  DCHECK(!show_promo_called_);
  show_promo_called_ = true;

  DCHECK(!is_showing_);
  is_showing_ = true;

  // Here, we start the promo display. We highlight the app menu button and open
  // the promo bubble.
  auto* app_menu_button = browser_view_->toolbar()->app_menu_button();
  app_menu_button->AddObserver(this);
  app_menu_button->SetPromoFeature(InProductHelpFeature::kReopenTab);

  // Get keyboard shortcut for reopening last closed tab. This should exist.
  ui::Accelerator accelerator;
  bool has_accelerator =
      browser_view_->GetAccelerator(IDC_RESTORE_TAB, &accelerator);
  DCHECK(has_accelerator);

  auto feature_promo_bubble_timeout =
      std::make_unique<FeaturePromoBubbleTimeout>(base::TimeDelta(),
                                                  base::TimeDelta());
  promo_bubble_ =
      disable_bubble_timeout_for_test_
          ? FeaturePromoBubbleView::CreateOwned(
                app_menu_button, views::BubbleBorder::Arrow::TOP_RIGHT,
                FeaturePromoBubbleView::ActivationAction::DO_NOT_ACTIVATE,
                IDS_REOPEN_TAB_PROMO, IDS_REOPEN_TAB_PROMO_SCREENREADER,
                accelerator, std::move(feature_promo_bubble_timeout))
          : FeaturePromoBubbleView::CreateOwned(
                app_menu_button, views::BubbleBorder::Arrow::TOP_RIGHT,
                FeaturePromoBubbleView::ActivationAction::DO_NOT_ACTIVATE,
                IDS_REOPEN_TAB_PROMO, IDS_REOPEN_TAB_PROMO_SCREENREADER,
                accelerator);
  promo_bubble_->set_close_on_deactivate(false);
  promo_bubble_->GetWidget()->AddObserver(this);
}

void ReopenTabPromoController::OnTabReopened(int command_id) {
  iph_service_->TabReopened();

  if (!is_showing_)
    return;
  if (command_id != AppMenuModel::kMinRecentTabsCommandId &&
      command_id != IDC_RESTORE_TAB)
    return;

  promo_step_ = StepAtDismissal::kTabReopened;
  if (command_id == IDC_RESTORE_TAB) {
    // If using the keyboard shortcut, we bypass the other steps and so we close
    // the bubble now.
    if (promo_bubble_)
      promo_bubble_->GetWidget()->Close();
    PromoEnded();
  }
}

void ReopenTabPromoController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(promo_bubble_);
  promo_bubble_ = nullptr;

  // If we haven't progressed past |StepAtDismissal::kBubbleShown|, the bubble
  // timed out without the user following our IPH. End it.
  if (promo_step_ == StepAtDismissal::kBubbleShown)
    PromoEnded();
}

void ReopenTabPromoController::AppMenuShown() {
  promo_step_ = StepAtDismissal::kMenuOpened;

  // Close the promo bubble since it doesn't automatically close on click.
  promo_bubble_->GetWidget()->Close();

  // Stop showing promo on app menu button.
  browser_view_->toolbar()->app_menu_button()->SetPromoFeature(base::nullopt);
}

void ReopenTabPromoController::AppMenuClosed() {
  PromoEnded();
}

void ReopenTabPromoController::PromoEnded() {
  UMA_HISTOGRAM_ENUMERATION(kReopenTabPromoDismissedAtHistogram, promo_step_);

  // We notify the service regardless of whether IPH succeeded. Success is
  // determined by whether the reopen tab event was sent.
  iph_service_->HelpDismissed();

  auto* app_menu_button = browser_view_->toolbar()->app_menu_button();
  app_menu_button->SetPromoFeature(base::nullopt);
  app_menu_button->RemoveObserver(this);

  is_showing_ = false;
}
