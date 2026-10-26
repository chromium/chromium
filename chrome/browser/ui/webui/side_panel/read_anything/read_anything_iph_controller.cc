// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_iph_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/accessibility/accessibility_features.h"

namespace {

using DistillationState = ReadAnythingController::DistillationState;
using PresentationState = ReadAnythingController::PresentationState;

// How long Reading Mode must show content before showing the AI playback IPH.
base::TimeDelta g_ai_playback_iph_delay = base::Seconds(30);

BrowserUserEducationInterface* GetUserEducation(
    ReadAnythingController* controller) {
  tabs::TabInterface* tab = controller ? controller->tab() : nullptr;
  return tab ? BrowserUserEducationInterface::From(
                   tab->GetBrowserWindowInterface())
             : nullptr;
}

}  // namespace

ReadAnythingIphController::ReadAnythingIphController(
    ReadAnythingController* read_anything_controller)
    : read_anything_controller_(read_anything_controller) {
  CHECK(read_anything_controller_);
  read_anything_controller_observer_.Observe(read_anything_controller_);
}

ReadAnythingIphController::~ReadAnythingIphController() = default;

// static
base::AutoReset<base::TimeDelta>
ReadAnythingIphController::SetAiPlaybackIphDelayForTesting(
    base::TimeDelta delay) {
  return {&g_ai_playback_iph_delay, delay};
}

void ReadAnythingIphController::OnDistillationStateChanged(
    DistillationState new_state) {
  if (!read_anything_controller_) {
    return;
  }

  // Store the state before any early return so that Activate() can use it if
  // the content arrives while Reading Mode is closed.
  distillation_state_ = new_state;

  if (new_state == DistillationState::kDistillationEmpty) {
    ai_playback_iph_timer_.Stop();
    auto* user_ed = GetUserEducation(read_anything_controller_);
    if (!user_ed) {
      return;
    }
    user_ed->AbortFeaturePromo(
        feature_engagement::kIPHReadingModeLineFocusFeature);
    if (features::IsReadAnythingReadAloudExperimentalPlaybackUiEnabled()) {
      user_ed->AbortFeaturePromo(
          feature_engagement::kIPHReadingModeAiPlaybackFeature);
      user_ed->AbortFeaturePromo(
          feature_engagement::kIPHReadingModeAiPlaybackSettingsFeature);
    }
    return;
  }

  if (new_state != DistillationState::kDistillationWithContent ||
      read_anything_controller_->GetPresentationState() ==
          PresentationState::kInactive) {
    return;
  }
  auto* user_ed = GetUserEducation(read_anything_controller_);
  if (!user_ed) {
    return;
  }
  if (features::IsReadAnythingLineFocusEnabled()) {
    // Only show the line focus IPH if there is content because line focus
    // does not do anything on an empty page.
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHReadingModeLineFocusFeature);
  }
  MaybeStartAiPlaybackIphTimer();
}

void ReadAnythingIphController::OnAiPlaybackStateChanged(bool enabled) {
  if (!read_anything_controller_ || !enabled ||
      !features::IsReadAnythingReadAloudExperimentalPlaybackUiEnabled()) {
    return;
  }
  ai_playback_iph_timer_.Stop();
  if (auto* user_ed = GetUserEducation(read_anything_controller_)) {
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHReadingModeAiPlaybackFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHReadingModeAiPlaybackSettingsFeature);
  }
}

void ReadAnythingIphController::OnLineFocusTurnedOn() {
  if (!read_anything_controller_) {
    return;
  }

  if (auto* user_ed = GetUserEducation(read_anything_controller_)) {
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHReadingModeLineFocusFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }
}

void ReadAnythingIphController::Activate(
    bool active,
    read_anything::mojom::ReadAnythingOpenTrigger trigger,
    std::optional<base::TimeDelta> completed_session_duration) {
  if (!read_anything_controller_) {
    return;
  }

  if (!active) {
    // Keep the countdown running while switching presentations.
    if (!read_anything_controller_->is_presentation_transitioning()) {
      ai_playback_iph_timer_.Stop();
    }
    return;
  }

  auto* user_ed = GetUserEducation(read_anything_controller_);
  if (!user_ed) {
    return;
  }
  const PresentationState state =
      read_anything_controller_->GetPresentationState();
  if (state == PresentationState::kInImmersiveOverlay ||
      state == PresentationState::kInSidePanel) {
    // ReadAnythingEntryPointController::ToggleUI() tells user education when
    // the shortcut is used.
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHReadingModeKeyboardShortcutFeature);
  }
  if (state == PresentationState::kInImmersiveOverlay) {
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHReadingModePresentationModeFeature);
  }
  if (distillation_state_ == DistillationState::kDistillationWithContent) {
    MaybeStartAiPlaybackIphTimer();
  }
}

void ReadAnythingIphController::OnDestroyed() {
  read_anything_controller_observer_.Reset();
  ai_playback_iph_timer_.Stop();
  read_anything_controller_ = nullptr;
}

void ReadAnythingIphController::MaybeStartAiPlaybackIphTimer() {
  if (!features::IsReadAnythingReadAloudExperimentalPlaybackUiEnabled() ||
      ai_playback_iph_timer_.IsRunning()) {
    return;
  }
  // Unretained is safe: `this` owns the timer.
  ai_playback_iph_timer_.Start(
      FROM_HERE, g_ai_playback_iph_delay,
      base::BindOnce(&ReadAnythingIphController::MaybeShowAiPlaybackIph,
                     base::Unretained(this)));
}

void ReadAnythingIphController::MaybeShowAiPlaybackIph() {
  if (!read_anything_controller_) {
    return;
  }

  CHECK(features::IsReadAnythingReadAloudExperimentalPlaybackUiEnabled());
  if (read_anything_controller_->GetPresentationState() ==
          PresentationState::kInactive ||
      distillation_state_ != DistillationState::kDistillationWithContent) {
    return;
  }
  if (auto* user_ed = GetUserEducation(read_anything_controller_)) {
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHReadingModeAiPlaybackFeature);
  }
}
