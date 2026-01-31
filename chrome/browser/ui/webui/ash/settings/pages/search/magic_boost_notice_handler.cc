// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/search/magic_boost_notice_handler.h"

#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/ash/input_method/editor_transition_enums.h"
#include "chrome/browser/ash/magic_boost/magic_boost_controller.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "ui/display/screen.h"

namespace ash::settings {

MagicBoostNoticeHandler::MagicBoostNoticeHandler(
    mojo::PendingReceiver<magic_boost_handler::mojom::PageHandler> receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {}

MagicBoostNoticeHandler::~MagicBoostNoticeHandler() = default;

void MagicBoostNoticeHandler::ShowNotice() {
  if (chromeos::MagicBoostState::Get()->IsUserEligibleForGenAIFeatures() &&
      ash::MagicBoostController::Get()) {
    ash::MagicBoostController::Get()->ShowDisclaimerUi(
        display::Screen::Get()->GetPrimaryDisplay().id(),
        magic_boost::TransitionAction::kDoNothing,
        magic_boost::OptInFeatures::kOrcaAndHmr);
    return;
  }

  ash::input_method::EditorMediator* mediator =
      ash::input_method::EditorMediatorFactory::GetInstance()->GetForProfile(
          profile_);
  if (mediator != nullptr && mediator->IsAllowedForUse()) {
    mediator->ShowNotice(
        input_method::EditorNoticeTransitionAction::kDoNothing);
  }
}

}  // namespace ash::settings
