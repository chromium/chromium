// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <unordered_map>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/types/to_address.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_drag_data.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_overflow_button.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_outsets_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace tab_groups {
namespace {

// The padding at the top and bottom of the bar used to center all displayed
// buttons.
constexpr int kButtonPadding = 4;
// The thickness, in dips, of the drop indicators during drop sessions.
constexpr int kDropIndicatorThicknessDips = 2;

}  // namespace

SavedTabGroupBar::SavedTabGroupBar(BrowserWindowInterface* browser,
                                   TabGroupSyncService* tab_group_service,
                                   bool animations_enabled)
    : tab_group_service_(tab_group_service),
      browser_(browser),
      animations_enabled_(animations_enabled) {
  DCHECK(browser_);
  DCHECK(tab_group_service);
  GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SAVED_TAB_GROUPS));

  SetProperty(views::kElementIdentifierKey, kSavedTabGroupBarElementId);

  // Removed the insets for vertical padding from the layout manager.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kBetweenElementSpacing));

  overflow_button_ = AddChildView(CreateOverflowButton());

  // When registering an observer, the TabGroupServiceImpl calls OnInitialized
  // if it's already prepared to take input.
  tab_group_service_->AddObserver(this);
}

SavedTabGroupBar::SavedTabGroupBar(BrowserWindowInterface* browser,
                                   bool animations_enabled)
    : SavedTabGroupBar(browser,
                       tab_groups::TabGroupSyncServiceFactory::GetForProfile(
                           browser->GetProfile()),
                       animations_enabled) {}

SavedTabGroupBar::~SavedTabGroupBar() {
  tab_group_service_->RemoveObserver(this);
}

std::optional<size_t> SavedTabGroupBar::GetIndexOfGroup(
    const base::Uuid& guid) const {
  std::vector<SavedTabGroup> groups = tab_group_service_->GetAllGroups();
  auto it = std::ranges::find_if(groups, [&](const SavedTabGroup& group) {
    return group.saved_guid() == guid;
  });

  if (it == groups.end()) {
    return std::nullopt;
  }

  return std::distance(groups.begin(), it);
}

void SavedTabGroupBar::UpdateDropIndex() {
  const gfx::Point cursor_location = drag_data_->location().value();
  const base::Uuid dragged_group_guid = drag_data_->guid();

  // Calculates the index in `parent` that a group dragged to `location` should
  // be dropped at. `vertical` should be true when the buttons in `parent` are
  // laid out vertically, and false if they are horizontally laid out.
  // TODO(tbergquist): Use Rect::ManhattanDistanceToPoint instead of `vertical`.
  const auto get_drop_index = [](const views::View* parent, gfx::Point location,
                                 bool vertical) {
    size_t i = 0;
    for (const views::View* const child : parent->children()) {
      const SavedTabGroupButton* const button =
          views::AsViewClass<SavedTabGroupButton>(child);
      // Skip non-button views, or buttons that are not shown.
      if (!button || !button->GetVisible()) {
        continue;
      }

      // We want to drop in front of the first button that's positioned after
      // `cursor_location`.
      if (vertical ? button->bounds().CenterPoint().y() > location.y()
                   : button->bounds().CenterPoint().x() > location.x()) {
        return i;
      }
      i++;
    }
    // If no buttons were after `cursor_location`, drop at the end.
    return i;
  };

  const std::optional<size_t> current_index =
      GetIndexOfGroup(dragged_group_guid);

  std::optional<size_t> drop_index = std::nullopt;

  if (!drop_index.has_value()) {
    drop_index = get_drop_index(this, cursor_location, false);
  }

  // If the dragged group is going from left to right within the model (i.e. if
  // `desired_index` > `current_index`), we must account for all of the groups
  // between the two indices shifting by 1 when the group is moved from
  // `current_index`.
  if (current_index.has_value() && current_index < drop_index) {
    drop_index = drop_index.value() - 1;
  }

  CHECK_LT(drop_index.value(), tab_group_service_->GetAllGroups().size());
  drag_data_->SetInsertionIndex(drop_index);
  SchedulePaint();
}

std::optional<size_t> SavedTabGroupBar::GetDropIndex() const {
  if (!drag_data_ || !drag_data_->insertion_index()) {
    return std::nullopt;
  }

  CHECK_LT(drag_data_->insertion_index().value(),
           tab_group_service_->GetAllGroups().size());
  return drag_data_->insertion_index();
}

void SavedTabGroupBar::HandleDrop() {
  tab_group_service_->UpdateGroupPosition(drag_data_->guid(), std::nullopt,
                                          GetDropIndex().value());
  drag_data_.reset();
  SchedulePaint();
}

