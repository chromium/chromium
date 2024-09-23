// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MANAGER_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"

namespace chromeos::editor_menu {

class EditorManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEditorModeChanged(const EditorMode& mode) = 0;
  };

  virtual ~EditorManager() = default;

  virtual void GetEditorPanelContext(
      base::OnceCallback<void(const EditorContext&)> callback) = 0;
  virtual void OnPromoCardDismissed() = 0;
  virtual void OnPromoCardDeclined() = 0;
  virtual void StartEditingFlow() = 0;
  virtual void StartEditingFlowWithPreset(std::string_view text_query_id) = 0;
  virtual void StartEditingFlowWithFreeform(std::string_view text) = 0;
  virtual void OnEditorMenuVisibilityChanged(bool visible) = 0;
  virtual void LogEditorMode(EditorMode mode) = 0;
  virtual void AddObserver(EditorManager::Observer* observer) = 0;
  virtual void RemoveObserver(EditorManager::Observer* observer) = 0;
  virtual void NotifyEditorModeChanged(const EditorMode& mode) = 0;
  virtual void RequestCacheContext() = 0;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MANAGER_H_
