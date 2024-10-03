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
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_proxy.h"
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
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace tab_groups {
namespace {

// The maximum number of buttons (excluding the overflow menu button) that can
// appear in the SavedTabGroupBar.
constexpr int kMaxVisibleButtons = 4;
// The amount of padding between elements listed in the overflow menu.
const int kOverflowMenuButtonPadding = 8;
// The padding at the top and bottom of the bar used to center all displayed
// buttons.
constexpr int kButtonPadding = 4;
// The amount of padding between buttons in the view.
constexpr int kBetweenElementSpacing = 8;
// The thickness, in dips, of the drop indicators during drop sessions.
constexpr int kDropIndicatorThicknessDips = 2;
}  // namespace

// OverflowMenu generally handles drop sessions by delegating to `parent_bar_`.
// Important lifecycle note: when the drop session moves from the bar to the
// overflow menu or vice versa, the state for tracking it in the bar will be
// destroyed and recreated.
class SavedTabGroupBar::OverflowMenu : public views::View {
  METADATA_HEADER(OverflowMenu, views::View)

 public:
  explicit OverflowMenu(SavedTabGroupBar& parent_bar)
      : parent_bar_(parent_bar) {}

  ~OverflowMenu() override = default;

  bool GetDropFormats(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) override {
    return parent_bar_->GetDropFormats(formats, format_types);
  }

  bool AreDropTypesRequired() override {
    return parent_bar_->AreDropTypesRequired();
  }

  bool CanDrop(const OSExchangeData& data) override {
    return parent_bar_->CanDrop(data);
  }

  void OnDragEntered(const ui::DropTargetEvent& event) override {
    parent_bar_->OnDragEntered(event);
  }

  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    // Convert the event location into `parent_bar_`'s coordinate space.
    const gfx::Point screen_loc = ConvertPointToScreen(this, event.location());
    const gfx::Point bar_loc =
        ConvertPointFromScreen(base::to_address(parent_bar_), screen_loc);
    ui::DropTargetEvent event_copy(event);
    event_copy.set_location(bar_loc);

    return parent_bar_->OnDragUpdated(event_copy);
  }

  void OnDragExited() override { parent_bar_->OnDragExited(); }

  void OnDragDone() override { parent_bar_->OnDragDone(); }

  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override {
    return parent_bar_->GetDropCallback(event);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    MaybePaintDropIndicatorInOverflow(canvas);
  }

  void MaybePaintDropIndicatorInOverflow(gfx::Canvas* canvas) {
    const std::optional<int> overflow_menu_indicator_index =
        CalculateDropIndicatorIndexInOverflow();
    if (!overflow_menu_indicator_index.has_value()) {
      return;
    }

    const int y = overflow_menu_indicator_index.value() > 0
                      ? children()[overflow_menu_indicator_index.value() - 1]
                                ->bounds()
                                .bottom() +
                            kOverflowMenuButtonPadding / 2
                      : kDropIndicatorThicknessDips / 2;

    const gfx::Rect drop_indicator_bounds =
        gfx::Rect(0, y - kDropIndicatorThicknessDips / 2, width(),
                  kDropIndicatorThicknessDips);
    canvas->FillRect(drop_indicator_bounds,
                     GetColorProvider()->GetColor(kColorBookmarkBarForeground));
  }

  // Returns the index within the overflow menu the drop indicator should be
  // painted at, or nullopt if no indicator should be painted.
  std::optional<int> CalculateDropIndicatorIndexInOverflow() {
    const std::optional<int> indicator_index =
        parent_bar_->CalculateDropIndicatorIndexInCombinedSpace();
    if (!indicator_index.has_value()) {
      return std::nullopt;
    }

    const int overflow_menu_indicator_index =
        indicator_index.value() - parent_bar_->GetNumberOfVisibleGroups();
    if (overflow_menu_indicator_index < 0) {
      // The drop index is not in the overflow menu. No drop indicator.
      return std::nullopt;
    }

    const bool came_from_bar =
        parent_bar_->GetIndexOfGroup(parent_bar_->drag_data_->guid()).value()
        << parent_bar_->GetNumberOfVisibleGroups();
    if (overflow_menu_indicator_index == 0 && came_from_bar) {
      // The drop index is on the border between the overflow menu and the bar,
      // and because the group came from the bar, it will stay in the bar.
      return std::nullopt;
    }

    return overflow_menu_indicator_index;
  }

 private:
  // The SavedTabGroupBar that this menu is associated with.
  raw_ref<SavedTabGroupBar> parent_bar_;
};

