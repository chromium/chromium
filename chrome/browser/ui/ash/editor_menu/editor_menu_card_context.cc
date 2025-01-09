// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/editor_menu/editor_menu_card_context.h"

#include "chrome/browser/ui/ash/editor_menu/editor_menu_strings.h"
#include "chrome/browser/ui/ash/editor_menu/utils/text_and_image_mode.h"

namespace chromeos::editor_menu {

EditorMenuCardContext::EditorMenuCardContext() = default;

EditorMenuCardContext::EditorMenuCardContext(const EditorMenuCardContext&) =
    default;

EditorMenuCardContext::~EditorMenuCardContext() = default;

TextAndImageMode EditorMenuCardContext::text_and_image_mode() const {
  return CalculateTextAndImageMode(editor_mode_, lobster_mode_);
}

bool EditorMenuCardContext::consent_status_settled() const {
  return consent_status_settled_;
}

EditorMode EditorMenuCardContext::editor_mode() const {
  return editor_mode_;
}

PresetTextQueries EditorMenuCardContext::preset_queries() const {
  PresetTextQueries preset_queries;

  switch (text_and_image_mode()) {
    case TextAndImageMode::kBlocked:
    case TextAndImageMode::kPromoCard:
    case TextAndImageMode::kEditorWriteOnly:
      return {};
    case TextAndImageMode::kEditorRewriteOnly:
      return editor_preset_queries_;
    case TextAndImageMode::kLobsterWithNoSelectedText:
      return {};
    case TextAndImageMode::kLobsterWithSelectedText:
      return {PresetTextQuery(/*preset_text_id=*/kLobsterPresetId,
                              GetEditorMenuLobsterChipLabel(),
                              PresetQueryCategory::kLobster)};
    case TextAndImageMode::kEditorWriteAndLobster:
      return {};
    case TextAndImageMode::kEditorRewriteAndLobster:
      preset_queries = editor_preset_queries_;
      preset_queries.push_back({PresetTextQuery(
          /*preset_text_id=*/kLobsterPresetId, GetEditorMenuLobsterChipLabel(),
          PresetQueryCategory::kLobster)});
      return preset_queries;
  }
}

EditorMenuCardContext& EditorMenuCardContext::set_consent_status_settled(
    bool consent_status_settled) {
  consent_status_settled_ = consent_status_settled;
  return *this;
}

EditorMenuCardContext& EditorMenuCardContext::set_editor_preset_queries(
    const PresetTextQueries& editor_preset_queries) {
  editor_preset_queries_ = editor_preset_queries;
  return *this;
}

EditorMenuCardContext& EditorMenuCardContext::set_editor_mode(
    EditorMode editor_mode) {
  editor_mode_ = editor_mode;
  return *this;
}

EditorMenuCardContext& EditorMenuCardContext::set_lobster_mode(
    LobsterMode lobster_mode) {
  lobster_mode_ = lobster_mode;
  return *this;
}

EditorMenuCardContext& EditorMenuCardContext::build() {
  return *this;
}

}  // namespace chromeos::editor_menu
