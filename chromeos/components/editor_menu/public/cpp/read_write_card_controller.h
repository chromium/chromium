// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_READ_WRITE_CARD_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_READ_WRITE_CARD_CONTROLLER_H_

#include "base/component_export.h"

class Profile;

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {

// A controller to manage the creation/dismissal of Quick Answers and Editor
// Menu related views.
class COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP) ReadWriteCardController {
 public:
  ReadWriteCardController() = default;
  virtual ~ReadWriteCardController() = default;

  // Called when the context menu is shown but the surrounding text still
  // pending.
  // `profile` is the profile that is associated with the browser in which the
  // context menu is shown.
  virtual void OnContextMenuShown(Profile* profile) = 0;

  // Called when the surrounding text is available.
  // `anchor_bounds` is the bounds of the anchor view, which is the context menu
  // for browser.
  // `selected_text` is the text selected by the user. Could be empty.
  // `surrounding_text` is the text surrounding selection.
  virtual void OnTextAvailable(const gfx::Rect& anchor_bounds,
                               const std::string& selected_text,
                               const std::string& surrounding_text) = 0;

  // Called when the bounds of the anchor view changes.
  virtual void OnAnchorBoundsChanged(const gfx::Rect& anchor_bounds) = 0;

  // Called when the context menu is closed.
  // `is_other_command_executed`: whether commands other than the card view is
  // executed, e.g. the commands in the context menu.
  virtual void OnDismiss(bool is_other_command_executed) = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_READ_WRITE_CARD_CONTROLLER_H_