bool SavedTabGroupBar::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  format_types->insert(SavedTabGroupDragData::GetFormatType());
  return true;
}

bool SavedTabGroupBar::AreDropTypesRequired() {
  return true;
}

bool SavedTabGroupBar::CanDrop(const OSExchangeData& data) {
  std::optional<SavedTabGroupDragData> drag_data =
      SavedTabGroupDragData::ReadFromOSExchangeData(&data);
  if (!drag_data.has_value()) {
    return false;
  }

  return tab_group_service_->GetGroup(drag_data.value().guid()).has_value();
}

void SavedTabGroupBar::OnDragEntered(const ui::DropTargetEvent& event) {
  std::optional<SavedTabGroupDragData> drag_data =
      SavedTabGroupDragData::ReadFromOSExchangeData(&(event.data()));
  CHECK(drag_data.has_value());

  drag_data_ = std::make_unique<SavedTabGroupDragData>(drag_data.value());

  const int mirrored_x = GetMirroredXInView(event.location().x());
  drag_data_->SetLocation(gfx::Point(mirrored_x, event.location().y()));
  UpdateDropIndex();
}

int SavedTabGroupBar::OnDragUpdated(const ui::DropTargetEvent& event) {
  // Event locations are in unmirrored coordinates (i.e. origin in the top left,
  // even in RTL). Mirror the location so the rest of the calculations can take
  // place in the standard mirrored coordinate space (i.e. origin in the top
  // right in RTL).
  const int mirrored_x = GetMirroredXInView(event.location().x());
  drag_data_->SetLocation(gfx::Point(mirrored_x, event.location().y()));
  UpdateDropIndex();

  return ui::DragDropTypes::DRAG_MOVE;
}

void SavedTabGroupBar::OnDragExited() {
  drag_data_.reset();
  SchedulePaint();
}

views::View::DropCallback SavedTabGroupBar::GetDropCallback(
    const ui::DropTargetEvent& event) {
  return base::BindOnce(
      [](SavedTabGroupBar* bar, const ui::DropTargetEvent& event,
         ui::mojom::DragOperation& drag,
         std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
        bar->HandleDrop();
        drag = ui::mojom::DragOperation::kMove;
      },
      base::Unretained(this));
}

void SavedTabGroupBar::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  MaybePaintDropIndicatorInBar(canvas);
}

void SavedTabGroupBar::OnInitialized() {
  RemoveAllChildViews();
  overflow_button_ = AddChildView(CreateOverflowButton());
  LoadAllButtonsFromModel();
  InvalidateLayout();
}

void SavedTabGroupBar::OnTabGroupAdded(const SavedTabGroup& group,
                                       TriggerSource source) {
  UpsertSavedTabGroupButton(group.saved_guid());
}

void SavedTabGroupBar::OnTabGroupUpdated(const SavedTabGroup& group,
                                         TriggerSource source) {
  UpsertSavedTabGroupButton(group.saved_guid());
}

void SavedTabGroupBar::OnTabGroupLocalIdChanged(
    const base::Uuid& sync_id,
    const std::optional<LocalTabGroupID>& local_id) {
  UpsertSavedTabGroupButton(sync_id);
  MaybeShowClosePromo(sync_id);
}

void SavedTabGroupBar::OnTabGroupRemoved(const base::Uuid& sync_id,
                                         TriggerSource source) {
  SavedTabGroupRemoved(sync_id);
}

void SavedTabGroupBar::OnTabGroupMigrated(const SavedTabGroup& new_group,
                                          const base::Uuid& old_sync_id,
                                          TriggerSource source) {
  SavedTabGroupRemoved(old_sync_id);
  UpsertSavedTabGroupButton(new_group.saved_guid());
}

void SavedTabGroupBar::OnTabGroupsReordered(TriggerSource source) {
  SavedTabGroupReordered();
}

void SavedTabGroupBar::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
  bubble_delegate_ = nullptr;
}

void SavedTabGroupBar::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  // If the everything_menu_button_ doesnt exist yet, then there's nothing to
  // layout yet.
  if (!overflow_button_) {
    return;
  }

  const int stg_bar_width = width();
  bool should_show_overflow = ShouldShowOverflowButtonForWidth(stg_bar_width);
  const int overflow_button_width =
      should_show_overflow ? overflow_button_->GetPreferredSize().width() +
                                 kBetweenElementSpacing
                           : 0;

  const int last_visible_button_index = CalculateLastVisibleButtonIndexForWidth(
      stg_bar_width - overflow_button_width);
  UpdateButtonVisibilities(should_show_overflow, last_visible_button_index);
}

