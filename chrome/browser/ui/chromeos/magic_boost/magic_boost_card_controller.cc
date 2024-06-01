// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"

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

MagicBoostCardController* g_magic_boost_opt_in_handler_for_testing = nullptr;

}  // namespace

// static
MagicBoostCardController* MagicBoostCardController::Get() {
  if (g_magic_boost_opt_in_handler_for_testing) {
    return g_magic_boost_opt_in_handler_for_testing;
  }
  static base::NoDestructor<MagicBoostCardController> instance;
  return instance.get();
}

MagicBoostCardController::MagicBoostCardController() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  mahi_prefs_controller_ = std::make_unique<mahi::MahiPrefsControllerAsh>();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  mahi_prefs_controller_ = std::make_unique<mahi::MahiPrefsControllerLacros>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

MagicBoostCardController::~MagicBoostCardController() = default;

void MagicBoostCardController::ShowOptInUi(
    const gfx::Rect& anchor_view_bounds) {
  CHECK(!opt_in_widget_);
  CHECK(!disclaimer_widget_);
  opt_in_widget_ =
      MagicBoostOptInCard::CreateWidget(anchor_view_bounds, is_orca_included_);
  opt_in_widget_->ShowInactive();
}

void MagicBoostCardController::CloseOptInUi() {
  opt_in_widget_.reset();
}

void MagicBoostCardController::ShowDisclaimerUi() {
  if (disclaimer_widget_) {
    return;
  }
  disclaimer_widget_ = MagicBoostDisclaimerView::CreateWidget();
  disclaimer_widget_->Show();
}

void MagicBoostCardController::CloseDisclaimerUi() {
  disclaimer_widget_.reset();
}

bool MagicBoostCardController::ShouldQuickAnswersAndMahiShowOptIn() {
  // TODO(b/341485303): Check for Magic Boost consent status.
  return false;
}

void MagicBoostCardController::SetAllFeaturesState(bool enabled) {
  SetQuickAnswersAndMahiFeaturesState(enabled);
  SetOrcaFeatureState(enabled);
}

void MagicBoostCardController::SetQuickAnswersAndMahiFeaturesState(
    bool enabled) {
  mahi_prefs_controller_->SetMahiEnabled(enabled);

  // TODO(b/339043693): Enable/disable Quick Answers.
}

void MagicBoostCardController::SetIsOrcaIncludedForTest(bool include) {
  is_orca_included_ = include;
}

ScopedMagicBoostCardControllerForTesting::
    ScopedMagicBoostCardControllerForTesting(
        MagicBoostCardController* controller_for_testing) {
  g_magic_boost_opt_in_handler_for_testing = controller_for_testing;
}

ScopedMagicBoostCardControllerForTesting::
    ~ScopedMagicBoostCardControllerForTesting() {
  g_magic_boost_opt_in_handler_for_testing = nullptr;
}

}  // namespace chromeos
