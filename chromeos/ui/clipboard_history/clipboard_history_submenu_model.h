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
  // `source` indicates where the submenu model is used. It should be a context
  // menu.
  static std::unique_ptr<ClipboardHistorySubmenuModel>
  CreateClipboardHistorySubmenuModel(
      crosapi::mojom::ClipboardHistoryControllerShowSource source);

  ClipboardHistorySubmenuModel(const ClipboardHistorySubmenuModel&) = delete;
  ClipboardHistorySubmenuModel& operator=(const ClipboardHistorySubmenuModel&) =
      delete;
  ~ClipboardHistorySubmenuModel() override;

 private:
  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  ClipboardHistorySubmenuModel(
      crosapi::mojom::ClipboardHistoryControllerShowSource source,
      const std::vector<crosapi::mojom::ClipboardHistoryItemDescriptor>&
          item_descriptors);

  const crosapi::mojom::ClipboardHistoryControllerShowSource source_;

  // Mappings from command ids to clipboard history item ids.
  std::map<int, base::UnguessableToken> item_ids_by_command_ids_;
};

}  // namespace chromeos::clipboard_history

#endif  // CHROMEOS_UI_CLIPBOARD_HISTORY_CLIPBOARD_HISTORY_SUBMENU_MODEL_H_