BEGIN_METADATA(SavedTabGroupBar, OverflowMenu)
END_METADATA

SavedTabGroupBar::SavedTabGroupBar(Browser* browser,
                                   TabGroupSyncService* tab_group_service,
                                   bool animations_enabled = true)
    : tab_group_service_(tab_group_service),
      browser_(browser),
      animations_enabled_(animations_enabled),
      ui_update_enabled_(IsTabGroupsSaveUIUpdateEnabled()) {
  DCHECK(browser_);
  DCHECK(tab_group_service);
  GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SAVED_TAB_GROUPS));

  SetProperty(views::kElementIdentifierKey, kSavedTabGroupBarElementId);

  // TODO(dljames): Add a container view which only houses the saved buttons.
  // The overflow will continue to be directly added to the bar.
  std::unique_ptr<views::LayoutManager> layout_manager =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(kButtonPadding, 0), kBetweenElementSpacing);
  SetLayoutManager(std::move(layout_manager));

  overflow_button_ = AddChildView(CreateOverflowButton());

  // Add the observer.
  // TODO(crbug.com/361110303): Consider consolidating logic by forwarding
  // observer in proxy.
  if (tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    tab_group_service_->AddObserver(this);
  } else {
    static_cast<TabGroupSyncServiceProxy*>(tab_group_service_)
        ->AddSavedTabGroupModelObserver(this);
  }

  HideOverflowButton();
  if (!tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    // Prevent us from adding the same groups twice when this feature is
    // enabled. When we register this view as an observer of the
    // TabGroupSyncService, the OnInitialized observer function can be called
    // after the construction of this object which also calls
    // LoadAllButtonsFromModel().
    LoadAllButtonsFromModel();
  }

  ReorderChildView(overflow_button_, children().size());

  GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SAVED_TAB_GROUPS));
}

SavedTabGroupBar::SavedTabGroupBar(Browser* browser,
                                   bool animations_enabled = true)
    : SavedTabGroupBar(browser,
                       tab_groups::SavedTabGroupUtils::GetServiceForProfile(
                           browser->profile()),
                       animations_enabled) {}

SavedTabGroupBar::~SavedTabGroupBar() {
  everything_menu_.reset();

  // Remove all buttons from the hierarchy
  RemoveAllButtons();

  // TODO(crbug.com/361110303): Consider consolidating logic by forwarding
  // observer in proxy.
  if (tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    tab_group_service_->RemoveObserver(this);
  } else {
    static_cast<TabGroupSyncServiceProxy*>(tab_group_service_)
        ->RemoveSavedTabGroupModelObserver(this);
  }
}

void SavedTabGroupBar::ShowEverythingMenu() {
  CHECK(ui_update_enabled_);
  base::RecordAction(base::UserMetricsAction(
      "TabGroups_SavedTabGroups_EverythingButtonPressed"));
  if (everything_menu_ && everything_menu_->IsShowing()) {
    return;
  }

  everything_menu_ = std::make_unique<STGEverythingMenu>(
      overflow_button_->button_controller(), browser_);
  everything_menu_->RunMenu();
}

