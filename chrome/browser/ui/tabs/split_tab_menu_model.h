// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_MENU_MODEL_H_

#include <cstddef>
#include <string>

#include "ui/base/interaction/element_identifier.h"
#include "ui/menus/simple_menu_model.h"

class TabStripModel;

namespace ui {
class ImageModel;
}  // namespace ui

namespace gfx {
struct VectorIcon;
}

namespace split_tabs {
class SplitTabId;
enum class SplitTabActiveLocation;
}  // namespace split_tabs

class SplitTabMenuModel : public ui::SimpleMenuModel,
                          public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSwapPositionMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kExitSplitMenuItem);

  explicit SplitTabMenuModel(TabStripModel* tab_strip_model);
  ~SplitTabMenuModel() override;

  enum class CommandId { kSwapPosition, kClose, kExitSplit };

  // ui::SimpleMenuModel::Delegate override
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  ui::ImageModel GetIconForCommandId(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Returns the split id for the split tabs that this menu is acting on.
  split_tabs::SplitTabId GetSplitTabId() const;
  const gfx::VectorIcon& GetSwapPositionIcon(
      split_tabs::SplitTabActiveLocation active_split_tab_location) const;

  raw_ptr<TabStripModel> tab_strip_model_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_MENU_MODEL_H_
