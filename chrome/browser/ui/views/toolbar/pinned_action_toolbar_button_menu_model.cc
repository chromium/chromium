// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"

#include <optional>
#include <string>

#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/actions/action_utils.h"
#include "ui/actions/actions.h"
#include "ui/menus/simple_menu_model.h"

DEFINE_UI_CLASS_PROPERTY_KEY(actions::ActionId, kActionIdKey, -1)

PinnedActionToolbarButtonMenuModel::PinnedActionToolbarButtonMenuModel(
    BrowserWindowInterface* browser_interface,
    actions::ActionId action_id)
    : browser_(browser_interface), action_id_(action_id) {
  // If the action has child actions add those first followed by a separator.
  actions::ActionItem* action_item = GetActionItemFor(action_id_);
  CHECK(action_item);
  if (!action_item->GetChildren().empty()) {
    for (const auto& child_item : action_item->GetChildren().children()) {
      // Adding all ActionItems as Command types here, if the ActionItem should
      // be displayed as Checked that is handled in `GetTypeAt` which will
      // evaluated the ActionItem's checked state when the menu is run.
      items_.emplace_back(*child_item->GetActionId(), TYPE_COMMAND);
    }
    items_.emplace_back(TYPE_SEPARATOR);
  }
  // Add the pin/unpin and customize toolbar items.
  items_.emplace_back(kActionPinActionToToolbar, TYPE_COMMAND);
  items_.emplace_back(kActionUnpinActionFromToolbar, TYPE_COMMAND);
  items_.emplace_back(kActionSidePanelShowCustomizeChromeToolbar, TYPE_COMMAND);
}

PinnedActionToolbarButtonMenuModel::~PinnedActionToolbarButtonMenuModel() =
    default;

base::WeakPtr<ui::MenuModel> PinnedActionToolbarButtonMenuModel::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

size_t PinnedActionToolbarButtonMenuModel::GetItemCount() const {
  return items_.size();
}

ui::MenuModel::ItemType PinnedActionToolbarButtonMenuModel::GetTypeAt(
    size_t index) const {
  if (items_[index].type == TYPE_SEPARATOR) {
    return items_[index].type;
  }

  return GetActionItemFor(items_[index].action_id)->GetChecked()
             ? TYPE_CHECK
             : items_[index].type;
}

ui::MenuSeparatorType PinnedActionToolbarButtonMenuModel::GetSeparatorTypeAt(
    size_t index) const {
  return ui::NORMAL_SEPARATOR;
}

int PinnedActionToolbarButtonMenuModel::GetCommandIdAt(size_t index) const {
  return static_cast<int>(index);
}

std::u16string PinnedActionToolbarButtonMenuModel::GetLabelAt(
    size_t index) const {
  if (GetTypeAt(index) == TYPE_SEPARATOR) {
    return std::u16string();
  }

  return std::u16string(GetActionItemFor(items_[index].action_id)->GetText());
}

bool PinnedActionToolbarButtonMenuModel::IsItemDynamicAt(size_t index) const {
  return false;
}

bool PinnedActionToolbarButtonMenuModel::GetAcceleratorAt(
    size_t index,
    ui::Accelerator* accelerator) const {
  return false;
}

bool PinnedActionToolbarButtonMenuModel::IsItemCheckedAt(size_t index) const {
  if (GetTypeAt(index) == TYPE_SEPARATOR) {
    return false;
  }

  return GetActionItemFor(items_[index].action_id)->GetChecked();
}

int PinnedActionToolbarButtonMenuModel::GetGroupIdAt(size_t index) const {
  return false;
}

ui::ImageModel PinnedActionToolbarButtonMenuModel::GetIconAt(
    size_t index) const {
  if (GetTypeAt(index) == TYPE_SEPARATOR) {
    return ui::ImageModel();
  }

  const ui::ImageModel& image =
      GetActionItemFor(items_[index].action_id)->GetImage();
  // Update the icon size for a context menu if possible.
  if (!image.IsEmpty() && image.IsVectorIcon()) {
    ui::VectorIconModel vector_icon_model = image.GetVectorIcon();
    return ui::ImageModel::FromVectorIcon(
        *vector_icon_model.vector_icon(), vector_icon_model.color(),
        ui::SimpleMenuModel::kDefaultIconSize);
  }
  return image;
}

