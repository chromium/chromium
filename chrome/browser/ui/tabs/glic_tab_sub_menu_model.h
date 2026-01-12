// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_TAB_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_GLIC_TAB_SUB_MENU_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "ui/menus/simple_menu_model.h"

class TabStripModel;

namespace glic {

// A sub menu model for sharing and unsharing tabs with Gemini.
class GlicTabSubMenuModel : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  GlicTabSubMenuModel(TabStripModel* tab_strip_model, int context_index);
  ~GlicTabSubMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // Command IDs for recent conversations.
  static constexpr int kMinRecentConversationCommandId = 1000;
  static constexpr int kMaxRecentConversationCommandId = 1009;

 private:
  const raw_ptr<TabStripModel> tab_strip_model_;
  const int context_index_;
  std::vector<ConversationInfo> recent_conversations_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_TABS_GLIC_TAB_SUB_MENU_MODEL_H_
