// Copyright 2022 The Chromium Authors
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
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_utils.h"

namespace {
SavedTabGroupModel* GetSavedTabGroupModelFromBrowser(Browser* browser) {
  DCHECK(browser);
  SavedTabGroupKeyedService* keyed_service =
      SavedTabGroupServiceFactory::GetForProfile(browser->profile());
  return keyed_service ? keyed_service->model() : nullptr;
}
}  // namespace

// TODO(crbug/1372008): Prevent `SavedTabGroupBar` from instantiating if the
// corresponding feature flag is disabled.
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

  if (!saved_tab_group_model_)
    return;

  saved_tab_group_model_->AddObserver(this);
  AddAllButtons();
}

SavedTabGroupBar::SavedTabGroupBar(Browser* browser,
                                   bool animations_enabled = true)
    : SavedTabGroupBar(browser,
                       GetSavedTabGroupModelFromBrowser(browser),
                       animations_enabled) {}

SavedTabGroupBar::~SavedTabGroupBar() {
  // Remove all buttons from the hierarchy
  RemoveAllButtons();

  if (saved_tab_group_model_)
    saved_tab_group_model_->RemoveObserver(this);
}

void SavedTabGroupBar::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetNameChecked(
      l10n_util::GetStringUTF8(IDS_ACCNAME_SAVED_TAB_GROUPS));
}

void SavedTabGroupBar::SavedTabGroupAddedLocally(const base::GUID& guid) {
  SavedTabGroupAdded(guid);
}

void SavedTabGroupBar::SavedTabGroupRemovedLocally(
    const SavedTabGroup* removed_group) {
  SavedTabGroupRemoved(removed_group->saved_guid());
}

void SavedTabGroupBar::SavedTabGroupUpdatedLocally(
    const base::GUID& group_guid,
    const absl::optional<base::GUID>& tab_guid) {
  SavedTabGroupUpdated(group_guid);
}

void SavedTabGroupBar::SavedTabGroupReorderedLocally() {
  for (views::View* child : children()) {
    const absl::optional<int> model_index = saved_tab_group_model_->GetIndexOf(
        views::AsViewClass<SavedTabGroupButton>(child)->guid());
    ReorderChildView(child, model_index.value());
  }

  PreferredSizeChanged();
}

void SavedTabGroupBar::SavedTabGroupAddedFromSync(const base::GUID& guid) {
  SavedTabGroupAdded(guid);
}

void SavedTabGroupBar::SavedTabGroupRemovedFromSync(
    const SavedTabGroup* removed_group) {
  SavedTabGroupRemoved(removed_group->saved_guid());
}

void SavedTabGroupBar::SavedTabGroupUpdatedFromSync(
    const base::GUID& group_guid,
    const absl::optional<base::GUID>& tab_guid) {
  SavedTabGroupUpdated(group_guid);
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
                              base::Unretained(this), group.saved_guid()),
          animations_enabled_),
      index);
}

void SavedTabGroupBar::SavedTabGroupAdded(const base::GUID& guid) {
  absl::optional<int> index = saved_tab_group_model_->GetIndexOf(guid);
  if (!index.has_value())
    return;
  AddTabGroupButton(*saved_tab_group_model_->Get(guid), index.value());
  PreferredSizeChanged();
}

void SavedTabGroupBar::SavedTabGroupRemoved(const base::GUID& guid) {
  RemoveTabGroupButton(guid);
  PreferredSizeChanged();
}

void SavedTabGroupBar::SavedTabGroupUpdated(const base::GUID& guid) {
  absl::optional<int> index = saved_tab_group_model_->GetIndexOf(guid);
  if (!index.has_value())
    return;
  const SavedTabGroup* group = saved_tab_group_model_->Get(guid);
  RemoveTabGroupButton(guid);
  AddTabGroupButton(*group, index.value());
  PreferredSizeChanged();
}

void SavedTabGroupBar::AddAllButtons() {
  const std::vector<SavedTabGroup>& saved_tab_groups =
      saved_tab_group_model_->saved_tab_groups();

  for (size_t index = 0; index < saved_tab_groups.size(); index++)
    AddTabGroupButton(saved_tab_groups[index], index);
}

void SavedTabGroupBar::RemoveTabGroupButton(const base::GUID& guid) {
  // Make sure we have a valid button before trying to remove it.
  views::View* button = GetButton(guid);
  DCHECK(button);

  RemoveChildViewT(button);
}

void SavedTabGroupBar::RemoveAllButtons() {
  for (int index = children().size() - 1; index >= 0; index--)
    RemoveChildViewT(children().at(index));
}

views::View* SavedTabGroupBar::GetButton(const base::GUID& guid) {
  for (views::View* child : children()) {
    if (views::IsViewClass<SavedTabGroupButton>(child) &&
        views::AsViewClass<SavedTabGroupButton>(child)->guid() == guid)
      return child;
  }

  return nullptr;
}

void SavedTabGroupBar::OnTabGroupButtonPressed(const base::GUID& id,
                                               const ui::Event& event) {
  DCHECK(saved_tab_group_model_ && saved_tab_group_model_->Contains(id));

  const SavedTabGroup* group = saved_tab_group_model_->Get(id);

  // TODO: Handle click if group has already been opened (crbug.com/1238539)
  // left click on a saved tab group opens all links in new group
  if (event.flags() & ui::EF_LEFT_MOUSE_BUTTON) {
    if (group->saved_tabs().empty())
      return;
    chrome::OpenSavedTabGroup(browser_, group->saved_guid(),
                              group->saved_tabs().size());
  }
}