ui::ButtonMenuItemModel*
PinnedActionToolbarButtonMenuModel::GetButtonMenuItemAt(size_t index) const {
  return nullptr;
}

bool PinnedActionToolbarButtonMenuModel::IsEnabledAt(size_t index) const {
  if (GetTypeAt(index) == TYPE_SEPARATOR) {
    return true;
  }
  if (items_[index].action_id == kActionPinActionToToolbar ||
      items_[index].action_id == kActionUnpinActionFromToolbar) {
    bool is_pinnable = GetActionItemFor(action_id_)
                           ->GetProperty(actions::kActionItemPinnableKey) ==
                       std::underlying_type_t<actions::ActionPinnableState>(
                           actions::ActionPinnableState::kPinnable);
    return browser_->GetProfile()->IsRegularProfile() && is_pinnable;
  }
  if (items_[index].action_id == kActionSidePanelShowCustomizeChromeToolbar) {
    // TODO(https://crbug.com/365591184) This check should become a static
    // method on customize chrome's side panel controller once the
    // abstract-base-class is removed and should be used to update the
    // ActionItem's enabled property.
    tabs::TabInterface* tab = browser_->GetTabStripModel()->GetActiveTab();
    customize_chrome::SidePanelController* side_panel_controller =
        tab->GetTabFeatures()->customize_chrome_side_panel_controller();
    return side_panel_controller &&
           side_panel_controller->IsCustomizeChromeEntryAvailable();
  }
  return GetActionItemFor(items_[index].action_id)->GetEnabled();
}

bool PinnedActionToolbarButtonMenuModel::IsVisibleAt(size_t index) const {
  if (GetTypeAt(index) == TYPE_SEPARATOR) {
    return true;
  }
  bool is_pinned = PinnedToolbarActionsModel::Get(browser_->GetProfile())
                       ->Contains(action_id_);
  if (is_pinned && items_[index].action_id == kActionPinActionToToolbar) {
    return false;
  }
  if (!is_pinned && items_[index].action_id == kActionUnpinActionFromToolbar) {
    return false;
  }
  return GetActionItemFor(items_[index].action_id)->GetVisible();
}

ui::MenuModel* PinnedActionToolbarButtonMenuModel::GetSubmenuModelAt(
    size_t index) const {
  return nullptr;
}

void PinnedActionToolbarButtonMenuModel::ActivatedAt(size_t index) {
  ActivatedAt(index, 0);
}

void PinnedActionToolbarButtonMenuModel::ActivatedAt(size_t index,
                                                     int event_flags) {
  DCHECK(GetTypeAt(index) != TYPE_SEPARATOR);

  auto action_id = items_[index].action_id;
  auto* action_item = GetActionItemFor(action_id);
  if (action_id == kActionPinActionToToolbar ||
      action_id == kActionUnpinActionFromToolbar) {
    action_item->InvokeAction(actions::ActionInvocationContext::Builder()
                                  .SetProperty(kActionIdKey, action_id_)
                                  .Build());
    const std::optional<std::string> metrics_name =
        actions::ActionIdMap::ActionIdToString(action_id_);
    CHECK(metrics_name.has_value());
    base::RecordComputedAction(base::StrCat(
        {"Actions.PinnedToolbarButton.",
         action_id == kActionPinActionToToolbar ? "Pinned" : "Unpinned",
         ".ByContextMenu.", metrics_name.value()}));
  } else {
    action_item->InvokeAction();
  }
}

actions::ActionId PinnedActionToolbarButtonMenuModel::GetActionIdAtForTesting(
    size_t index) {
  return items_[index].action_id;
}

PinnedActionToolbarButtonMenuModel::Item::Item(Item&&) = default;
PinnedActionToolbarButtonMenuModel::Item::Item(actions::ActionId action_id,
                                               ItemType type)
    : action_id(action_id), type(type) {}
PinnedActionToolbarButtonMenuModel::Item::Item(ItemType type) : type(type) {}
PinnedActionToolbarButtonMenuModel::Item&
PinnedActionToolbarButtonMenuModel::Item::operator=(Item&&) = default;
PinnedActionToolbarButtonMenuModel::Item::~Item() = default;

actions::ActionItem* PinnedActionToolbarButtonMenuModel::GetActionItemFor(
    actions::ActionId id) const {
  return actions::ActionManager::Get().FindAction(
      id, browser_->GetActions()->root_action_item());
}