std::optional<size_t> SavedTabGroupBar::GetIndexOfGroup(
    const base::Uuid& guid) const {
  std::vector<SavedTabGroup> groups = tab_group_service_->GetAllGroups();
  auto it = base::ranges::find_if(groups, [&](const SavedTabGroup& group) {
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
  if (overflow_menu_) {
    // `cursor_location` is in mirrored coordinates (i.e. origin in the top
    // right in RTL); ConvertPointFromScreen assumes unmirrored coordinates
    // (i.e. origin in the top left in RTL).
    const gfx::Point unmirrored_loc(GetMirroredXInView(cursor_location.x()),
                                    cursor_location.y());
    const gfx::Point overflow_menu_cursor_loc = ConvertPointFromScreen(
        overflow_menu_, ConvertPointToScreen(this, unmirrored_loc));
    // Re-mirroring is unnecessary, because we only care about y-coordinates
    // after Contains (which wouldn't be affected by re-mirroring anyways).
    if (overflow_menu_->bounds().Contains(overflow_menu_cursor_loc)) {
      drop_index = get_drop_index(overflow_menu_, overflow_menu_cursor_loc,
                                  /* vertical= */ true) +
                   GetNumberOfVisibleGroups();
    }
  }

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
  if (overflow_menu_) {
    overflow_menu_->SchedulePaint();
  }
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

  // Since v2 do not support dragging tab group into overflow menu, we only need
  // to show the menu for v1;
  if (!ui_update_enabled_) {
    const bool dragging_over_button =
        overflow_button_->GetVisible() &&
        mirrored_x >= overflow_button_->bounds().x();
    const bool would_drop_into_overflow =
        GetDropIndex() >= static_cast<size_t>(GetNumberOfVisibleGroups());

    if (dragging_over_button || would_drop_into_overflow) {
      MaybeShowOverflowMenu();
    }
  }

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

void SavedTabGroupBar::SavedTabGroupAddedLocally(const base::Uuid& guid) {
  SavedTabGroupAdded(guid);
}

void SavedTabGroupBar::SavedTabGroupRemovedLocally(
    const SavedTabGroup& removed_group) {
  SavedTabGroupRemoved(removed_group.saved_guid());
}

void SavedTabGroupBar::SavedTabGroupLocalIdChanged(
    const base::Uuid& saved_group_id) {
  UpsertSavedTabGroupButton(saved_group_id);

  MaybeShowClosePromo(saved_group_id);
}

void SavedTabGroupBar::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  UpsertSavedTabGroupButton(group_guid);
}

void SavedTabGroupBar::SavedTabGroupReorderedLocally() {
  SavedTabGroupReordered();
}

void SavedTabGroupBar::SavedTabGroupReorderedFromSync() {
  SavedTabGroupReordered();
}

void SavedTabGroupBar::SavedTabGroupTabMovedLocally(
    const base::Uuid& group_guid,
    const base::Uuid& tab_guid) {
  UpsertSavedTabGroupButton(group_guid);
}

void SavedTabGroupBar::SavedTabGroupAddedFromSync(const base::Uuid& guid) {
  UpsertSavedTabGroupButton(guid);
}

void SavedTabGroupBar::SavedTabGroupRemovedFromSync(
    const SavedTabGroup& removed_group) {
  SavedTabGroupRemoved(removed_group.saved_guid());
}

void SavedTabGroupBar::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  UpsertSavedTabGroupButton(group_guid);
}

void SavedTabGroupBar::OnInitialized() {
  RemoveAllChildViews();
  LoadAllButtonsFromModel();
  overflow_button_ = AddChildView(CreateOverflowButton());
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

void SavedTabGroupBar::OnTabGroupsReordered(TriggerSource source) {
  SavedTabGroupReordered();
}

void SavedTabGroupBar::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
  overflow_menu_ = nullptr;
  bubble_delegate_ = nullptr;
}

void SavedTabGroupBar::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  // The box layout manager has applied a vertical inset of `kButtonPadding` to
  // the `overflow_button_`. This is not what we want. So manually layout
  // `overflow_button_` again to make sure it gets the same height as `this`
  // height and vertically centered.
  const auto overflow_button_bounds = overflow_button_->bounds();
  overflow_button_->SetBounds(overflow_button_bounds.x(), 0,
                              overflow_button_bounds.width(), height());

  const int stg_bar_width = width();
  bool should_show_overflow = ShouldShowOverflowButtonForWidth(stg_bar_width);
  const int overflow_button_width =
      overflow_button_->GetPreferredSize().width() + kBetweenElementSpacing;

  if (ui_update_enabled_) {
    if (stg_bar_width == 0) {
      return;
    } else {
      CHECK(stg_bar_width >= overflow_button_width);
      should_show_overflow = true;
    }
  }

  const int last_visible_button_index = CalculateLastVisibleButtonIndexForWidth(
      stg_bar_width - (should_show_overflow ? overflow_button_width : 0));
  UpdateButtonVisibilities(should_show_overflow, last_visible_button_index);
  UpdateOverflowMenu();
}

