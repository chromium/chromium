// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_MENU_MODEL_H_

#include <cstddef>
#include <optional>
#include <string>

#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"
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
enum class SplitTabLayout;
}  // namespace split_tabs

class SplitTabMenuModel : public ui::SimpleMenuModel,
                          public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kReversePositionMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseStartTabMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseEndTabMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kExitSplitMenuItem);

  // These values are persisted to logs. Entries should not be renumbered and
  // number values should never be reused.
  // LINT.IfChange(SplitViewMenuEntry)
  enum class CommandId {
    kReversePosition = 0,
    kCloseSpecifiedTab,
    kCloseStartTab,
    kCloseEndTab,
    kExitSplit,
    kSendFeedback,
    kMaxValue = kSendFeedback,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:SplitViewMenuEntry)

  // Enum denoting where this menu is opening from. This is used to determine
  // which close options to show and for logging.
  enum class MenuSource {
    kToolbarButton,
    kMiniToolbar,
    kTabContextMenu,
  };

  SplitTabMenuModel(TabStripModel* tab_strip_model,
                    MenuSource menu_source,
                    std::optional<int> split_tab_index = std::nullopt);
  ~SplitTabMenuModel() override;

  // ui::SimpleMenuModel::Delegate override
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  ui::ImageModel GetIconForCommandId(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Returns the split id for the split tabs that this menu is acting on.
  split_tabs::SplitTabId GetSplitTabId() const;
  const gfx::VectorIcon& GetReversePositionIcon(
      split_tabs::SplitTabActiveLocation active_split_tab_location) const;
  split_tabs::SplitTabLayout GetSplitLayout() const;
  void CloseTabAtIndex(int index);
  void SendFeedback();

  raw_ptr<TabStripModel> tab_strip_model_ = nullptr;

  // This represents where the menu was opened from, for metrics logging
  // purposes.
  MenuSource menu_source_;

  // This represents the tab that this menu model should be operating on. If
  // nothing is provided, use the currently active tab.
  std::optional<int> split_tab_index_;
};

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_MENU_MODEL_H_
