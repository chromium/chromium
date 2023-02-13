// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_overflow_button.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_outsets_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_utils.h"

namespace {
// The maximum number of buttons (excluding the overflow menu button) that can
// appear in the SavedTabGroupBar.
constexpr int kMaxVisibleButtons = 4;

// The amount of padding between elements listed in the overflow menu.
const int kOverflowMenuButtonPadding = 8;

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

  overflow_button_ = AddChildView(std::make_unique<SavedTabGroupOverflowButton>(
      base::BindRepeating(&SavedTabGroupBar::OnOverflowButtonPressed,
                          base::Unretained(this), this)));

  AddAllButtons();

  ReorderChildView(overflow_button_, children().size());
  HideOverflowButton();
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
    if (child == overflow_button_) {
      continue;
    }

    const absl::optional<int> model_index = saved_tab_group_model_->GetIndexOf(
        views::AsViewClass<SavedTabGroupButton>(child)->guid());
    ReorderChildView(child, model_index.value());
  }

  // Ensure the overflow button is the last button in the view hierarchy.
  ReorderChildView(overflow_button_, children().size());
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
    int next_x =
        current_x +
        (button->GetVisible() ? preferred_size.width() + button_padding : 0);
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

  if (children().size() > (kMaxVisibleButtons + 1)) {
    // Only 4 buttons + the overflow button can be visible at a time. Hide any
    // additional buttons.
    if (!overflow_button_->GetVisible()) {
      ShowOverflowButton();
    }

    auto* button = children()[index];
    button->SetVisible(false);
  } else if (overflow_button_->GetVisible()) {
    HideOverflowButton();
  }
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
  SavedTabGroupButton* button =
      views::AsViewClass<SavedTabGroupButton>(GetButton(group->saved_guid()));
  DCHECK(button);

  button->UpdateButtonData(*group);

  // Hide the button if it should not be visible.
  if (index.value() >= kMaxVisibleButtons &&
      children().size() >= (kMaxVisibleButtons + 1)) {
    button->SetVisible(false);
  } else {
    button->SetSize(button->GetPreferredSize());
    button->SetVisible(true);
  }

  if (button->GetVisible()) {
    PreferredSizeChanged();
  }
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
  const bool visible_button_removed = button->GetVisible();

  DCHECK(button);
  RemoveChildViewT(button);

  // If a visible button was removed set the next button to be visible.
  if (children().size() >= (kMaxVisibleButtons + 1)) {
    if (visible_button_removed) {
      auto* invisible_button = children()[3];
      invisible_button->SetSize(invisible_button->GetPreferredSize());
      invisible_button->SetVisible(true);
      PreferredSizeChanged();
    }

    if (children().size() == (kMaxVisibleButtons + 1)) {
      HideOverflowButton();
      PreferredSizeChanged();
    }
  }
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
    SavedTabGroupKeyedService* keyed_service =
        SavedTabGroupServiceFactory::GetForProfile(browser_->profile());

    keyed_service->OpenSavedTabGroupInBrowser(browser_, group->saved_guid());
  }
}

void SavedTabGroupBar::OnOverflowButtonPressed(views::View* view,
                                               const ui::Event& event) {
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      view, views::BubbleBorder::TOP_RIGHT);

  bubble_delegate_ = bubble_delegate.get();
  bubble_delegate_->SetShowTitle(false);
  bubble_delegate_->SetShowCloseButton(false);
  bubble_delegate_->SetButtons(ui::DIALOG_BUTTON_NONE);
  gfx::Insets insets = gfx::Insets::TLBR(16, 16, 16, 48);
  bubble_delegate_->set_margins(insets);
  bubble_delegate_->set_fixed_width(200);

  auto* overflow_menu =
      bubble_delegate_->SetContentsView(std::make_unique<views::View>());
  auto box = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kOverflowMenuButtonPadding);
  box->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);
  overflow_menu->SetLayoutManager(std::move(box));

  // Add all buttons that are not currently visible to the overflow menu
  for (auto* child : view->children()) {
    if (child->GetVisible() ||
        !views::IsViewClass<SavedTabGroupButton>(child)) {
      continue;
    }

    SavedTabGroupButton* button =
        views::AsViewClass<SavedTabGroupButton>(child);
    const SavedTabGroup* const group =
        saved_tab_group_model_->Get(button->guid());

    overflow_menu->AddChildView(std::make_unique<SavedTabGroupButton>(
        *group,
        base::BindRepeating(&SavedTabGroupBar::page_navigator,
                            base::Unretained(this)),
        base::BindRepeating(&SavedTabGroupBar::OnTabGroupButtonPressed,
                            base::Unretained(this), group->saved_guid()),
        animations_enabled_));
  }

  auto* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
  widget->Show();
  return;
}

void SavedTabGroupBar::HideOverflowButton() {
  overflow_button_->SetVisible(false);
}

void SavedTabGroupBar::ShowOverflowButton() {
  overflow_button_->SetVisible(true);
}
