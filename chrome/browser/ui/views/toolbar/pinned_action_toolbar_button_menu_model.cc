// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/menus/simple_menu_model.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kPinnedActionToolbarUnpinElementId);

namespace {
// Returns true if the button pin state is managed through prefs instead of
// the PinnedToolbarActionsModel.
bool IsPinStateManagedByPrefs(actions::ActionId action_id) {
  return action_id == kActionSplitTab || action_id == kActionForward ||
         action_id == kActionHome;
}

// Helper function to get the pref name for a given action id.
// Only valid for actions that have their pin state managed by prefs.
std::string_view GetPrefForActionId(actions::ActionId action_id) {
  static constexpr base::fixed_flat_map<actions::ActionId, std::string_view, 3>
      action_id_to_pref =
          base::MakeFixedFlatMap<actions::ActionId, std::string_view>(
              {{kActionSplitTab, prefs::kPinSplitTabButton},
               {kActionForward, prefs::kShowForwardButton},
               {kActionHome, prefs::kShowHomeButton}});

  return action_id_to_pref.at(action_id);
}
}  // namespace

DEFINE_UI_CLASS_PROPERTY_KEY(actions::ActionId, kActionIdKey, -1)

PinnedActionToolbarButtonMenuModel::PinnedActionToolbarButtonMenuModel(
    BrowserWindowInterface* browser_interface,
    actions::ActionId action_id)
    : browser_(browser_interface), action_id_(action_id) {
  AddActionSpecificItems();
  // Add the pin/unpin and customize toolbar items.
  items_.emplace_back(kActionPinActionToToolbar, TYPE_COMMAND);
  items_.emplace_back(kActionUnpinActionFromToolbar, TYPE_COMMAND,
                      kPinnedActionToolbarUnpinElementId);
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
    const bool is_pinnable = IsPinnable();
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
  const bool is_pinned = IsPinned();
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
  if (action_id == kActionPinActionToToolbar ||
      action_id == kActionUnpinActionFromToolbar) {
    UpdatePinState(action_id);
  } else {
    GetActionItemFor(action_id)->InvokeAction();
  }
}

ui::ElementIdentifier
PinnedActionToolbarButtonMenuModel::GetElementIdentifierAt(size_t index) const {
  if (index >= items_.size()) {
    return ui::ElementIdentifier();
  }

  return items_[index].unique_id.value_or(ui::ElementIdentifier());
}

actions::ActionId PinnedActionToolbarButtonMenuModel::GetActionIdAtForTesting(
    size_t index) {
  return items_[index].action_id;
}

PinnedActionToolbarButtonMenuModel::Item::Item(Item&&) = default;
PinnedActionToolbarButtonMenuModel::Item::Item(
    actions::ActionId action_id,
    ItemType type,
    std::optional<ui::ElementIdentifier> unique_id)
    : action_id(action_id), type(type), unique_id(unique_id) {}
PinnedActionToolbarButtonMenuModel::Item::Item(ItemType type) : type(type) {}
PinnedActionToolbarButtonMenuModel::Item&
PinnedActionToolbarButtonMenuModel::Item::operator=(Item&&) = default;
PinnedActionToolbarButtonMenuModel::Item::~Item() = default;

void PinnedActionToolbarButtonMenuModel::AddActionSpecificItems() {
  if (!IsPinStateManagedByPrefs(action_id_)) {
    // If the action has child actions add those first followed by a separator.
    actions::ActionItem* action_item = GetActionItemFor(action_id_);
    CHECK(action_item);
    if (!action_item->GetChildren().empty()) {
      for (const auto& child_item : action_item->GetChildren().children()) {
        // Adding all ActionItems as Command types here, if the ActionItem
        // should be displayed as Checked that is handled in `GetTypeAt` which
        // will evaluated the ActionItem's checked state when the menu is run.
        items_.emplace_back(*child_item->GetActionId(), TYPE_COMMAND);
      }
      items_.emplace_back(TYPE_SEPARATOR);
    }
  }
}

actions::ActionItem* PinnedActionToolbarButtonMenuModel::GetActionItemFor(
    actions::ActionId id) const {
  return actions::ActionManager::Get().FindAction(
      id, browser_->GetActions()->root_action_item());
}

bool PinnedActionToolbarButtonMenuModel::IsPinnable() const {
  return IsPinStateManagedByPrefs(action_id_) ||
         GetActionItemFor(action_id_)
                 ->GetProperty(actions::kActionItemPinnableKey) ==
             std::underlying_type_t<actions::ActionPinnableState>(
                 actions::ActionPinnableState::kPinnable);
}

bool PinnedActionToolbarButtonMenuModel::IsPinned() const {
  if (IsPinStateManagedByPrefs(action_id_)) {
    return browser_->GetProfile()->GetPrefs()->GetBoolean(
        GetPrefForActionId(action_id_));
  } else {
    return PinnedToolbarActionsModel::Get(browser_->GetProfile())
        ->Contains(action_id_);
  }
}

void PinnedActionToolbarButtonMenuModel::UpdatePinState(
    actions::ActionId pin_unpin_action) {
  const bool should_pin = pin_unpin_action == kActionPinActionToToolbar;
  if (IsPinStateManagedByPrefs(action_id_)) {
    PrefService* const pref_service = browser_->GetProfile()->GetPrefs();
    pref_service->SetBoolean(GetPrefForActionId(action_id_), should_pin);
  } else {
    GetActionItemFor(pin_unpin_action)
        ->InvokeAction(actions::ActionInvocationContext::Builder()
                           .SetProperty(kActionIdKey, action_id_)
                           .Build());
  }

  const std::optional<std::string> metrics_name =
      actions::ActionIdMap::ActionIdToString(action_id_);
  CHECK(metrics_name.has_value());
  base::RecordComputedAction(base::StrCat(
      {"Actions.PinnedToolbarButton.", should_pin ? "Pinned" : "Unpinned",
       ".ByContextMenu.", metrics_name.value()}));
}