int SavedTabGroupBar::V2CalculatePreferredWidthRestrictedBy(
    int max_width) const {
  DCHECK(ui_update_enabled_);

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

int SavedTabGroupBar::CalculatePreferredWidthRestrictedBy(int max_width) const {
  if (ui_update_enabled_) {
    return V2CalculatePreferredWidthRestrictedBy(max_width);
  }

  // Early return if the only button is the overflow button. It should be
  // invisible in this case. Happens when saved tab groups is enabled and no
  // groups are saved yet.
  if (overflow_button_ == children()[0]) {
    return 0;
  }

  // Denotes whether or not the overflow button should be shown. This is true
  // when there are strictly greater than kMaxVisibleButtons buttons OR when the
  // kMaxVisibleButtons buttons do not fit in the space provided.
  const bool should_show_overflow = ShouldShowOverflowButtonForWidth(max_width);
  const int overflow_button_width =
      kBetweenElementSpacing + overflow_button_->GetPreferredSize().width();

  // Reserve space for the overflow button.
  if (should_show_overflow) {
    max_width -= overflow_button_width;
  }

  const int last_visible_button_index =
      CalculateLastVisibleButtonIndexForWidth(max_width);

  int width = should_show_overflow ? overflow_button_width : 0;
  for (int i = 0; i <= last_visible_button_index; ++i) {
    width += children()[i]->GetPreferredSize().width() + kBetweenElementSpacing;
  }

  return width;
}

bool SavedTabGroupBar::IsOverflowButtonVisible() {
  return overflow_button_ && overflow_button_->GetVisible();
}

void SavedTabGroupBar::AddTabGroupButton(const SavedTabGroup& group,
                                         int index) {
  // Do not add unpinned tab group for v2.
  if (ui_update_enabled_ && !group.is_pinned()) {
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
          browser_, animations_enabled_),
      clamped_index);
  if (group.saved_tabs().size() == 0) {
    view->SetVisible(false);
  }
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
  bool should_have_a_button = (!ui_update_enabled_ || group->is_pinned()) &&
                              !group->saved_tabs().empty();

  if (currently_has_a_button && should_have_a_button) {
    button->UpdateButtonData(group.value());
  } else if (!currently_has_a_button && should_have_a_button) {
    AddTabGroupButton(group.value(), ui_update_enabled_ ? 0 : index.value());
  } else if (currently_has_a_button && !should_have_a_button) {
    RemoveChildViewT(button);
  }

  InvalidateLayout();
}

void SavedTabGroupBar::SavedTabGroupReordered() {
  // Selection sort the buttons to match the model's order.
  std::unordered_map<std::string, SavedTabGroupButton*> buttons_by_guid;
  for (views::View* child : children()) {
    SavedTabGroupButton* button =
        views::AsViewClass<SavedTabGroupButton>(child);
    if (button) {
      buttons_by_guid[button->guid().AsLowercaseString()] = button;
    }
  }

  const std::vector<SavedTabGroup>& groups = tab_group_service_->GetAllGroups();
  for (size_t i = 0; i < groups.size(); ++i) {
    const std::string guid = groups[i].saved_guid().AsLowercaseString();
    if (base::Contains(buttons_by_guid, guid)) {
      views::View* const button = buttons_by_guid[guid];
      ReorderChildView(button, i);
    }
  }

  // Ensure the overflow button is the last button in the view hierarchy.
  ReorderChildView(overflow_button_, children().size());

  InvalidateLayout();
}

void SavedTabGroupBar::LoadAllButtonsFromModel() {
  const std::vector<SavedTabGroup>& saved_tab_groups =
      tab_group_service_->GetAllGroups();

  for (size_t index = 0; index < saved_tab_groups.size(); index++) {
    AddTabGroupButton(saved_tab_groups[index], index);
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
    tab_group_service_->OpenTabGroup(
        group->saved_guid(),
        std::make_unique<TabGroupActionContextDesktop>(
            browser_, OpeningSource::kOpenedFromRevisitUi));
  }
}

std::unique_ptr<SavedTabGroupOverflowButton>
SavedTabGroupBar::CreateOverflowButton() {
  return std::make_unique<SavedTabGroupOverflowButton>(base::BindRepeating(
      ui_update_enabled_ ? &SavedTabGroupBar::ShowEverythingMenu
                         : &SavedTabGroupBar::MaybeShowOverflowMenu,
      base::Unretained(this)));
}

