// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/clipboard_history/clipboard_history_submenu_model.h"

#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"
#include "ui/base/command_id_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos::clipboard_history {

// static
std::unique_ptr<ClipboardHistorySubmenuModel>
ClipboardHistorySubmenuModel::CreateClipboardHistorySubmenuModel(
    crosapi::mojom::ClipboardHistoryControllerShowSource source,
    ShowClipboardHistoryMenuCallback show_menu_callback) {
  CHECK(source == crosapi::mojom::ClipboardHistoryControllerShowSource::
                      kRenderViewContextMenu ||
        source == crosapi::mojom::ClipboardHistoryControllerShowSource::
                      kTextfieldContextMenu);
  return base::WrapUnique(new ClipboardHistorySubmenuModel(
      source, QueryItemDescriptors(), show_menu_callback));
}

ClipboardHistorySubmenuModel::~ClipboardHistorySubmenuModel() = default;

void ClipboardHistorySubmenuModel::ExecuteCommand(int command_id,
                                                  int event_flags) {
  // If `command_id` matches any clipboard data, paste the matched data.
  if (auto iter = item_ids_by_command_ids_.find(command_id);
      iter != item_ids_by_command_ids_.end()) {
    PasteClipboardItemById(iter->second, event_flags, source_);
    return;
  }

  // Otherwise, `command_id` should correspond to the command to show the
  // standalone clipboard history menu.
  CHECK_EQ(command_id, IDS_APP_SHOW_CLIPBOARD_HISTORY);
  show_menu_callback_.Run(event_flags);
}

bool ClipboardHistorySubmenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  if (command_id == IDS_APP_SHOW_CLIPBOARD_HISTORY) {
    *accelerator = ui::Accelerator(ui::VKEY_V, ui::EF_COMMAND_DOWN);
    return true;
  }

  return ui::SimpleMenuModel::Delegate::GetAcceleratorForCommandId(command_id,
                                                                   accelerator);
}

ClipboardHistorySubmenuModel::ClipboardHistorySubmenuModel(
    crosapi::mojom::ClipboardHistoryControllerShowSource source,
    const std::vector<crosapi::mojom::ClipboardHistoryItemDescriptor>&
        item_descriptors,
    ShowClipboardHistoryMenuCallback show_menu_callback)
    : ui::SimpleMenuModel(this),
      source_(source),
      show_menu_callback_(show_menu_callback) {
  for (size_t index = 0; index < item_descriptors.size(); ++index) {
    // Use the first unbounded command ID as the start ID so that the command
    // IDs in the submenu do not conflict with those in the parent menu.
    const size_t command_id = COMMAND_ID_FIRST_UNBOUNDED + index;
    AddItemWithIcon(command_id, item_descriptors[index].display_text,
                    GetIconForDescriptor(item_descriptors[index]));
    item_ids_by_command_ids_.emplace(command_id,
                                     item_descriptors[index].item_id);
  }

  // The last item can be activated to show the standalone clipboard history
  // menu.
  AddItem(IDS_APP_SHOW_CLIPBOARD_HISTORY,
          l10n_util::GetStringUTF16(IDS_APP_SHOW_CLIPBOARD_HISTORY));
}

}  // namespace chromeos::clipboard_history
