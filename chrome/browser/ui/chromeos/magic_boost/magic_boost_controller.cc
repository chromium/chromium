// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_controller.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/mahi/mahi_prefs_controller.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_disclaimer_view.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"
#include "ui/views/widget/unique_widget_ptr.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/mahi/mahi_prefs_controller_ash.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/mahi/mahi_prefs_controller_lacros.h"
#endif

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

MagicBoostController::MagicBoostController() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  mahi_prefs_controller_ = std::make_unique<mahi::MahiPrefsControllerAsh>();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  mahi_prefs_controller_ = std::make_unique<mahi::MahiPrefsControllerLacros>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

MagicBoostController::~MagicBoostController() = default;

void MagicBoostController::ShowOptInUi(const gfx::Rect& anchor_view_bounds) {
  CHECK(!opt_in_widget_);
  CHECK(!disclaimer_widget_);
  opt_in_widget_ =
      MagicBoostOptInCard::CreateWidget(anchor_view_bounds, is_orca_included_);
  opt_in_widget_->ShowInactive();
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

void MagicBoostController::CloseDisclaimerUi() {
  disclaimer_widget_.reset();
}

bool MagicBoostController::ShouldQuickAnswersAndMahiShowOptIn() {
  // TODO(b/341485303): Check for Magic Boost consent status.
  return true;
}

void MagicBoostController::SetAllFeaturesState(bool enabled) {
  SetQuickAnswersAndMahiFeaturesState(enabled);
  SetOrcaFeatureState(enabled);
}

void MagicBoostController::SetQuickAnswersAndMahiFeaturesState(bool enabled) {
  mahi_prefs_controller_->SetMahiEnabled(enabled);

  // TODO(b/339043693): Enable/disable Quick Answers.
}

void MagicBoostController::SetIsOrcaIncludedForTest(bool include) {
  is_orca_included_ = include;
}

ScopedMagicBoostControllerForTesting::ScopedMagicBoostControllerForTesting(
    MagicBoostController* controller_for_testing) {
  g_magic_boost_controller_for_testing = controller_for_testing;
}

ScopedMagicBoostControllerForTesting::~ScopedMagicBoostControllerForTesting() {
  g_magic_boost_controller_for_testing = nullptr;
}

}  // namespace chromeos
