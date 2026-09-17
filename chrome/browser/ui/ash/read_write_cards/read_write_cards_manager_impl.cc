// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_manager_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/magic_boost/magic_boost_controller.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_controller_impl.h"
#include "chrome/browser/ui/ash/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/ash/quick_answers/quick_answers_controller_impl.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_context.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"

namespace chromeos {

ReadWriteCardsManagerImpl::ReadWriteCardsManagerImpl(
    ApplicationLocaleStorage* application_locale_storage,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : quick_answers_controller_(std::make_unique<QuickAnswersControllerImpl>(
          application_locale_storage,
          ui_controller_)) {
  quick_answers_controller_->SetClient(
      std::make_unique<quick_answers::QuickAnswersClient>(
          shared_url_loader_factory,
          quick_answers_controller_->GetQuickAnswersDelegate()));

  if (chromeos::features::IsOrcaEnabled() ||
      ash::features::IsLobsterEnabled()) {
    editor_menu_controller_ =
        std::make_unique<editor_menu::EditorMenuControllerImpl>(
            application_locale_storage);
  }

  if (chromeos::features::IsMahiEnabled()) {
    mahi_menu_controller_.emplace(application_locale_storage, ui_controller_);
    magic_boost_card_controller_.emplace(application_locale_storage);
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

  editor_menu_controller_->GetEditorMenuCardContext(
      base::BindOnce(&ReadWriteCardsManagerImpl::OnGetEditorMenuCardContext,
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

void ReadWriteCardsManagerImpl::OnGetEditorMenuCardContext(
    const content::ContextMenuParams& params,
    editor_menu::FetchControllersCallback callback,
    const editor_menu::EditorMenuCardContext& editor_menu_card_context) {
  std::move(callback).Run(GetControllers(params, editor_menu_card_context));
}

std::vector<base::WeakPtr<chromeos::ReadWriteCardController>>
ReadWriteCardsManagerImpl::GetControllers(
    const content::ContextMenuParams& params,
    const editor_menu::EditorMenuCardContext& editor_menu_card_context) {
  const bool should_show_editor_menu =
      editor_menu_controller_ && params.is_editable;

  // Before branching off to MagicBoost, ensure top level funnel metrics for
  // Editor are recorded.
  if (should_show_editor_menu) {
    editor_menu_controller_->LogEditorMode(
        editor_menu_card_context.editor_mode());
  }

  bool editor_is_blocked = editor_menu_card_context.text_and_image_mode() ==
                           editor_menu::TextAndImageMode::kBlocked;
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
      magic_boost_state->IsUserEligibleForGenAIFeatures()) {
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

}  // namespace chromeos
