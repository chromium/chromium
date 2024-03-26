// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_manager_lacros.h"

#include <string_view>
#include <utility>

#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chrome/browser/ui/views/editor_menu/utils/mojo.h"
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos::editor_menu {
namespace {

mojo::Remote<crosapi::mojom::EditorPanelManager>& GetPanelManagerRemote() {
  chromeos::LacrosService* const lacros_service =
      chromeos::LacrosService::Get();
  CHECK(lacros_service->IsAvailable<crosapi::mojom::EditorPanelManager>());
  return lacros_service->GetRemote<crosapi::mojom::EditorPanelManager>();
}

}  // namespace

EditorManagerLacros::LacrosObserver::LacrosObserver(
    EditorManagerLacros* manager)
    : manager_(manager) {}

void EditorManagerLacros::LacrosObserver::OnEditorPanelModeChanged(
    crosapi::mojom::EditorPanelMode mode) {
  manager_->NotifyEditorModeChanged(FromMojoEditorMode(mode));
}

EditorManagerLacros::EditorManagerLacros()
    : lacros_observer_(this),
      panel_manager_remote_(GetPanelManagerRemote()),
      editor_observer_receiver_(&lacros_observer_) {
  (*panel_manager_remote_)
      ->BindEditorObserver(
          editor_observer_receiver_.BindNewPipeAndPassRemote());
}

EditorManagerLacros::~EditorManagerLacros() = default;

void EditorManagerLacros::GetEditorPanelContext(
    base::OnceCallback<void(EditorContext)> callback) {
  (*panel_manager_remote_)
      ->GetEditorPanelContext(
          base::BindOnce(&EditorManagerLacros::OnEditorPanelContextResult,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void EditorManagerLacros::OnPromoCardDismissed() {
  (*panel_manager_remote_)->OnPromoCardDismissed();
}

void EditorManagerLacros::OnPromoCardDeclined() {
  (*panel_manager_remote_)->OnPromoCardDeclined();
}

void EditorManagerLacros::StartEditingFlow() {
  (*panel_manager_remote_)->StartEditingFlow();
}

void EditorManagerLacros::StartEditingFlowWithPreset(
    std::string_view text_query_id) {
  (*panel_manager_remote_)
      ->StartEditingFlowWithPreset(std::string(text_query_id));
}

void EditorManagerLacros::StartEditingFlowWithFreeform(std::string_view text) {
  (*panel_manager_remote_)->StartEditingFlowWithFreeform(std::string(text));
}

void EditorManagerLacros::OnEditorMenuVisibilityChanged(bool visible) {
  (*panel_manager_remote_)->OnEditorMenuVisibilityChanged(visible);
}

void EditorManagerLacros::LogEditorMode(EditorMode mode) {
  (*panel_manager_remote_)->LogEditorMode(ToMojoEditorMode(mode));
}

void EditorManagerLacros::AddObserver(EditorManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void EditorManagerLacros::RemoveObserver(EditorManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void EditorManagerLacros::NotifyEditorModeChanged(const EditorMode& mode) {
  for (EditorManager::Observer& obs : observers_) {
    obs.OnEditorModeChanged(mode);
  }
}

void EditorManagerLacros::OnEditorPanelContextResult(
    base::OnceCallback<void(const EditorContext)> callback,
    crosapi::mojom::EditorPanelContextPtr panel_context) {
  std::move(callback).Run(FromMojoEditorContext(std::move(panel_context)));
}

}  // namespace chromeos::editor_menu
