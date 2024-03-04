// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_manager_ash.h"

#include <string_view>

#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chrome/browser/ui/views/editor_menu/utils/mojo.h"
#include "chrome/browser/ui/views/editor_menu/utils/preset_text_query.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
#include "content/public/browser/browser_context.h"

namespace chromeos::editor_menu {

EditorManagerAsh::EditorManagerAsh(content::BrowserContext* context)
    : panel_manager_(ash::input_method::EditorMediatorFactory::GetInstance()
                         ->GetForProfile(Profile::FromBrowserContext(context))
                         ->panel_manager()) {}

EditorManagerAsh::~EditorManagerAsh() = default;

void EditorManagerAsh::GetEditorPanelContext(
    base::OnceCallback<void(EditorContext)> callback) {
  panel_manager_->GetEditorPanelContext(
      base::BindOnce(&EditorManagerAsh::OnEditorPanelContextResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void EditorManagerAsh::OnPromoCardDismissed() {
  panel_manager_->OnPromoCardDismissed();
}

void EditorManagerAsh::OnPromoCardDeclined() {
  panel_manager_->OnPromoCardDeclined();
}

void EditorManagerAsh::StartEditingFlow() {
  panel_manager_->StartEditingFlow();
}

void EditorManagerAsh::StartEditingFlowWithPreset(
    std::string_view text_query_id) {
  panel_manager_->StartEditingFlowWithPreset(std::string(text_query_id));
}

void EditorManagerAsh::StartEditingFlowWithFreeform(std::string_view text) {
  panel_manager_->StartEditingFlowWithFreeform(std::string(text));
}

void EditorManagerAsh::OnEditorMenuVisibilityChanged(bool visible) {
  panel_manager_->OnEditorMenuVisibilityChanged(visible);
}

void EditorManagerAsh::LogEditorMode(EditorMode mode) {
  panel_manager_->LogEditorMode(ToMojoEditorMode(mode));
}

void EditorManagerAsh::OnEditorPanelContextResult(
    base::OnceCallback<void(EditorContext)> callback,
    crosapi::mojom::EditorPanelContextPtr panel_context) {
  std::move(callback).Run(FromMojoEditorContext(std::move(panel_context)));
}

}  // namespace chromeos::editor_menu
