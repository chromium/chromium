// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/components/editor_menu/public/cpp/editor_menu_controller.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace chromeos::editor_menu {

// Implementation of EditorMenuController. It manages the editor menu related
// views.
class EditorMenuControllerImpl : public EditorMenuController {
 public:
  enum class ConsentStatus {
    kPending = 0,
    kAccepted,
    kRejected,
  };

  EditorMenuControllerImpl();
  EditorMenuControllerImpl(const EditorMenuControllerImpl&) = delete;
  EditorMenuControllerImpl& operator=(const EditorMenuControllerImpl&) = delete;
  ~EditorMenuControllerImpl() override;

  // EditorMenuController:
  void MaybeShowEditorMenu(const gfx::Rect& anchor_bounds) override;
  void DismissEditorMenu() override;
  void UpdateAnchorBounds(const gfx::Rect& anchor_bounds) override;

 private:
  views::UniqueWidgetPtr editor_menu_widget_;

  base::WeakPtrFactory<EditorMenuControllerImpl> weak_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_
