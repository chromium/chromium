// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_manager_ash.h"

#include <string_view>

#include "base/observer_list.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chrome/browser/ui/views/editor_menu/utils/mojo.h"
#include "chromeos/components/editor_menu/public/cpp/preset_text_query.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"

namespace chromeos::editor_menu {
namespace {

EditorMode ToEditorMode(ash::input_method::EditorMode mode) {
  switch (mode) {
    case ash::input_method::EditorMode::kHardBlocked:
      return EditorMode::kHardBlocked;
    case ash::input_method::EditorMode::kSoftBlocked:
      return EditorMode::kSoftBlocked;
    case ash::input_method::EditorMode::kConsentNeeded:
      return EditorMode::kPromoCard;
    case ash::input_method::EditorMode::kRewrite:
      return EditorMode::kRewrite;
    case ash::input_method::EditorMode::kWrite:
      return EditorMode::kWrite;
  }
}

}  // namespace

EditorManagerAsh::EditorManagerAsh(
    ash::input_method::EditorPanelManager* panel_manager)
    : panel_manager_(panel_manager), ash_observer_(this) {
  CHECK(panel_manager_);
  panel_manager_->AddObserver(&ash_observer_);
}

EditorManagerAsh::~EditorManagerAsh() {
  panel_manager_->RemoveObserver(&ash_observer_);
}

void EditorManagerAsh::GetEditorPanelContext(
    base::OnceCallback<void(const EditorContext&)> callback) {
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

void EditorManagerAsh::AddObserver(EditorManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void EditorManagerAsh::RemoveObserver(EditorManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void EditorManagerAsh::NotifyEditorModeChanged(const EditorMode& mode) {
  observers_.Notify(&EditorManager::Observer::OnEditorModeChanged, mode);
}

void EditorManagerAsh::RequestCacheContext() {
  panel_manager_->RequestCacheContext();
}

void EditorManagerAsh::OnEditorPanelContextResult(
    base::OnceCallback<void(const EditorContext&)> callback,
    crosapi::mojom::EditorPanelContextPtr panel_context) {
  std::move(callback).Run(FromMojoEditorContext(std::move(panel_context)));
}

EditorManagerAsh::AshObserver::AshObserver(EditorManagerAsh* manager)
    : manager_(manager) {}

EditorManagerAsh::AshObserver::~AshObserver() = default;

void EditorManagerAsh::AshObserver::OnEditorModeChanged(
    const ash::input_method::EditorMode& mode) {
  manager_->NotifyEditorModeChanged(ToEditorMode(mode));
}

}  // namespace chromeos::editor_menu
