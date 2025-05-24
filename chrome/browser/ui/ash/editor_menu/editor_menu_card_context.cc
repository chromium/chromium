// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/editor_menu/editor_menu_card_context.h"

#include "chrome/browser/ui/ash/editor_menu/editor_menu_strings.h"
#include "chrome/browser/ui/ash/editor_menu/utils/text_and_image_mode.h"
#include "chromeos/constants/chromeos_features.h"

namespace chromeos::editor_menu {

EditorMenuCardContext::EditorMenuCardContext() = default;

EditorMenuCardContext::EditorMenuCardContext(const EditorMenuCardContext&) =
    default;

EditorMenuCardContext::~EditorMenuCardContext() = default;

TextAndImageMode EditorMenuCardContext::text_and_image_mode() const {
  // TODO: b:389553095 - With the introduction of EditorTextSelectionMode,
  // merge kRewrite and kWrite into a single enum value.
  switch (editor_mode_) {
    case EditorMode::kRewrite:
    case EditorMode::kWrite:
    case EditorMode::kConsentNeeded:
      if (editor_mode_ == EditorMode::kConsentNeeded &&
          !chromeos::features::IsMagicBoostRevampEnabled()) {
        return TextAndImageMode::kPromoCard;
      }
      if (lobster_mode_ == LobsterMode::kBlocked) {
        return text_selection_mode_ ==
                       EditorMenuCardTextSelectionMode::kHasSelection
                   ? TextAndImageMode::kEditorRewriteOnly
                   : TextAndImageMode::kEditorWriteOnly;
      }
      return text_selection_mode_ ==
                     EditorMenuCardTextSelectionMode::kHasSelection
                 ? TextAndImageMode::kEditorRewriteAndLobster
                 : TextAndImageMode::kEditorWriteAndLobster;

    case EditorMode::kSoftBlocked:
    case EditorMode::kHardBlocked:
      if (lobster_mode_ == LobsterMode::kBlocked) {
        return TextAndImageMode::kBlocked;
      }

      return text_selection_mode_ ==
                     EditorMenuCardTextSelectionMode::kHasSelection
                 ? TextAndImageMode::kLobsterWithSelectedText
                 : TextAndImageMode::kLobsterWithNoSelectedText;
  }
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

EditorMenuCardContext& EditorMenuCardContext::set_text_selection_mode(
    EditorMenuCardTextSelectionMode text_selection_mode) {
  text_selection_mode_ = text_selection_mode;
  return *this;
}

EditorMenuCardContext& EditorMenuCardContext::build() {
  return *this;
}

}  // namespace chromeos::editor_menu
