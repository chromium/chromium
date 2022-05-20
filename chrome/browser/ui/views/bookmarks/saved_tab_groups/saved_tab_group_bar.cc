// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"
#include <algorithm>

#include "base/bind.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/box_layout.h"

namespace {
SavedTabGroupModel* GetSavedTabGroupModelFromBrowser(Browser* browser) {
  SavedTabGroupKeyedService* keyed_service =
      SavedTabGroupServiceFactory::GetForProfile(browser->profile());
  return keyed_service ? keyed_service->model() : nullptr;
}
}  // namespace

SavedTabGroupBar::SavedTabGroupBar(Browser* browser,
                                   SavedTabGroupModel* saved_tab_group_model,
                                   bool animations_enabled = true)
    : saved_tab_group_model_(saved_tab_group_model),
      browser_(browser),
      animations_enabled_(animations_enabled) {
  std::unique_ptr<views::LayoutManager> layout_manager =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          GetLayoutConstant(TOOLBAR_ELEMENT_PADDING));
  SetLayoutManager(std::move(layout_manager));

  if (saved_tab_group_model_) {
    saved_tab_group_model_->AddObserver(this);
    const std::vector<SavedTabGroup>& saved_tab_groups =
        saved_tab_group_model_->saved_tab_groups();
    for (size_t index = 0; index < saved_tab_groups.size(); index++) {
      AddTabGroupButton(saved_tab_groups[index], index);
    }
  }
}

SavedTabGroupBar::SavedTabGroupBar(Browser* browser,
                                   bool animations_enabled = true)
    : SavedTabGroupBar(browser,
                       GetSavedTabGroupModelFromBrowser(browser),
                       animations_enabled) {}

SavedTabGroupBar::~SavedTabGroupBar() {
  // remove all buttons from the heirarchy
  RemoveAllButtons();

  if (saved_tab_group_model_)
    saved_tab_group_model_->RemoveObserver(this);
}

void SavedTabGroupBar::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_SAVED_TAB_GROUPS));
}

void SavedTabGroupBar::SavedTabGroupAdded(const SavedTabGroup& group,
                                          int index) {
  AddTabGroupButton(group, index);
  PreferredSizeChanged();
}

void SavedTabGroupBar::SavedTabGroupRemoved(int index) {
  RemoveTabGroupButton(index);
  PreferredSizeChanged();
}

void SavedTabGroupBar::SavedTabGroupUpdated(const SavedTabGroup& group,
                                            int index) {
  RemoveTabGroupButton(index);
  AddTabGroupButton(group, index);
  PreferredSizeChanged();
}

void SavedTabGroupBar::SavedTabGroupMoved(const SavedTabGroup& group,
                                          int old_index,
                                          int new_index) {
  ReorderChildView(children().at(old_index), new_index);
  PreferredSizeChanged();
}

// TODO dpenning: Support the state of the SavedTabGroup open in a tab strip
// changing.
void SavedTabGroupBar::SavedTabGroupClosed(int index) {}

void SavedTabGroupBar::AddTabGroupButton(const SavedTabGroup& group,
                                         int index) {
  // Check that the index is valid for buttons
  DCHECK_LE(index, static_cast<int>(children().size()));

  // TODO dpenning: Find the open tab group in one of the browser linked to the
  // profile of the SavedTabGroupModel. if there is one then set the highlight
  // for the button.
  AddChildViewAt(
      std::make_unique<SavedTabGroupButton>(
          group,
          base::BindRepeating(&SavedTabGroupBar::page_navigator,
                              base::Unretained(this)),
          base::BindRepeating(&SavedTabGroupBar::OnTabGroupButtonPressed,
                              base::Unretained(this), group.group_id),
          /*is_group_in_tabstrip*/ false, animations_enabled_),
      index);
}

void SavedTabGroupBar::RemoveTabGroupButton(int index) {
  // Check that the index is valid for buttons
  DCHECK_LT(index, static_cast<int>(children().size()));

  RemoveChildViewT(children().at(index));
}

void SavedTabGroupBar::RemoveAllButtons() {
  for (int index = children().size() - 1; index >= 0; index--)
    RemoveChildViewT(children().at(index));
}

void SavedTabGroupBar::OnTabGroupButtonPressed(
    const tab_groups::TabGroupId& group_id,
    const ui::Event& event) {
  DCHECK(saved_tab_group_model_ && saved_tab_group_model_->Contains(group_id));

  const SavedTabGroup* group = saved_tab_group_model_->Get(group_id);

  // TODO: Handle click if group has already been opened (crbug.com/1238539)
  // left click on a saved tab group opens all links in new group
  if (event.flags() & ui::EF_LEFT_MOUSE_BUTTON) {
    if (group->saved_tabs.empty())
      return;
    chrome::OpenSavedTabGroup(browser_, GetPageNavigatorGetter(), group,
                              WindowOpenDisposition::NEW_BACKGROUND_TAB);
  }
}

base::RepeatingCallback<content::PageNavigator*()>
SavedTabGroupBar::GetPageNavigatorGetter() {
  auto getter = [](base::WeakPtr<SavedTabGroupBar> saved_tab_group_bar)
      -> content::PageNavigator* {
    if (!saved_tab_group_bar)
      return nullptr;
    return saved_tab_group_bar->page_navigator_;
  };
  return base::BindRepeating(getter, weak_ptr_factory_.GetWeakPtr());
}

int SavedTabGroupBar::CalculatePreferredWidthRestrictedBy(int max_x) {
  const int button_padding = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  int current_x = 0;
  // iterate through the list of buttons in the child views
  for (auto* button : children()) {
    gfx::Size preferred_size = button->GetPreferredSize();
    int next_x = current_x + preferred_size.width() + button_padding;
    if (next_x > max_x)
      return current_x;
    current_x = next_x;
  }
  return current_x;
}