int SavedTabGroupBar::CalculatePreferredWidthRestrictedBy(int max_width) const {
  // If the everything_menu_button_ doesnt exist yet, then there's nothing to
  // layout yet.
  if (!overflow_button_) {
    return 0;
  }
  // For V2, the preferred width of Saved tab groups bar depends on the number
  // of pinned tab groups (pinned state is WIP) in bookmark bar (plus Everything
  // button);
  // TODO(crbug.com/329659664): Refactor this method once pinned state is done
  // and add tests.

  // Everything button always shows for V2.
  int width =
      overflow_button_->GetPreferredSize().width() + kBetweenElementSpacing;
  max_width -= width;
  if (max_width < 0) {
    return 0;
  }

  // Add all the visible buttons width to result.
  const int last_visible_button_index =
      CalculateLastVisibleButtonIndexForWidth(max_width);
  for (int i = 0; i <= last_visible_button_index; ++i) {
    width += children()[i]->GetPreferredSize().width() + kBetweenElementSpacing;
  }
  return width;
}

bool SavedTabGroupBar::IsOverflowButtonVisible() const {
  return overflow_button_ && overflow_button_->GetVisible();
}

void SavedTabGroupBar::AddTabGroupButton(const SavedTabGroup& group,
                                         int index) {
  // Do not add unpinned tab group for v2.
  if (!group.is_pinned()) {
    return;
  }

  // Ensure the button is placed within the bounds of children(). The last
  // button is always reserved for the everything / overflow menu.
  int num_buttons = static_cast<int>(children().size());
  int clamped_index =
      index < num_buttons ? index : std::max(0, num_buttons - 1);
  views::View* view = AddChildViewAt(
      std::make_unique<SavedTabGroupButton>(
          group,
          base::BindRepeating(&SavedTabGroupBar::OnTabGroupButtonPressed,
                              base::Unretained(this), group.saved_guid()),
          browser_->GetBrowserForMigrationOnly(), animations_enabled_),
      clamped_index);
  view->SetProperty(views::kMarginsKey, gfx::Insets::VH(kButtonPadding, 0));
  if (group.saved_tabs().size() == 0) {
    view->SetVisible(false);
  }
}

void SavedTabGroupBar::ShowEverythingMenu() {
  base::RecordAction(base::UserMetricsAction(
      "TabGroups_SavedTabGroups_EverythingButtonPressed"));
  if (everything_menu_ && everything_menu_->IsShowing()) {
    return;
  }

  everything_menu_ = std::make_unique<STGEverythingMenu>(
      overflow_button_->button_controller(),
      browser_->GetBrowserForMigrationOnly(),
      STGEverythingMenu::MenuContext::kSavedTabGroupBar);

  everything_menu_->RunMenu();
}

void SavedTabGroupBar::SavedTabGroupAdded(const base::Uuid& guid) {
  UpsertSavedTabGroupButton(guid);
}

void SavedTabGroupBar::SavedTabGroupRemoved(const base::Uuid& guid) {
  RemoveTabGroupButton(guid);

  InvalidateLayout();
}

void SavedTabGroupBar::UpsertSavedTabGroupButton(const base::Uuid& guid) {
  std::optional<int> index = GetIndexOfGroup(guid);
  if (!index.has_value()) {
    return;
  }

  const std::optional<SavedTabGroup> group = tab_group_service_->GetGroup(guid);
  CHECK(group.has_value());
  SavedTabGroupButton* button =
      views::AsViewClass<SavedTabGroupButton>(GetButton(group->saved_guid()));

  bool currently_has_a_button = button != nullptr;
  bool should_have_a_button =
      group->is_pinned() && !group->saved_tabs().empty();

  if (currently_has_a_button && should_have_a_button) {
    button->UpdateButtonData(group.value());
  } else if (!currently_has_a_button && should_have_a_button) {
    AddTabGroupButton(group.value(), 0);
  } else if (currently_has_a_button && !should_have_a_button) {
    RemoveChildViewT(button);
  }

  SavedTabGroupReordered();
}

