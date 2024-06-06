// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/mahi/mahi_prefs_controller.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_disclaimer_view.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "ui/views/widget/unique_widget_ptr.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/check_deref.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"
#include "chrome/browser/chromeos/mahi/mahi_prefs_controller_ash.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/mahi/mahi_prefs_controller_lacros.h"
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

namespace chromeos {

namespace {

MagicBoostCardController* g_magic_boost_opt_in_handler_for_testing = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)

crosapi::mojom::MagicBoostController* g_crosapi_instance_for_testing = nullptr;

crosapi::mojom::MagicBoostController& GetMagicBoostControllerAsh() {
  if (g_crosapi_instance_for_testing) {
    return CHECK_DEREF(g_crosapi_instance_for_testing);
  }
  return CHECK_DEREF(crosapi::CrosapiManager::Get()
                         ->crosapi_ash()
                         ->magic_boost_controller_ash());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

  // Bind remote and pass receiver to `MagicBoostController`.
  chromeos::LacrosService::Get()->BindMagicBoostController(
      remote_.BindNewPipeAndPassReceiver());
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

void MagicBoostCardController::ShowDisclaimerUi(
    int64_t display_id,
    crosapi::mojom::MagicBoostController::TransitionAction action) {
  // TODO(b/319735347): Add integration tests to make sure that this function
  // always goes through the crosapi.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  remote_->ShowDisclaimerUi(display_id, action);
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  GetMagicBoostControllerAsh().ShowDisclaimerUi(display_id, action);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // TODO(b/341832244): Move these logic to
  // `MagicBoostControllerAsh::ShowDisclaimerUi`.
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void MagicBoostCardController::BindMagicBoostControllerCrosapiForTesting(
    mojo::PendingRemote<crosapi::mojom::MagicBoostController> pending_remote) {
  remote_.reset();
  remote_.Bind(std::move(pending_remote));
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void MagicBoostCardController::SetMagicBoostControllerCrosapiForTesting(
    crosapi::mojom::MagicBoostController* delegate) {
  g_crosapi_instance_for_testing = delegate;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
