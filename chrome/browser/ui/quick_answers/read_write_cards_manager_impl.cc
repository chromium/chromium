// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/read_write_cards_manager_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/hash/sha1.h"
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
  }

  if (chromeos::features::IsMagicBoostEnabled()) {
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
  if (editor_menu_controller_ && params.is_editable) {
    editor_menu_controller_->SetBrowserContext(context);
    editor_menu_controller_->GetEditorMode(base::BindOnce(
        &ReadWriteCardsManagerImpl::OnGetEditorModeResult,
        weak_factory_.GetWeakPtr(), params, std::move(callback)));
    return;
  }

  std::move(callback).Run(GetControllers(params, /*editor_mode=*/std::nullopt));
}

void ReadWriteCardsManagerImpl::SetContextMenuBounds(
    const gfx::Rect& context_menu_bounds) {
  ui_controller_.SetContextMenuBounds(context_menu_bounds);
}

void ReadWriteCardsManagerImpl::OnGetEditorModeResult(
    const content::ContextMenuParams& params,
    editor_menu::FetchControllersCallback callback,
    editor_menu::EditorMode editor_mode) {
  std::move(callback).Run(GetControllers(params, editor_mode));
}

std::vector<base::WeakPtr<chromeos::ReadWriteCardController>>
ReadWriteCardsManagerImpl::GetControllers(
    const content::ContextMenuParams& params,
    std::optional<editor_menu::EditorMode> editor_mode) {
  // Display Quick Answers card if it is eligible and there's selected text.
  const bool should_show_qa = QuickAnswersState::IsEligible() &&
                              !params.selection_text.empty() &&
                              quick_answers_controller_;
  const bool should_show_mahi =
      chromeos::features::IsMahiEnabled() && mahi_menu_controller_ &&
      mahi_menu_controller_->IsFocusedPageDistillable();

  std::optional<HMRConsentStatus> hmr_consent_status;
  if (chromeos::features::IsMagicBoostEnabled()) {
    hmr_consent_status = MagicBoostState::Get()->hmr_consent_status();
  }

  // Check if we should go through Magic Boost opt-in flow.
  const bool should_opt_in_orca_with_magic_boost =
      editor_mode == editor_menu::EditorMode::kPromoCard;
  const bool should_opt_in_hmr_with_magic_boost =
      !editor_mode && (should_show_mahi || should_show_qa) &&
      hmr_consent_status == HMRConsentStatus::kUnset;

  if (magic_boost_card_controller_ && (should_opt_in_hmr_with_magic_boost ||
                                       should_opt_in_orca_with_magic_boost)) {
    // Show Editor Panel after completing the opt-in flow for Orca.
    magic_boost_card_controller_->set_transition_action(
        should_opt_in_orca_with_magic_boost
            ? crosapi::mojom::MagicBoostController::TransitionAction::
                  kShowEditorPanel
            : crosapi::mojom::MagicBoostController::TransitionAction::
                  kDoNothing);

    // Set the features that triggers the magic boost feature (the opt in card
    // and the disclaimer view). If it should opt in orca, then the features are
    // `OptInFeatures::kOrcaAndHmr`.
    OptInFeatures opt_in_features;
    if (should_opt_in_orca_with_magic_boost) {
      opt_in_features = OptInFeatures::kOrcaAndHmr;
    } else {
      CHECK(should_opt_in_hmr_with_magic_boost);
      opt_in_features = OptInFeatures::kHmrOnly;
    }
    magic_boost_card_controller_->SetOptInFeature(opt_in_features);

    return {magic_boost_card_controller_->GetWeakPtr()};
  }

  // If Magic Boost is not enabled, each feature (besides Mahi which only uses
  // Magic Boost) will have its own opt-in flow, provided within each individual
  // controller.
  if (editor_mode) {
    // Use editor menu if available.
    if (editor_mode.value() != editor_menu::EditorMode::kBlocked) {
      return {editor_menu_controller_->GetWeakPtr()};
    }

    editor_menu_controller_->LogEditorMode(editor_menu::EditorMode::kBlocked);
  }

  // Otherwise, use Quick Answers and Mahi if available.

  // Return no controller if consent_status is `kDeclined` (users explicitly
  // decline in the opt-in flow), or `kUnset` (both Quick Answers and Mahi is
  // not available to opt-in).
  if (hmr_consent_status == HMRConsentStatus::kDeclined ||
      hmr_consent_status == HMRConsentStatus::kUnset) {
    return {};
  }

  if (hmr_consent_status) {
    CHECK(hmr_consent_status == HMRConsentStatus::kApproved ||
          hmr_consent_status == HMRConsentStatus::kPending);
  }

  std::vector<base::WeakPtr<chromeos::ReadWriteCardController>> controllers;

  if (should_show_qa) {
    controllers.emplace_back(quick_answers_controller_->GetWeakPtr());
  }

  if (mahi_menu_controller_) {
    mahi_menu_controller_->RecordPageDistillable();
    if (should_show_mahi) {
      controllers.emplace_back(mahi_menu_controller_->GetWeakPtr());
    }
  }

  return controllers;
}

}  // namespace chromeos
