// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_MENU_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_MENU_CONTROLLER_H_

#include "base/component_export.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos::editor_menu {

// A controller to manage the creation/dismissal of Editor Menu related views.
class COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP) EditorMenuController {
 public:
  EditorMenuController();
  virtual ~EditorMenuController();

  static EditorMenuController* Get();

  // Show the editor menu related views. `anchor_bounds` is the bounds of the
  // anchor view, which is the context menu for browser.
  virtual void MaybeShowEditorMenu(const gfx::Rect& anchor_bounds) = 0;

  // Dismiss the editor menu related views currently shown.
  virtual void DismissEditorMenu() = 0;

  // Update the bounds of the anchor view.
  virtual void UpdateAnchorBounds(const gfx::Rect& anchor_bounds) = 0;
};

}  // namespace chromeos::editor_menu

#endif  // CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_MENU_CONTROLLER_H_
