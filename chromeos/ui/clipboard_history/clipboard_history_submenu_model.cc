// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/clipboard_history/clipboard_history_submenu_model.h"

#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"

namespace chromeos::clipboard_history {

// static
std::unique_ptr<ClipboardHistorySubmenuModel>
ClipboardHistorySubmenuModel::CreateClipboardHistorySubmenuModel(
    size_t start_command_id) {
  return base::WrapUnique(new ClipboardHistorySubmenuModel(
      start_command_id, QueryItemDescriptors()));
}

ClipboardHistorySubmenuModel::~ClipboardHistorySubmenuModel() = default;

void ClipboardHistorySubmenuModel::ExecuteCommand(int command_id,
                                                  int event_flags) {
  if (auto iter = item_ids_by_command_ids_.find(command_id);
      iter != item_ids_by_command_ids_.end()) {
    PasteClipboardItemById(iter->second);
  }
}

ClipboardHistorySubmenuModel::ClipboardHistorySubmenuModel(
    size_t start_command_id,
    const std::vector<crosapi::mojom::ClipboardHistoryItemDescriptor>&
        item_descriptors)
    : ui::SimpleMenuModel(this) {
  for (size_t index = 0; index < item_descriptors.size(); ++index) {
    const size_t command_id = start_command_id + index;
    AddItemWithIcon(
        command_id, item_descriptors[index].display_text,
        GetIconForDisplayFormat(item_descriptors[index].display_format));
    item_ids_by_command_ids_.emplace(command_id,
                                     item_descriptors[index].item_id);
  }
}

}  // namespace chromeos::clipboard_history
