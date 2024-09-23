// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/utils/mojo.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"

namespace chromeos::editor_menu {
namespace {

using crosapi::mojom::EditorPanelContextPtr;
using crosapi::mojom::EditorPanelMode;
using crosapi::mojom::EditorPanelPresetTextQuery;
using crosapi::mojom::EditorPanelPresetTextQueryPtr;

}  // namespace

PresetQueryCategory FromMojoPresetQueryCategory(
    const crosapi::mojom::EditorPanelPresetQueryCategory category) {
  switch (category) {
    case crosapi::mojom::EditorPanelPresetQueryCategory::kUnknown:
      return PresetQueryCategory::kUnknown;
    case crosapi::mojom::EditorPanelPresetQueryCategory::kShorten:
      return PresetQueryCategory::kShorten;
    case crosapi::mojom::EditorPanelPresetQueryCategory::kElaborate:
      return PresetQueryCategory::kElaborate;
    case crosapi::mojom::EditorPanelPresetQueryCategory::kRephrase:
      return PresetQueryCategory::kRephrase;
    case crosapi::mojom::EditorPanelPresetQueryCategory::kFormalize:
      return PresetQueryCategory::kFormalize;
    case crosapi::mojom::EditorPanelPresetQueryCategory::kEmojify:
      return PresetQueryCategory::kEmojify;
    case crosapi::mojom::EditorPanelPresetQueryCategory::kProofread:
      return PresetQueryCategory::kProofread;
  }
}

EditorPanelMode ToMojoEditorMode(EditorMode mode) {
  switch (mode) {
    case EditorMode::kHardBlocked:
      return EditorPanelMode::kHardBlocked;
    case EditorMode::kSoftBlocked:
      return EditorPanelMode::kSoftBlocked;
    case EditorMode::kWrite:
      return EditorPanelMode::kWrite;
    case EditorMode::kRewrite:
      return EditorPanelMode::kRewrite;
    case EditorMode::kPromoCard:
      return EditorPanelMode::kPromoCard;
  }
}

EditorMode FromMojoEditorMode(EditorPanelMode mode) {
  switch (mode) {
    case EditorPanelMode::kHardBlocked:
      return EditorMode::kHardBlocked;
    case EditorPanelMode::kSoftBlocked:
      return EditorMode::kSoftBlocked;
    case EditorPanelMode::kWrite:
      return EditorMode::kWrite;
    case EditorPanelMode::kRewrite:
      return EditorMode::kRewrite;
    case EditorPanelMode::kPromoCard:
      return EditorMode::kPromoCard;
    case EditorPanelMode::kBlocked_DEPRECATED:
      LOG(ERROR) << "Reach DEPRECATED blocked editor mode, return HardBlocked "
                    "mode instead";
      return EditorMode::kHardBlocked;
  }
}

EditorContext FromMojoEditorContext(EditorPanelContextPtr panel_context) {
  PresetTextQueries preset_queries;
  for (const auto& query : panel_context->preset_text_queries) {
    preset_queries.push_back(
        PresetTextQuery(query->text_query_id, base::UTF8ToUTF16(query->name),
                        FromMojoPresetQueryCategory(query->category)));
  }

  return EditorContext(FromMojoEditorMode(panel_context->editor_panel_mode),
                       panel_context->consent_status_settled,
                       std::move(preset_queries));
}

}  // namespace chromeos::editor_menu
