// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/mahi/mahi_prefs_controller.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "ui/views/view_utils.h"
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

void MagicBoostCardController::OnContextMenuShown(Profile* profile) {}

void MagicBoostCardController::OnTextAvailable(
    const gfx::Rect& anchor_bounds,
    const std::string& selected_text,
    const std::string& surrounding_text) {
  ShowOptInUi(/*anchor_view_bounds=*/anchor_bounds);
}

void MagicBoostCardController::OnAnchorBoundsChanged(
    const gfx::Rect& anchor_bounds) {
  if (!opt_in_widget_ || !opt_in_widget_->GetContentsView()) {
    return;
  }

  views::AsViewClass<MagicBoostOptInCard>(opt_in_widget_->GetContentsView())
      ->UpdateWidgetBounds(/*anchor_view_bounds=*/anchor_bounds);
}

void MagicBoostCardController::OnDismiss(bool is_other_command_executed) {
  // If context menu is dismissed and the opt-in widget is active (i.e. keyboard
  // focus is on a button), we should not close the widget.
  if (opt_in_widget_ && !opt_in_widget_->IsActive()) {
    opt_in_widget_.reset();
  }
}

void MagicBoostCardController::ShowOptInUi(
    const gfx::Rect& anchor_view_bounds) {
  CHECK(!opt_in_widget_);

  // If the disclaimer view is showing, close it.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  remote_->CloseDisclaimerUi();
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  GetMagicBoostControllerAsh().CloseDisclaimerUi();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  opt_in_widget_ = MagicBoostOptInCard::CreateWidget(
      /*controller=*/this, anchor_view_bounds, is_orca_included_);
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

}  // namespace chromeos
