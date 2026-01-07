// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/magic_boost/magic_boost_card_controller.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/magic_boost/magic_boost_controller.h"
#include "chrome/browser/ui/ash/magic_boost/magic_boost_constants.h"
#include "chrome/browser/ui/ash/magic_boost/magic_boost_metrics.h"
#include "chrome/browser/ui/ash/magic_boost/magic_boost_opt_in_card.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace chromeos {

namespace {

ash::MagicBoostController* g_instance_for_testing = nullptr;

ash::MagicBoostController& GetMagicBoostController() {
  if (g_instance_for_testing) {
    CHECK_IS_TEST();
    return CHECK_DEREF(g_instance_for_testing);
  }
  return CHECK_DEREF(ash::MagicBoostController::Get());
}

}  // namespace

MagicBoostCardController::MagicBoostCardController(
    const ApplicationLocaleStorage* application_locale_storage)
    : application_locale_storage_(CHECK_DEREF(application_locale_storage)) {
  // `MahiMediaAppEventsProxy` might not be available in tests.
  if (chromeos::MahiMediaAppEventsProxy::Get()) {
    chromeos::MahiMediaAppEventsProxy::Get()->AddObserver(this);
  }
}

MagicBoostCardController::~MagicBoostCardController() {
  if (chromeos::MahiMediaAppEventsProxy::Get()) {
    chromeos::MahiMediaAppEventsProxy::Get()->RemoveObserver(this);
  }
}

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
    views::AsViewClass<MagicBoostOptInCard>(opt_in_widget_->GetContentsView())
        ->OnAnchorMenuDismissed();

    opt_in_widget_.reset();
  }
}

void MagicBoostCardController::OnPdfContextMenuShown(const gfx::Rect& anchor) {
  auto* magic_boost_state = MagicBoostState::Get();

  if (magic_boost_state->ShouldShowHmrCard()) {
    return;
  }

  magic_boost_state->ShouldIncludeOrcaInOptIn(base::BindOnce(
      [](base::WeakPtr<MagicBoostCardController> controller,
         const gfx::Rect& anchor, bool should_include_orca) {
        if (!controller) {
          return;
        }

        controller->SetOptInFeature(
            should_include_orca ? ash::magic_boost::OptInFeatures::kOrcaAndHmr
                                : ash::magic_boost::OptInFeatures::kHmrOnly);
        controller->ShowOptInUi(/*anchor_view_bounds=*/anchor);
      },
      weak_factory_.GetWeakPtr(), anchor));
}

void MagicBoostCardController::OnPdfContextMenuHide() {
  OnDismiss(/*is_other_command_executed=*/false);
}

void MagicBoostCardController::ShowOptInUi(
    const gfx::Rect& anchor_view_bounds) {
  if (opt_in_widget_) {
    opt_in_widget_.reset();
  }

  // If the disclaimer view is showing, close it.
  CloseDisclaimerUi();

  opt_in_widget_ = MagicBoostOptInCard::CreateWidget(
      &application_locale_storage_.get(),
      /*controller=*/this, anchor_view_bounds);
  opt_in_widget_->ShowInactive();

  magic_boost::RecordOptInCardActionMetrics(
      opt_in_features_, magic_boost::OptInCardAction::kShowCard);
}

void MagicBoostCardController::CloseOptInUi() {
  opt_in_widget_.reset();
}

void MagicBoostCardController::ShowDisclaimerUi(int64_t display_id) {
  GetMagicBoostController().ShowDisclaimerUi(display_id, transition_action_,
                                             opt_in_features_);
}

void MagicBoostCardController::CloseDisclaimerUi() {
  GetMagicBoostController().CloseDisclaimerUi();
}

void MagicBoostCardController::SetOptInFeature(
    const ash::magic_boost::OptInFeatures& features) {
  opt_in_features_ = features;
}

const ash::magic_boost::OptInFeatures&
MagicBoostCardController::GetOptInFeatures() const {
  return opt_in_features_;
}

base::WeakPtr<MagicBoostCardController> MagicBoostCardController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MagicBoostCardController::SetMagicBoostControllerForTesting(
    ash::MagicBoostController* delegate) {
  g_instance_for_testing = delegate;
}

}  // namespace chromeos
