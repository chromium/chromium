// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_MUTE_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_MUTE_MENU_MODEL_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"
#include "components/split_tabs/split_tab_id.h"
#include "ui/menus/simple_menu_model.h"

class TabStripModel;

namespace ui {
class ImageModel;
}  // namespace ui

// Submenu of the tab context menu shown in place of "Mute sites" when the menu
// targets a split. It lets the user mute or unmute both sites in the split, or
// only the site shown in one of the views.
class SplitTabMuteMenuModel : public ui::SimpleMenuModel,
                              public ui::SimpleMenuModel::Delegate {
 public:
  // Start command IDs at 2001 to avoid conflicts with other submenus.
  enum class CommandId {
    kToggleAllSitesMuted =
        ExistingBaseSubMenuModel::kMinSplitTabMuteMenuModelCommandId,
    kToggleStartSiteMuted,
    kToggleEndSiteMuted,
  };

  // `tab_index` is the index of the tab the context menu was opened on, which
  // must be part of `split_id`.
  SplitTabMuteMenuModel(TabStripModel* tab_strip_model,
                        int tab_index,
                        split_tabs::SplitTabId split_id);
  SplitTabMuteMenuModel(const SplitTabMuteMenuModel&) = delete;
  SplitTabMuteMenuModel& operator=(const SplitTabMuteMenuModel&) = delete;
  ~SplitTabMuteMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  ui::ImageModel GetIconForCommandId(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Returns the index of the tab targeted by `id`, or nullopt if `id` does not
  // target a single view or the split no longer exists.
  std::optional<int> GetTabIndexForCommand(CommandId id) const;

  // Returns true if the split is laid out side by side rather than stacked.
  bool IsSideBySide() const;

  // Returns true if executing `id` will mute rather than unmute.
  bool WillMute(CommandId id) const;

  const raw_ptr<TabStripModel> tab_strip_model_;
  const int tab_index_;
  const split_tabs::SplitTabId split_id_;
};

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_MUTE_MENU_MODEL_H_