void SavedTabGroupBar::SavedTabGroupReordered() {
  std::unordered_map<base::Uuid, SavedTabGroupButton*, base::UuidHash>
      buttons_by_guid;
  for (views::View* child : children()) {
    SavedTabGroupButton* button =
        views::AsViewClass<SavedTabGroupButton>(child);
    if (button) {
      buttons_by_guid[button->guid()] = button;
    }
  }

  // Assuming ReadAllGroups should return the groups in the correct order.
  const std::vector<const SavedTabGroup*> groups =
      tab_group_service_->ReadAllGroups();
  for (size_t i = 0; i < groups.size(); ++i) {
    const base::Uuid& guid = groups[i]->saved_guid();

    if (base::Contains(buttons_by_guid, guid)) {
      views::View* const button = buttons_by_guid[guid];
      ReorderChildView(button, i);
    }
  }

  if (overflow_button_) {
    // Ensure the overflow button is the last button in the view hierarchy.
    ReorderChildView(overflow_button_, children().size());
  }

  InvalidateLayout();
}

void SavedTabGroupBar::LoadAllButtonsFromModel() {
  const std::vector<const SavedTabGroup*> groups =
      tab_group_service_->ReadAllGroups();

  for (size_t index = 0; index < groups.size(); index++) {
    AddTabGroupButton(*groups[index], index);
  }
}

void SavedTabGroupBar::RemoveTabGroupButton(const base::Uuid& guid) {
  // Make sure we have a valid button before trying to remove it.
  views::View* button = GetButton(guid);
  if (button) {
    RemoveChildViewT(button);
  }
}

void SavedTabGroupBar::RemoveAllButtons() {
  for (int index = children().size() - 1; index >= 0; index--) {
    RemoveChildViewT(children().at(index));
  }
}

views::View* SavedTabGroupBar::GetButton(const base::Uuid& guid) {
  for (views::View* child : children()) {
    if (views::IsViewClass<SavedTabGroupButton>(child) &&
        views::AsViewClass<SavedTabGroupButton>(child)->guid() == guid) {
      return child;
    }
  }

  return nullptr;
}

void SavedTabGroupBar::OnTabGroupButtonPressed(const base::Uuid& id,
                                               const ui::Event& event) {
  DCHECK(tab_group_service_ && tab_group_service_->GetGroup(id).has_value());
  const std::optional<SavedTabGroup> group = tab_group_service_->GetGroup(id);

  if (group->saved_tabs().empty()) {
    return;
  }

  bool space_pressed = event.IsKeyEvent() && event.AsKeyEvent()->key_code() ==
                                                 ui::KeyboardCode::VKEY_SPACE;

  bool left_mouse_button_pressed = event.flags() & ui::EF_LEFT_MOUSE_BUTTON;

  if (left_mouse_button_pressed || space_pressed) {
    if (base::FeatureList::IsEnabled(features::kTabGroupMenuImprovements)) {
      // Open the context menu.
      SavedTabGroupButton* saved_tab_group_button =
          views::AsViewClass<SavedTabGroupButton>(
              GetButton(group->saved_guid()));
      CHECK(saved_tab_group_button);

      gfx::Point coordinates;
      ui::mojom::MenuSourceType source_type;
      if (left_mouse_button_pressed) {
        coordinates = ConvertPointToScreen(saved_tab_group_button,
                                           event.AsLocatedEvent()->location());
        source_type = ui::mojom::MenuSourceType::kMouse;
      } else {
        coordinates = saved_tab_group_button->GetKeyboardContextMenuLocation();
        source_type = ui::mojom::MenuSourceType::kKeyboard;
      }

      saved_tab_group_button->ShowContextMenuForView(saved_tab_group_button,
                                                     coordinates, source_type);

    } else {
      // Open the tab group on click or space.

      const bool will_open_shared_group =
          group->is_shared_tab_group() && !group->local_group_id().has_value();

      tab_group_service_->OpenTabGroup(
          group->saved_guid(), std::make_unique<TabGroupActionContextDesktop>(
                                   browser_->GetBrowserForMigrationOnly(),
                                   OpeningSource::kOpenedFromRevisitUi));
      if (will_open_shared_group) {
        saved_tab_groups::metrics::RecordSharedTabGroupRecallType(
            saved_tab_groups::metrics::SharedTabGroupRecallTypeDesktop::
                kOpenedFromBookmarksBar);
      }
    }
  }
}

std::unique_ptr<SavedTabGroupOverflowButton>
SavedTabGroupBar::CreateOverflowButton() {
  return std::make_unique<SavedTabGroupOverflowButton>(base::BindRepeating(
      &SavedTabGroupBar::ShowEverythingMenu, base::Unretained(this)));
}

int SavedTabGroupBar::GetNumberOfVisibleGroups() const {
  int count = 0;
  for (const auto* button : GetSavedTabGroupButtons()) {
    if (button->GetVisible()) {
      ++count;
    }
  }
  return count;
}