void SavedTabGroupBar::MaybeShowOverflowMenu() {
  // Don't show the menu if it's already showing.
  if (overflow_menu_) {
    return;
  }

  auto overflow_menu = std::make_unique<OverflowMenu>(*this);
  overflow_menu->SetProperty(views::kElementIdentifierKey,
                             kSavedTabGroupOverflowMenuId);

  // 1. Layout the menu as a vertical list.
  const gfx::Insets insets = gfx::Insets::TLBR(16, 16, 16, 48);
  auto box = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, insets,
      kOverflowMenuButtonPadding);
  box->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);
  overflow_menu->SetLayoutManager(std::move(box));

  // 2. Create the bubble / background which will hold the overflow menu.
  // TODO(dljames): Set the background color to match the current theme.
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      overflow_button_, views::BubbleBorder::TOP_LEFT,
      views::BubbleBorder::DIALOG_SHADOW, true);
  bubble_delegate->set_fixed_width(200);
  bubble_delegate->set_margins(gfx::Insets());
  bubble_delegate->set_adjust_if_offscreen(true);
  bubble_delegate->set_close_on_deactivate(true);
  bubble_delegate->SetShowTitle(false);
  bubble_delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate->SetShowCloseButton(false);
  bubble_delegate->SetEnableArrowKeyTraversal(true);
  bubble_delegate->SetContentsView(std::move(overflow_menu));

  bubble_delegate_ = bubble_delegate.get();
  overflow_menu_ =
      views::AsViewClass<OverflowMenu>(bubble_delegate->GetContentsView());

  // 3. Populate the menu.
  UpdateOverflowMenu();

  // 4. Display the menu.
  auto* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
  widget_observation_.Observe(widget);
  widget->Show();
}

void SavedTabGroupBar::UpdateOverflowMenu() {
  // Don't update the overflow menu if it doesn't exist.
  if (!overflow_menu_) {
    return;
  }

  // Remove all existing children.
  overflow_menu_->RemoveAllChildViews();

  // Add all buttons that are not currently visible to the overflow menu.
  for (const views::View* const child : children()) {
    if (child->GetVisible() ||
        !views::IsViewClass<SavedTabGroupButton>(child)) {
      continue;
    }

    const SavedTabGroupButton* const button =
        views::AsViewClass<SavedTabGroupButton>(child);
    const std::optional<SavedTabGroup> group =
        tab_group_service_->GetGroup(button->guid());

    overflow_menu_->AddChildView(std::make_unique<SavedTabGroupButton>(
        *group,
        base::BindRepeating(&SavedTabGroupBar::OnTabGroupButtonPressed,
                            base::Unretained(this), group->saved_guid()),
        browser_, animations_enabled_));
  }

  if (overflow_menu_->GetWidget()) {
    if (overflow_menu_->children().empty()) {
      overflow_menu_->GetWidget()->Close();
    }
  }
}

void SavedTabGroupBar::HideOverflowButton() {
  overflow_button_->SetVisible(false);
}

void SavedTabGroupBar::ShowOverflowButton() {
  overflow_button_->SetVisible(true);
}

int SavedTabGroupBar::GetNumberOfVisibleGroups() const {
  int count = 0;
  for (const views::View* const child : children()) {
    if (child->GetVisible() && child != overflow_button_) {
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
  const int num_buttons = children().size() - 1;
  return CalculateLastVisibleButtonIndexForWidth(max_width) < num_buttons - 1;
}

int SavedTabGroupBar::CalculateLastVisibleButtonIndexForWidth(
    int max_width) const {
  // kMaxVisibleButtons does not apply to v2.
  const int buttons_to_consider =
      ui_update_enabled_
          ? children().size() - 1
          : std::min(children().size() - 1, size_t(kMaxVisibleButtons));
  int current_width = 0;

  // Returns an invalid index when no button is visible.
  int last_visible_button_index = -1;

  // Only consider buttons from index 0 to kMaxVisibleButtons-1 in the worst
  // case. For each button to consider, check if we have enough room to
  // display it.
  for (int i = 0; i < buttons_to_consider; ++i) {
    const int button_width = children()[i]->GetPreferredSize().width();
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

  const bool came_from_overflow_menu =
      int(GetIndexOfGroup(drag_data_->guid()).value()) >=
      GetNumberOfVisibleGroups();
  if (indicator_index.value() == GetNumberOfVisibleGroups() &&
      came_from_overflow_menu) {
    // The drop index is on the border between the overflow menu and the bar,
    // and because the group came from the overflow menu, it will stay in the
    // overflow menu.
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
  // Only show this promo with the V2 enabled flag.
  if (!tab_groups::IsTabGroupsSaveV2Enabled()) {
    return;
  }

  // Do not show close promo while the browser is closing
  if (!browser_ || browser_->IsBrowserClosing()) {
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
  browser_->window()->MaybeShowFeaturePromo(std::move(params));
}

BEGIN_METADATA(SavedTabGroupBar)
END_METADATA

}  // namespace tab_groups
