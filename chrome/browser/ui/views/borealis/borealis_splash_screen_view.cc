// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_splash_screen_view.h"

#include "ash/public/cpp/window_properties.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/layout/box_layout.h"

namespace {

borealis::BorealisSplashScreenView* delegate_ = nullptr;

}

namespace borealis {

// Declared in chrome/browser/ash/borealis/borealis_util.h.
void ShowBorealisSplashScreenView(Profile* profile) {
  return BorealisSplashScreenView::Show(profile);
}

void CloseBorealisSplashScreenView() {
  if (delegate_) {
    delegate_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }
}

void BorealisSplashScreenView::Show(Profile* profile) {
  if (!delegate_) {
    auto delegate = std::make_unique<BorealisSplashScreenView>(profile);
    delegate_ = delegate.get();
    views::DialogDelegate::CreateDialogWidget(std::move(delegate), nullptr,
                                              nullptr);
    delegate_->GetWidget()->GetNativeWindow()->SetProperty(
        ash::kShelfIDKey, ash::ShelfID(borealis::kBorealisAppId).Serialize());
  }
  delegate_->GetWidget()->Show();
}

BorealisSplashScreenView::BorealisSplashScreenView(Profile* profile) {
  profile_ = profile;
  borealis::BorealisService::GetForProfile(profile_)
      ->WindowManager()
      .AddObserver(this);

  SetShowCloseButton(false);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::TEXT, views::DialogContentType::TEXT));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_close_on_deactivate(false);

  // TODO: b/174589567 Make splash screen look like mockups.
  views::MessageBoxView* message_box =
      new views::MessageBoxView(u"BOREALIS IS STARTING... PLEASE WAIT");
  AddChildView(message_box);
}

void BorealisSplashScreenView::OnSessionStarted() {
  DCHECK(GetWidget() != nullptr);
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void BorealisSplashScreenView::OnWindowManagerDeleted(
    borealis::BorealisWindowManager* window_manager) {
  DCHECK(window_manager ==
         &borealis::BorealisService::GetForProfile(profile_)->WindowManager());
  window_manager->RemoveObserver(this);
}

BorealisSplashScreenView::~BorealisSplashScreenView() {
  if (profile_)
    borealis::BorealisService::GetForProfile(profile_)
        ->WindowManager()
        .RemoveObserver(this);
  delegate_ = nullptr;
}

BorealisSplashScreenView* BorealisSplashScreenView::GetActiveViewForTesting() {
  return delegate_;
}

}  // namespace borealis
