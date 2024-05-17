// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_controller.h"

#include "base/no_destructor.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_disclaimer_view.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace chromeos {

namespace {

MagicBoostController* g_magic_boost_controller_for_testing = nullptr;

}  // namespace

// static
MagicBoostController* MagicBoostController::Get() {
  if (g_magic_boost_controller_for_testing) {
    return g_magic_boost_controller_for_testing;
  }
  static base::NoDestructor<MagicBoostController> instance;
  return instance.get();
}

MagicBoostController::MagicBoostController() = default;

MagicBoostController::~MagicBoostController() = default;

void MagicBoostController::ShowOptInUi(const gfx::Rect& anchor_view_bounds) {
  CHECK(!opt_in_widget_);
  CHECK(!disclaimer_widget_);
  opt_in_widget_ = MagicBoostOptInCard::CreateWidget(anchor_view_bounds);
  opt_in_widget_->Show();
}

void MagicBoostController::CloseOptInUi() {
  opt_in_widget_.reset();
}

void MagicBoostController::ShowDisclaimerUi() {
  if (disclaimer_widget_) {
    return;
  }
  disclaimer_widget_ = MagicBoostDisclaimerView::CreateWidget();
  disclaimer_widget_->Show();
}

bool MagicBoostController::ShouldQuickAnswersAndMahiShowOptIn() {
  // TODO(b/339043693): Implement this function.
  return false;
}

void MagicBoostController::SetAllFeaturesState(bool enabled) {
  SetQuickAnswersAndMahiFeaturesState(enabled);
  SetOrcaFeatureState(enabled);
}

void MagicBoostController::SetQuickAnswersAndMahiFeaturesState(bool enabled) {
  // TODO(b/339043693): Implement this function.
}

ScopedMagicBoostControllerForTesting::ScopedMagicBoostControllerForTesting(
    MagicBoostController* controller_for_testing) {
  g_magic_boost_controller_for_testing = controller_for_testing;
}

ScopedMagicBoostControllerForTesting::~ScopedMagicBoostControllerForTesting() {
  g_magic_boost_controller_for_testing = nullptr;
}

}  // namespace chromeos
