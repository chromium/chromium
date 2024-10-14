// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MANAGER_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MANAGER_ASH_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#include "chrome/browser/ui/views/editor_menu/editor_manager.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"

namespace chromeos::editor_menu {

class EditorManagerAsh : public EditorManager {
 public:
  explicit EditorManagerAsh(
      ash::input_method::EditorPanelManager* panel_manager);
  ~EditorManagerAsh() override;

  // EditorManager overrides
  void GetEditorPanelContext(
      base::OnceCallback<void(const EditorContext&)> callback) override;
  void OnPromoCardDismissed() override;
  void OnPromoCardDeclined() override;
  void StartEditingFlow() override;
  void StartEditingFlowWithPreset(std::string_view text_query_id) override;
  void StartEditingFlowWithFreeform(std::string_view text) override;
  void OnEditorMenuVisibilityChanged(bool visible) override;
  void LogEditorMode(EditorMode mode) override;
  void AddObserver(EditorManager::Observer* observer) override;
  void RemoveObserver(EditorManager::Observer* observer) override;
  void NotifyEditorModeChanged(const EditorMode& mode) override;
  void RequestCacheContext() override;

 private:
  class AshObserver : public ash::input_method::EditorPanelManager::Observer {
   public:
    explicit AshObserver(EditorManagerAsh* manager);
    ~AshObserver() override;

    // EditorObserver overrides
    void OnEditorModeChanged(
        const ash::input_method::EditorMode& mode) override;

   private:
    // Not owned by this class
    raw_ptr<EditorManagerAsh> manager_;
  };

  void OnEditorPanelContextResult(
      base::OnceCallback<void(const EditorContext&)> callback,
      crosapi::mojom::EditorPanelContextPtr panel_context);

  raw_ptr<ash::input_method::EditorPanelManager> panel_manager_;

  AshObserver ash_observer_;

  base::ObserverList<EditorManager::Observer> observers_;

  base::WeakPtrFactory<EditorManagerAsh> weak_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MANAGER_ASH_H_
