// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_manager_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/hash/sha1.h"
#include "base/types/expected.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"

namespace chromeos {

using OptInFeatures = crosapi::mojom::MagicBoostController::OptInFeatures;

ReadWriteCardsManagerImpl::ReadWriteCardsManagerImpl()
    : quick_answers_controller_(
          std::make_unique<QuickAnswersControllerImpl>(ui_controller_)) {
  quick_answers_controller_->SetClient(
      std::make_unique<quick_answers::QuickAnswersClient>(
          g_browser_process->shared_url_loader_factory(),
          quick_answers_controller_->GetQuickAnswersDelegate()));

  if (chromeos::features::IsOrcaEnabled()) {
    editor_menu_controller_ =
        std::make_unique<editor_menu::EditorMenuControllerImpl>();
  }

  if (chromeos::features::IsMahiEnabled()) {
    mahi_menu_controller_.emplace(ui_controller_);
    magic_boost_card_controller_.emplace();
  }
}

ReadWriteCardsManagerImpl::~ReadWriteCardsManagerImpl() = default;

void ReadWriteCardsManagerImpl::FetchController(
    const content::ContextMenuParams& params,
    content::BrowserContext* context,
    editor_menu::FetchControllersCallback callback) {
  // Skip password input field.
  const bool is_password_field =
      params.form_control_type == blink::mojom::FormControlType::kInputPassword;
  if (is_password_field) {
    std::move(callback).Run({});
    return;
  }

  if (!editor_menu_controller_) {
    std::move(callback).Run(GetQuickAnswersAndMahiControllers(params));
    return;
  }

  editor_menu_controller_->GetEditorContext(
      base::BindOnce(&ReadWriteCardsManagerImpl::OnGetEditorContext,
                     weak_factory_.GetWeakPtr(), params, std::move(callback)));
}

void ReadWriteCardsManagerImpl::SetContextMenuBounds(
    const gfx::Rect& context_menu_bounds) {
  ui_controller_.SetContextMenuBounds(context_menu_bounds);
}

void ReadWriteCardsManagerImpl::TryCreatingEditorSession(
    const content::ContextMenuParams& params,
    content::BrowserContext* context) {
  if (editor_menu_controller_) {
    editor_menu_controller_->SetBrowserContext(context);
    editor_menu_controller_->TryCreatingEditorSession();
  }
}

void ReadWriteCardsManagerImpl::OnGetEditorContext(
    const content::ContextMenuParams& params,
    editor_menu::FetchControllersCallback callback,
    const editor_menu::EditorContext& editor_context) {
  std::move(callback).Run(GetControllers(params, editor_context));
}

std::vector<base::WeakPtr<chromeos::ReadWriteCardController>>
ReadWriteCardsManagerImpl::GetControllers(
    const content::ContextMenuParams& params,
    const editor_menu::EditorContext& editor_context) {
  const bool should_show_editor_menu =
      editor_menu_controller_ && params.is_editable;

  bool editor_is_blocked =
      editor_context.mode == editor_menu::EditorMode::kHardBlocked ||
      editor_context.mode == editor_menu::EditorMode::kSoftBlocked;

  // Before branching off to MagicBoost, ensure top level funnel metrics for
  // Editor are recorded when the feature is blocked from use.
  if (should_show_editor_menu && editor_is_blocked) {
    editor_menu_controller_->LogEditorMode(editor_context.mode);
  }

  auto opt_in_features = GetMagicBoostOptInFeatures(params, editor_context);

  if (opt_in_features) {
    crosapi::mojom::MagicBoostController::TransitionAction action =
        crosapi::mojom::MagicBoostController::TransitionAction::kDoNothing;

    // Calculate the action to take after the opt-in flow.
    if (should_show_editor_menu) {
      action = crosapi::mojom::MagicBoostController::TransitionAction::
          kShowEditorPanel;
    } else if (ShouldShowMahi(params)) {
      action =
          crosapi::mojom::MagicBoostController::TransitionAction::kShowHmrPanel;
    }

    // Always set the transition action to handle the edge case that this code
    // path is hit more than once with different actions.
    CHECK(magic_boost_card_controller_);
    magic_boost_card_controller_->set_transition_action(action);

    magic_boost_card_controller_->SetOptInFeature(opt_in_features.value());

    return {magic_boost_card_controller_->GetWeakPtr()};
  }

  // If Magic Boost is not enabled, each feature (besides Mahi which only uses
  // Magic Boost) will have its own opt-in flow, provided within each individual
  // controller.
  if (should_show_editor_menu && !editor_is_blocked) {
    // Use editor menu if available.
    return {editor_menu_controller_->GetWeakPtr()};
  }

  // Otherwise, use Quick Answers and Mahi if available.

  auto* magic_boost_state = chromeos::MagicBoostState::Get();
  bool should_show_hmr_card = true;
  if (magic_boost_card_controller_ &&
      magic_boost_state->IsMagicBoostAvailable()) {
    should_show_hmr_card = magic_boost_state->ShouldShowHmrCard();

    // Ensure the disclaimer view is closed before moving to the next step
    magic_boost_card_controller_->CloseDisclaimerUi();
  }

  if (!should_show_hmr_card) {
    return {};
  }

  return GetQuickAnswersAndMahiControllers(params);
}

std::vector<base::WeakPtr<chromeos::ReadWriteCardController>>
ReadWriteCardsManagerImpl::GetQuickAnswersAndMahiControllers(
    const content::ContextMenuParams& params) {
  std::vector<base::WeakPtr<chromeos::ReadWriteCardController>> controllers;

  if (ShouldShowQuickAnswers(params)) {
    controllers.emplace_back(quick_answers_controller_->GetWeakPtr());
  }

  if (mahi_menu_controller_) {
    mahi_menu_controller_->RecordPageDistillable();
    if (ShouldShowMahi(params)) {
      controllers.emplace_back(mahi_menu_controller_->GetWeakPtr());
    }
  }

  return controllers;
}

bool ReadWriteCardsManagerImpl::ShouldShowQuickAnswers(
    const content::ContextMenuParams& params) {
  // Display Quick Answers card if it is eligible and there's selected text.
  return QuickAnswersState::IsEligible() && !params.selection_text.empty() &&
         quick_answers_controller_;
}

bool ReadWriteCardsManagerImpl::ShouldShowMahi(
    const content::ContextMenuParams& params) {
  return chromeos::features::IsMahiEnabled() && mahi_menu_controller_ &&
         mahi_menu_controller_->IsFocusedPageDistillable();
}

std::optional<OptInFeatures>
ReadWriteCardsManagerImpl::GetMagicBoostOptInFeatures(
    const content::ContextMenuParams& params,
    const editor_menu::EditorContext& editor_context) {
  if (!magic_boost_card_controller_ ||
      !chromeos::MagicBoostState::Get()->IsMagicBoostAvailable()) {
    return std::nullopt;
  }

  // Check if we should go through Magic Boost opt-in flow when we should show
  // Editor card.
  const bool should_show_editor_menu =
      editor_menu_controller_ && params.is_editable;

  // Only opt in orca if it is not blocked by any hard requirements and its
  // current status is unset.
  const bool should_opt_in_orca =
      editor_context.mode != editor_menu::EditorMode::kHardBlocked &&
      !editor_context.consent_status_settled;

  if (should_show_editor_menu) {
    if (should_opt_in_orca) {
      // We should opt in both Orca and HMR if we are opting-in Orca.
      return OptInFeatures::kOrcaAndHmr;
    }

    return std::nullopt;
  }

  // Check if we should go through Magic Boost opt-in flow when we should show
  // Quick Answers and/or Mahi card.
  base::expected<HMRConsentStatus, MagicBoostState::Error> hmr_consent_status =
      MagicBoostState::Get()->hmr_consent_status();
  if ((ShouldShowQuickAnswers(params) || ShouldShowMahi(params)) &&
      hmr_consent_status == HMRConsentStatus::kUnset) {
    return should_opt_in_orca ? OptInFeatures::kOrcaAndHmr
                              : OptInFeatures::kHmrOnly;
  }

  return std::nullopt;
}

}  // namespace chromeos
