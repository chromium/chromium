// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_CLIPBOARD_HISTORY_CLIPBOARD_HISTORY_SUBMENU_MODEL_H_
#define CHROMEOS_UI_CLIPBOARD_HISTORY_CLIPBOARD_HISTORY_SUBMENU_MODEL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "ui/base/models/simple_menu_model.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace crosapi::mojom {
class ClipboardHistoryItemDescriptor;
enum class ClipboardHistoryControllerShowSource;
}  // namespace crosapi::mojom

namespace chromeos::clipboard_history {

// A context submenu model that contains clipboard history item descriptors.
// Used only if the clipboard history refresh feature is enabled.
class COMPONENT_EXPORT(CHROMEOS_UI_CLIPBOARD_HISTORY)
    ClipboardHistorySubmenuModel : public ui::SimpleMenuModel,
                                   public ui::SimpleMenuModel::Delegate {
 public:
  // That callback that can be run to show the standalone clipboard history menu
  // in Ash. `event_flags` describes the event that caused the menu to show.
  using ShowClipboardHistoryMenuCallback =
      base::RepeatingCallback<void(int event_flags)>;

  static std::unique_ptr<ClipboardHistorySubmenuModel>
  CreateClipboardHistorySubmenuModel(
      crosapi::mojom::ClipboardHistoryControllerShowSource submenu_type,
      ShowClipboardHistoryMenuCallback show_menu_callback);

  ClipboardHistorySubmenuModel(const ClipboardHistorySubmenuModel&) = delete;
  ClipboardHistorySubmenuModel& operator=(const ClipboardHistorySubmenuModel&) =
      delete;
  ~ClipboardHistorySubmenuModel() override;

 private:
  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void OnMenuWillShow(SimpleMenuModel* model) override;

  ClipboardHistorySubmenuModel(
      crosapi::mojom::ClipboardHistoryControllerShowSource submenu_type,
      const std::vector<crosapi::mojom::ClipboardHistoryItemDescriptor>&
          item_descriptors,
      ShowClipboardHistoryMenuCallback show_menu_callback);

  // Indicates the type of submenu where this model is used.
  const crosapi::mojom::ClipboardHistoryControllerShowSource submenu_type_;

  // Mappings from command ids to clipboard history item ids.
  std::map<int, base::UnguessableToken> item_ids_by_command_ids_;

  // The callback to show the standalone clipboard history menu.
  const ShowClipboardHistoryMenuCallback show_menu_callback_;
};

}  // namespace chromeos::clipboard_history

#endif  // CHROMEOS_UI_CLIPBOARD_HISTORY_CLIPBOARD_HISTORY_SUBMENU_MODEL_H_
