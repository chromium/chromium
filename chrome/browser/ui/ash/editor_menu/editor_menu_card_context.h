// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_EDITOR_MENU_EDITOR_MENU_CARD_CONTEXT_H_
#define CHROME_BROWSER_UI_ASH_EDITOR_MENU_EDITOR_MENU_CARD_CONTEXT_H_

#include "chrome/browser/ui/ash/editor_menu/utils/text_and_image_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/preset_text_query.h"

namespace chromeos::editor_menu {

enum class EditorMenuCardTextSelectionMode {
  kNoSelection,
  kHasSelection,
};

struct EditorMenuCardContext {
 public:
  EditorMenuCardContext();
  EditorMenuCardContext(const EditorMenuCardContext&);
  ~EditorMenuCardContext();

  TextAndImageMode text_and_image_mode() const;
  bool consent_status_settled() const;
  PresetTextQueries preset_queries() const;
  EditorMode editor_mode() const;
  EditorMenuCardContext& set_consent_status_settled(
      bool consent_status_settled);
  EditorMenuCardContext& set_editor_preset_queries(
      const PresetTextQueries& preset_queries);
  EditorMenuCardContext& set_editor_mode(EditorMode editor_mode);
  EditorMenuCardContext& set_lobster_mode(LobsterMode lobster_mode);
  EditorMenuCardContext& set_text_selection_mode(
      EditorMenuCardTextSelectionMode text_selection_mode);
  EditorMenuCardContext& build();

 private:
  // indicating whether the shared consent status is already determined or still
  // unset.
  bool consent_status_settled_ = false;
  PresetTextQueries editor_preset_queries_;
  EditorMode editor_mode_ = EditorMode::kHardBlocked;
  LobsterMode lobster_mode_ = LobsterMode::kBlocked;
  EditorMenuCardTextSelectionMode text_selection_mode_ =
      EditorMenuCardTextSelectionMode::kNoSelection;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_ASH_EDITOR_MENU_EDITOR_MENU_CARD_CONTEXT_H_