void SavedTabGroupBar::UpdateButtonVisibilities(bool show_overflow,
                                                int last_visible_button_index) {
  // Update visibilities
  overflow_button_->SetVisible(show_overflow);
  for (int i = 0; i < static_cast<int>(children().size()) - 1; ++i) {
    views::View* button = children()[i];
    button->SetVisible(i <= last_visible_button_index);
  }
}

bool SavedTabGroupBar::ShouldShowOverflowButtonForWidth(int max_width) const {
  return width() >=
         overflow_button_->GetPreferredSize().width() + kBetweenElementSpacing;
}

int SavedTabGroupBar::CalculateLastVisibleButtonIndexForWidth(
    int max_width) const {
  std::vector<SavedTabGroupButton*> buttons = GetSavedTabGroupButtons();
  int current_width = 0;
  int last_visible_button_index = -1;

  for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
    const int button_width = buttons[i]->GetPreferredSize().width();
    current_width += button_width + kBetweenElementSpacing;
    if (current_width > max_width) {
      break;
    }

    last_visible_button_index = i;
  }

  return last_visible_button_index;
}

void SavedTabGroupBar::MaybePaintDropIndicatorInBar(gfx::Canvas* canvas) {
  const std::optional<int> indicator_index = CalculateDropIndicatorIndexInBar();
  if (!indicator_index.has_value()) {
    return;
  }

  const int x =
      indicator_index.value() > 0
          ? children()[indicator_index.value() - 1]->bounds().right() +
                GetLayoutConstant(BOOKMARK_BAR_BUTTON_PADDING) / 2
          : kDropIndicatorThicknessDips / 2;

  const gfx::Rect drop_indicator_bounds =
      gfx::Rect(x - kDropIndicatorThicknessDips / 2, 0,
                kDropIndicatorThicknessDips, height());
  // `drop_indiciator_bounds` is in mirrored coordinates (i.e. origin in the
  // top right in RTL), but `FillRect` expects unmirrored coordinates (i.e.
  // origin in the top left, even in RTL).
  const gfx::Rect unmirrored_drop_indicator_bounds =
      GetMirroredRect(drop_indicator_bounds);
  canvas->FillRect(unmirrored_drop_indicator_bounds,
                   GetColorProvider()->GetColor(kColorBookmarkBarForeground));
}

std::optional<int> SavedTabGroupBar::CalculateDropIndicatorIndexInBar() const {
  const std::optional<int> indicator_index =
      CalculateDropIndicatorIndexInCombinedSpace();
  if (!indicator_index.has_value()) {
    return std::nullopt;
  }

  if (indicator_index.value() > GetNumberOfVisibleGroups()) {
    // The drop index is not in the bar.
    return std::nullopt;
  }

  return indicator_index;
}

std::optional<int>
SavedTabGroupBar::CalculateDropIndicatorIndexInCombinedSpace() const {
  if (!drag_data_ || !GetDropIndex().has_value()) {
    return std::nullopt;
  }

  const int insertion_index = GetDropIndex().value();
  const int current_index = GetIndexOfGroup(drag_data_->guid()).value();

  if (insertion_index > current_index) {
    // `insertion_index` doesn't include `current_index`, add it back in if
    // needed.
    return insertion_index + 1;
  } else if (insertion_index == current_index) {
    // Hide the indicator when the drop wouldn't reorder anything.
    return std::nullopt;
  }

  // Otherwise we can show an indicator at the actual drop index.
  return insertion_index;
}

void SavedTabGroupBar::MaybeShowClosePromo(const base::Uuid& saved_group_id) {
  // Do not show close promo while the browser is closing
  if (!browser_ || browser_->capabilities()->IsAttemptingToCloseBrowser()) {
    return;
  }

  // Only show this promo if the group exists and was closed.
  const std::optional<tab_groups::SavedTabGroup> group =
      tab_group_service_->GetGroup(saved_group_id);
  if (!group || group->local_group_id().has_value()) {
    return;
  }

  user_education::FeaturePromoParams params(
      feature_engagement::kIPHTabGroupsSaveV2CloseGroupFeature);
  BrowserUserEducationInterface::From(browser_)->MaybeShowFeaturePromo(
      std::move(params));
}

// New function implementation
std::vector<SavedTabGroupButton*> SavedTabGroupBar::GetSavedTabGroupButtons()
    const {
  std::vector<SavedTabGroupButton*> buttons;
  for (const views::View* child : children()) {
    if (auto* button = views::AsViewClass<SavedTabGroupButton>(child)) {
      buttons.push_back(const_cast<SavedTabGroupButton*>(button));
    }
  }
  return buttons;
}

BEGIN_METADATA(SavedTabGroupBar)
END_METADATA

}  // namespace tab_groups
