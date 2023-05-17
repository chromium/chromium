// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/clipboard_history/clipboard_history_submenu_model.h"

#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"
#include "ui/base/command_id_constants.h"

namespace chromeos::clipboard_history {

// static
std::unique_ptr<ClipboardHistorySubmenuModel>
ClipboardHistorySubmenuModel::CreateClipboardHistorySubmenuModel(
    crosapi::mojom::ClipboardHistoryControllerShowSource source) {
  CHECK(source == crosapi::mojom::ClipboardHistoryControllerShowSource::
                      kRenderViewContextMenu ||
        source == crosapi::mojom::ClipboardHistoryControllerShowSource::
                      kTextfieldContextMenu);
  return base::WrapUnique(
      new ClipboardHistorySubmenuModel(source, QueryItemDescriptors()));
}

ClipboardHistorySubmenuModel::~ClipboardHistorySubmenuModel() = default;

void ClipboardHistorySubmenuModel::ExecuteCommand(int command_id,
                                                  int event_flags) {
  if (auto iter = item_ids_by_command_ids_.find(command_id);
      iter != item_ids_by_command_ids_.end()) {
    PasteClipboardItemById(iter->second, event_flags, source_);
  }
}

ClipboardHistorySubmenuModel::ClipboardHistorySubmenuModel(
    crosapi::mojom::ClipboardHistoryControllerShowSource source,
    const std::vector<crosapi::mojom::ClipboardHistoryItemDescriptor>&
        item_descriptors)
    : ui::SimpleMenuModel(this), source_(source) {
  for (size_t index = 0; index < item_descriptors.size(); ++index) {
    // Use the first unbounded command ID as the start ID so that the command
    // IDs in the submenu do not conflict with those in the parent menu.
    const size_t command_id = COMMAND_ID_FIRST_UNBOUNDED + index;
    AddItemWithIcon(command_id, item_descriptors[index].display_text,
                    GetIconForDescriptor(item_descriptors[index]));
    item_ids_by_command_ids_.emplace(command_id,
                                     item_descriptors[index].item_id);
  }
}

}  // namespace chromeos::clipboard_history
