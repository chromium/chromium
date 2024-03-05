// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MANAGER_LACROS_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MANAGER_LACROS_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/views/editor_menu/editor_manager.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos::editor_menu {

class EditorManagerLacros : public EditorManager {
 public:
  class LacrosObserver : public crosapi::mojom::EditorObserver {
   public:
    explicit LacrosObserver(EditorManagerLacros* manager);

    // crosapi::mojom::EditorObserver overrides
    void OnEditorPanelModeChanged(
        crosapi::mojom::EditorPanelMode mode) override;

   private:
    raw_ptr<EditorManagerLacros> manager_;
  };

  EditorManagerLacros();
  ~EditorManagerLacros() override;

  // EditorManager overrides
  void GetEditorPanelContext(
      base::OnceCallback<void(EditorContext)> callback) override;
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

 private:
  void OnEditorPanelContextResult(
      base::OnceCallback<void(EditorContext)> callback,
      crosapi::mojom::EditorPanelContextPtr panel_context);

  LacrosObserver lacros_observer_;

  mojo::Remote<crosapi::mojom::EditorPanelManager>& panel_manager_remote_;

  mojo::Receiver<crosapi::mojom::EditorObserver> editor_observer_receiver_;

  base::ObserverList<EditorManager::Observer> observers_;

  base::WeakPtrFactory<EditorManagerLacros> weak_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MANAGER_LACROS_H_
