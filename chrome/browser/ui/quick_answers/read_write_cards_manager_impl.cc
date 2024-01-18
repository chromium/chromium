// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/read_write_cards_manager_impl.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/hash/sha1.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"

namespace chromeos {

ReadWriteCardsManagerImpl::ReadWriteCardsManagerImpl()
    : quick_answers_controller_(
          std::make_unique<QuickAnswersControllerImpl>()) {
  quick_answers_controller_->SetClient(
      std::make_unique<quick_answers::QuickAnswersClient>(
          g_browser_process->shared_url_loader_factory(),
          quick_answers_controller_->GetQuickAnswersDelegate()));

  if (chromeos::features::IsOrcaEnabled()) {
    editor_menu_controller_ =
        std::make_unique<editor_menu::EditorMenuControllerImpl>();
  }

  if (chromeos::features::IsMahiEnabled()) {
    mahi_menu_controller_ = std::make_unique<mahi::MahiMenuController>();
  }
}

ReadWriteCardsManagerImpl::~ReadWriteCardsManagerImpl() = default;

void ReadWriteCardsManagerImpl::FetchController(
    const content::ContextMenuParams& params,
    content::BrowserContext* context,
    editor_menu::FetchControllerCallback callback) {
  // Skip password input field.
  const bool is_password_field =
      params.form_control_type == blink::mojom::FormControlType::kInputPassword;
  if (is_password_field) {
    std::move(callback).Run(nullptr);
    return;
  }
  if (editor_menu_controller_) {
    auto* panel_manager =
        editor_menu_controller_->GetEditorPanelManager(context);
    if (panel_manager && params.is_editable) {
      panel_manager->GetEditorPanelContext(base::BindOnce(
          &ReadWriteCardsManagerImpl::OnEditorPanelContextCallback,
          weak_factory_.GetWeakPtr(), params, std::move(callback), context));
      return;
    }
  }

  std::move(callback).Run(GetMahiOrQuickAnswerControllerIfEligible(params));
}

void ReadWriteCardsManagerImpl::OnEditorPanelContextCallback(
    const content::ContextMenuParams& params,
    editor_menu::FetchControllerCallback callback,
    content::BrowserContext* context,
    const crosapi::mojom::EditorPanelContextPtr editor_panel_context) {
  if (editor_menu_controller_) {
    if (editor_panel_context->editor_panel_mode !=
        crosapi::mojom::EditorPanelMode::kBlocked) {
      editor_menu_controller_->SetBrowserContext(context);
      std::move(callback).Run(editor_menu_controller_->GetWeakPtr());
      return;
    }
    auto* panel_manager =
        editor_menu_controller_->GetEditorPanelManager(context);
    if (panel_manager) {
      panel_manager->LogEditorMode(crosapi::mojom::EditorPanelMode::kBlocked);
    }
  }
  std::move(callback).Run(GetMahiOrQuickAnswerControllerIfEligible(params));
}

base::WeakPtr<chromeos::ReadWriteCardController>
ReadWriteCardsManagerImpl::GetMahiOrQuickAnswerControllerIfEligible(
    const content::ContextMenuParams& params) {
  if (params.selection_text.empty() && mahi_menu_controller_ &&
      chromeos::features::IsMahiEnabled()) {
    return mahi_menu_controller_->GetWeakPtr();
  }

  if (!QuickAnswersState::Get()->is_eligible() ||
      params.selection_text.empty() || !quick_answers_controller_) {
    return base::WeakPtr<chromeos::ReadWriteCardController>();
  }

  return quick_answers_controller_->GetWeakPtr();
}

}  // namespace chromeos
