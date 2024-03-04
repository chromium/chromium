// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_manager_lacros.h"

#include <string_view>

#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chrome/browser/ui/views/editor_menu/utils/mojo.h"
#include "chromeos/lacros/lacros_service.h"

namespace chromeos::editor_menu {
namespace {

mojo::Remote<crosapi::mojom::EditorPanelManager>& GetPanelManagerRemote() {
  chromeos::LacrosService* const lacros_service =
      chromeos::LacrosService::Get();
  CHECK(lacros_service->IsAvailable<crosapi::mojom::EditorPanelManager>());
  return lacros_service->GetRemote<crosapi::mojom::EditorPanelManager>();
}

}  // namespace

EditorManagerLacros::EditorManagerLacros()
    : panel_manager_remote_(GetPanelManagerRemote()) {}

EditorManagerLacros::~EditorManagerLacros() = default;

void EditorManagerLacros::GetEditorPanelContext(
    base::OnceCallback<void(EditorContext)> callback) {
  panel_manager_remote_->GetEditorPanelContext(
      base::BindOnce(&EditorManagerLacros::OnEditorPanelContextResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void EditorManagerLacros::OnPromoCardDismissed() {
  panel_manager_remote_->OnPromoCardDismissed();
}

void EditorManagerLacros::OnPromoCardDeclined() {
  panel_manager_remote_->OnPromoCardDeclined();
}

void EditorManagerLacros::StartEditingFlow() {
  panel_manager_remote_->StartEditingFlow();
}

void EditorManagerLacros::StartEditingFlowWithPreset(
    std::string_view text_query_id) {
  panel_manager_remote_->StartEditingFlowWithPreset(std::string(text_query_id));
}

void EditorManagerLacros::StartEditingFlowWithFreeform(std::string_view text) {
  panel_manager_remote_->StartEditingFlowWithFreeform(std::string(text));
}

void EditorManagerLacros::OnEditorMenuVisibilityChanged(bool visible) {
  panel_manager_remote_->OnEditorMenuVisibilityChanged(visible);
}

void EditorManagerLacros::LogEditorMode(EditorMode mode) {
  panel_manager_remote_->LogEditorMode(ToMojoEditorMode(mode));
}

void EditorManagerLacros::OnEditorPanelContextResult(
    base::OnceCallback<void(const EditorContext)> callback,
    crosapi::mojom::EditorPanelContextPtr panel_context) {
  std::move(callback).Run(FromMojoEditorContext(std::move(panel_context)));
}

}  // namespace chromeos::editor_menu
