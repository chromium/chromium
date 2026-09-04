// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"

#include <variant>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/glass_frame_service.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/common/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_accessibility.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/tabs/horizontal/horizontal_tab_closing_helper.h"
#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_context_menu_command.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/boca/on_task/on_task_locked_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

TabStripCollectionController::TabStripCollectionController(
    TabStripModel* model,
    BrowserView* browser_view,
    RootTabCollectionNode& root_node,
    TabDragHandler& drag_handler,
    TabHoverCardController* hover_card_controller,
    TabStripOrientation orientation)
    : model_(model),
      browser_view_(browser_view),
      root_node_(root_node),
      drag_handler_(drag_handler),
      hover_card_controller_(hover_card_controller) {
  CHECK(browser_view_);

  if (orientation == TabStripOrientation::kHorizontal) {
    tab_closing_helper_ =
        std::make_unique<HorizontalTabClosingHelper>(root_node);
  }

  if (GlassFrameService* service = GlassFrameService::GetInstance()) {
    glass_frame_service_subscription_ =
        service->RegisterGlassFrameEligibilityChangedCallback(
            browser_view_->browser(),
            base::BindRepeating(
                &TabStripCollectionController::OnGlassFrameEligibilityChanged,
                base::Unretained(this)));
    OnGlassFrameEligibilityChanged(
        service->IsBrowserWindowEligible(browser_view_->browser()));
  }
}

TabStripCollectionController::~TabStripCollectionController() {
  if (context_menu_controller_.get()) {
    context_menu_controller_.reset();
  }
}

int TabStripCollectionController::GetTabCount() const {
  return model_->count();
}

const tabs::TabInterface* TabStripCollectionController::GetActiveTab() const {
  return model_->GetActiveTab();
}

const TabCollectionNode* TabStripCollectionController::GetAdjacentTab(
    const tabs::TabInterface* tab,
    bool leading) const {
  std::optional<int> maybe_index = model_->GetIndexOfTab(tab);
  if (!maybe_index.has_value()) {
    return nullptr;
  }

  int adjacent_index =
      leading ? maybe_index.value() - 1 : maybe_index.value() + 1;
  if (!model_->ContainsIndex(adjacent_index)) {
    return nullptr;
  }

  const tabs::TabInterface* adjacent_tab =
      model_->GetTabAtIndex(adjacent_index);

  TabCollectionNode* adjacent_node =
      adjacent_tab ? root_node_->GetNodeForHandle(adjacent_tab->GetHandle())
                   : nullptr;
  return adjacent_node;
}

std::optional<tab_groups::TabGroupId>
TabStripCollectionController::GetFocusedGroup() const {
  return model_->GetFocusedGroup();
}

bool TabStripCollectionController::IsGroupCollapsed(
    const tab_groups::TabGroupId& group_id) const {
  return model_->SupportsTabGroups() && model_->IsGroupCollapsed(group_id);
}

std::optional<SkColor> TabStripCollectionController::GetGroupColor(
    const tabs::TabInterface* tab_interface) const {
  std::optional<tab_groups::TabGroupId> group_id = tab_interface->GetGroup();
  if (!group_id.has_value() || !model_->SupportsTabGroups()) {
    return std::nullopt;
  }

  const TabGroupModel* group_model = model_->group_model();
  const TabGroup* group = (group_model->ContainsTabGroup(group_id.value()))
                              ? group_model->GetTabGroup(group_id.value())
                              : nullptr;
  if (!group || !group->visual_data()) {
    return std::nullopt;
  }

  const auto* color_provider = browser_view_->GetColorProvider();
  if (!color_provider) {
    return std::nullopt;
  }

  return color_provider->GetColor(GetTabGroupTabStripColorId(
      group->visual_data()->color(),
      browser_view_->GetWidget()
          ? browser_view_->GetWidget()->ShouldPaintAsActive()
          : true));
}

void TabStripCollectionController::ShiftTabNext(
    const tabs::TabInterface* tab_interface) {
  ShiftTabRelative(tab_interface, 1);
}

void TabStripCollectionController::ShiftTabPrevious(
    const tabs::TabInterface* tab_interface) {
  ShiftTabRelative(tab_interface, -1);
}

void TabStripCollectionController::ShiftGroupUp(
    const tab_groups::TabGroupId& group) {
  ShiftGroupRelative(group, -1);
}

void TabStripCollectionController::ShiftGroupDown(
    const tab_groups::TabGroupId& group) {
  ShiftGroupRelative(group, 1);
}

void TabStripCollectionController::MoveTabFirst(
    const tabs::TabInterface* tab_interface) {
  const std::optional<int> start_index = model_->GetIndexOfTab(tab_interface);
  if (!start_index.has_value()) {
    return;
  }

  std::optional<tab_groups::TabGroupId> focused_group = GetFocusedGroup();
  int target_index = 0;
  if (focused_group.has_value() && tab_interface->GetGroup() == focused_group) {
    const TabGroup* group =
        model_->group_model()->GetTabGroup(focused_group.value());
    if (!group) {
      return;
    }
    target_index = group->ListTabs().start();
  } else if (!model_->IsTabPinned(start_index.value())) {
    while (target_index < start_index && model_->IsTabPinned(target_index)) {
      ++target_index;
    }
  }

  if (!model_->ContainsIndex(target_index)) {
    return;
  }

  if (target_index != start_index.value()) {
    model_->MoveWebContentsAt(start_index.value(), target_index,
                              /*select_after_move=*/false);
  }

  // The tab may unintentionally land in the first group in the tab strip, so we
  // remove the group to ensure consistent behavior. Even if the tab is already
  // at the front, it should "move" out of its current group.
  if (tab_interface->GetGroup().has_value() &&
      tab_interface->GetGroup() != focused_group) {
    model_->RemoveFromGroup({target_index});
  }

  browser_view_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_TAB_AX_ANNOUNCE_MOVED_FIRST));
}

void TabStripCollectionController::MoveTabLast(
    const tabs::TabInterface* tab_interface) {
  const std::optional<int> maybe_start_index =
      model_->GetIndexOfTab(tab_interface);
  if (!maybe_start_index.has_value()) {
    return;
  }

  const int start_index = maybe_start_index.value();

  std::optional<tab_groups::TabGroupId> focused_group = GetFocusedGroup();
  int target_index;
  if (focused_group.has_value() && tab_interface->GetGroup() == focused_group) {
    const TabGroup* group =
        model_->group_model()->GetTabGroup(focused_group.value());
    if (!group) {
      return;
    }
    target_index = group->ListTabs().end() - 1;
  } else if (model_->IsTabPinned(start_index)) {
    int temp_index = start_index + 1;
    while (temp_index < model_->count() && model_->IsTabPinned(temp_index)) {
      ++temp_index;
    }
    target_index = temp_index - 1;
  } else {
    target_index = model_->count() - 1;
  }

  if (!model_->ContainsIndex(target_index)) {
    return;
  }

  if (target_index != start_index) {
    model_->MoveWebContentsAt(start_index, target_index,
                              /*select_after_move=*/false);
  }

  // The tab may unintentionally land in the last group in the tab strip, so we
  // remove the group to ensure consistent behavior. Even if the tab is already
  // at the back, it should "move" out of its current group.
  if (tab_interface->GetGroup().has_value() &&
      tab_interface->GetGroup() != focused_group) {
    model_->RemoveFromGroup({target_index});
  }

  browser_view_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_TAB_AX_ANNOUNCE_MOVED_LAST));
}

void TabStripCollectionController::SelectTab(
    const tabs::TabInterface* tab_interface,
    const TabStripUserGestureDetails& gesture_detail) {
  std::optional<int> tab_index = model_->GetIndexOfTab(tab_interface);
  if (!tab_index.has_value()) {
    return;
  }

  if (!model_->IsTabInForeground(tab_index.value())) {
    RecordMetricsOnTabSelectionChange(tab_interface->GetGroup());
  }

  std::optional<split_tabs::SplitTabId> split_id = tab_interface->GetSplit();
  if (split_id.has_value()) {
    tab_index = split_tabs::GetIndexOfLastActiveTab(model_, split_id.value());
  }

  model_->ActivateTabAt(tab_index.value(), gesture_detail);
}

void TabStripCollectionController::CloseTab(
    const tabs::TabInterface* tab_interface,
    CloseTabSource source) {
  if (tab_closing_helper_ && source != CloseTabSource::kFromNonUIEvent) {
    tab_closing_helper_->MaybeEnterTabClosingMode(std::nullopt, source);
  }

  model_->delegate()->CloseTab(tab_interface, source);
}

void TabStripCollectionController::ToggleSelected(
    const tabs::TabInterface* tab_interface) {
  std::optional<int> tab_index = model_->GetIndexOfTab(tab_interface);
  if (!tab_index.has_value()) {
    return;
  }

  if (model_->IsTabSelected(tab_index.value())) {
    model_->DeselectTabAt(tab_index.value());
  } else {
    model_->SelectTabAt(tab_index.value());
  }
}

void TabStripCollectionController::AddSelectionFromAnchorTo(
    const tabs::TabInterface* tab_interface) {
  std::optional<int> tab_index = model_->GetIndexOfTab(tab_interface);
  if (!tab_index.has_value()) {
    return;
  }

  model_->AddSelectionFromAnchorTo(tab_index.value());
}

void TabStripCollectionController::ExtendSelectionTo(
    const tabs::TabInterface* tab_interface) {
  std::optional<int> tab_index = model_->GetIndexOfTab(tab_interface);
  if (!tab_index.has_value()) {
    return;
  }

  model_->ExtendSelectionTo(tab_index.value());
}

const ui::ListSelectionModel& TabStripCollectionController::GetSelectionModel()
    const {
  return model_->selection_model().GetListSelectionModel();
}

void TabStripCollectionController::ToggleTabGroupCollapsedState(
    const TabGroup* group,
    ToggleTabGroupCollapsedStateOrigin origin) {
  if (model_->closing_all()) {
    return;
  }

  bool is_currently_collapsed = group->visual_data()->is_collapsed();
  bool should_toggle_group = true;

  // We use a WeakPtr because switching the active tab or adding
  // a new tab during a collapse operation can trigger the automatic
  // closure of the group, which synchronously destroys the TabGroup.
  base::WeakPtr<const TabGroup> weak_group = group->AsWeakPtr();

  tabs::TabInterface* active_tab = model_->GetActiveTab();
  if (!is_currently_collapsed && active_tab) {
    if (active_tab->GetGroup() == group->id()) {
      // If the active tab is in the group that is toggling to collapse, the
      // active tab should switch to the next available tab. If there are no
      // available tabs for the active tab to switch to, a new tab will
      // be created.
      const std::optional<int> next_active =
          model_->GetNextExpandedActiveTab(group->id());

      if (next_active.has_value()) {
        model_->ActivateTabAt(
            next_active.value(),
            TabStripUserGestureDetails(
                TabStripUserGestureDetails::GestureType::kOther));
      } else {
        // Create a new tab that will automatically be activated
        should_toggle_group = false;
        // In the "collapse group" scenario, we want the new tab to be created
        // outside of any group to avoid it being collapsed immediately.
        model_->delegate()->AddTabAt(GURL(), -1, true);
      }
    } else {
      // If the active tab is not in the group that is toggling to collapse,
      // reactive the active tab to deselect any other potentially selected
      // tabs.
      SelectTab(active_tab,
                TabStripUserGestureDetails(
                    TabStripUserGestureDetails::GestureType::kOther));
    }
  }

  if (!weak_group) {
    return;
  }

  if (origin != ToggleTabGroupCollapsedStateOrigin::kMenuAction ||
      should_toggle_group) {
    model_->ChangeTabGroupVisuals(
        group->id(),
        tab_groups::TabGroupVisualData(group->visual_data()->title(),
                                       group->visual_data()->color(),
                                       !is_currently_collapsed),
        group->IsCustomized());
  }

  if (should_toggle_group) {
    const TabCollectionNode* group_node =
        root_node_->GetNodeForHandle(group->GetCollectionHandle());
    if (group_node) {
      for (const auto& child_node : group_node->children()) {
        if (auto* tab_view = views::AsViewClass<TabView>(child_node->view())) {
          if (is_currently_collapsed) {
            tab_view->ReleaseFreezingVote(FreezingVoteReason::kCollapsedGroup);
          } else {
            tab_view->CreateFreezingVote(FreezingVoteReason::kCollapsedGroup);
          }
        }
      }
    }
  }

  const bool is_implicit_action =
      origin == ToggleTabGroupCollapsedStateOrigin::kMenuAction ||
      origin == ToggleTabGroupCollapsedStateOrigin::kTabsSelected;
  if (!is_implicit_action) {
    if (is_currently_collapsed) {
      base::RecordAction(
          base::UserMetricsAction("TabGroups_TabGroupHeader_Expanded"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("TabGroups_TabGroupHeader_Collapsed"));
    }
  }
}

void TabStripCollectionController::ShowTabContextMenu(
    TabCollectionNode* collection_node,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  tabs::ConstChildPtr node_data = collection_node->GetNodeData();
  CHECK(std::holds_alternative<tabs::ConstDanglingUntriagedTabInterface>(
      node_data));
  const tabs::TabInterface* tab =
      std::get<tabs::ConstDanglingUntriagedTabInterface>(node_data);

  std::optional<int> tab_index = model_->GetIndexOfTab(tab);
  if (!tab_index.has_value()) {
    return;
  }

  context_menu_controller_ =
      std::make_unique<TabContextMenuController>(tab->GetHandle(), this);

  auto model = std::make_unique<TabMenuModel>(
      context_menu_controller_.get(),
      TabMenuModelDelegate::From(browser_view_->browser()), model_,
      tab_index.value());

  CHECK(browser_view_->tab_strip_view());
  expand_on_hover_lock_ = browser_view_->tab_strip_view()->GetExpandOnHoverLock(
      ExpandOnHoverLockType::kKeepCurrentState);

  // `base::Unretained(this)` is safe because `context_menu_controller_` is
  // owned by `this`, ensuring the callback cannot outlive `this`.
  auto on_menu_closed =
      base::BindRepeating(&TabStripCollectionController::OnTabContextMenuClosed,
                          base::Unretained(this));

  TabMenuModel* model_ptr = model.get();
  context_menu_controller_->LoadModel(std::move(model), model_ptr,
                                      std::move(on_menu_closed));

  context_menu_controller_->RunMenuAt(point, source_type,
                                      collection_node->view()->GetWidget());
}

void TabStripCollectionController::ShowGroupEditorBubble(
    const TabCollectionNode* group_node) {
  auto* group_header_view =
      views::AsViewClass<TabGroupView>(group_node->view())->group_header();
  group_header_view->ShowContextMenuForViewImpl(
      group_header_view, gfx::Point(), ui::mojom::MenuSourceType::kNone);
}

std::unique_ptr<views::Widget>
TabStripCollectionController::ShowGroupEditorBubble(
    const tab_groups::TabGroupId& group_id,
    views::View* anchor_view,
    bool stop_context_menu_propagation) {
  return TabGroupEditorBubbleView::Show(
      browser_view_->browser(), group_id,
      /*anchor_view=*/anchor_view, /*anchor_rect=*/std::nullopt,
      /*stop_context_menu_propagation=*/stop_context_menu_propagation);
}

tab_groups::TabGroupSyncService*
TabStripCollectionController::GetTabGroupSyncService() {
  return tab_groups::TabGroupSyncServiceFactory::GetForProfile(
      browser_view_->GetProfile());
}

tabs::VerticalTabStripStateController*
TabStripCollectionController::GetStateController() {
  return const_cast<tabs::VerticalTabStripStateController*>(
      std::as_const(*this).GetStateController());
}

const tabs::VerticalTabStripStateController*
TabStripCollectionController::GetStateController() const {
  return tabs::VerticalTabStripStateController::From(browser_view_->browser());
}

bool TabStripCollectionController::IsContextMenuCommandChecked(
    TabStripModel::ContextMenuCommand command_id) {
  return false;
}

bool TabStripCollectionController::IsContextMenuCommandEnabled(
    tabs::TabInterface* tab,
    TabStripModel::ContextMenuCommand command_id) {
  return model_->IsContextMenuCommandEnabled(model_->GetIndexOfTab(tab),
                                             command_id);
}

bool TabStripCollectionController::IsContextMenuCommandAlerted(
    TabStripModel::ContextMenuCommand command_id) {
  return false;
}

void TabStripCollectionController::ExecuteContextMenuCommand(
    tabs::TabInterface* tab,
    TabStripModel::ContextMenuCommand command_id,
    int event_flags) {
  model_->ExecuteContextMenuCommand(model_->GetIndexOfTab(tab), command_id);
}

bool TabStripCollectionController::GetContextMenuAccelerator(
    int command_id,
    ui::Accelerator* accelerator) {
#if BUILDFLAG(IS_CHROMEOS)
  auto* browser = browser_view_->browser();
  auto* system_app =
      web_app::AppBrowserController::From(browser)
          ? web_app::AppBrowserController::From(browser)->system_app()
          : nullptr;
  if (system_app && !system_app->ShouldShowTabContextMenuShortcut(
                        browser->GetProfile(),
                        static_cast<tabs::TabContextMenuCommand>(command_id))) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  int browser_cmd;
  return TabStripModel::ContextMenuCommandToBrowserCommand(command_id,
                                                           &browser_cmd) &&
         browser_view_->GetWidget()->GetAccelerator(browser_cmd, accelerator);
}

void TabStripCollectionController::OnTabContextMenuClosed() {
  expand_on_hover_lock_.reset();
}

void TabStripCollectionController::TabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  CHECK(browser_view_);

  if (auto* tab_strip_view = browser_view_->tab_strip_view()) {
    tab_strip_view->OnTabGroupFocusChanged(new_focused_group_id,
                                           old_focused_group_id);
  }
  if (auto* browser_widget = browser_view_->browser_widget()) {
    UpdateFocusModeTheme(new_focused_group_id);
    browser_widget->ThemeChanged();
    if (auto* frame_view = browser_widget->GetFrameView()) {
      frame_view->SchedulePaint();
    }
  }

  UpdateAllTabsFocusFreezing();
}

void TabStripCollectionController::UpdateAllTabsFocusFreezing() {
  if (!features::IsTabGroupsFocusFreezingEnabled()) {
    return;
  }
  if (!model_ || !browser_view_ || !browser_view_->tab_strip_view()) {
    return;
  }
  for (tabs::TabInterface* tab : *model_) {
    views::View* const view =
        browser_view_->tab_strip_view()->GetTabAnchorView(tab->GetHandle());
    if (auto* tab_view = views::AsViewClass<TabView>(view)) {
      tab_view->UpdateFocusFreezing();
    }
  }
}

void TabStripCollectionController::UpdateFocusModeTheme(
    std::optional<tab_groups::TabGroupId> group_id) {
  std::optional<SkColor> color;
  if (group_id.has_value() && model_ && model_->group_model() &&
      model_->group_model()->ContainsTabGroup(group_id.value())) {
    const TabGroup* group =
        model_->group_model()->GetTabGroup(group_id.value());
    if (group && group->visual_data()) {
      const auto* color_provider =
          browser_view_ ? browser_view_->GetColorProvider() : nullptr;
      if (color_provider) {
        color = color_provider->GetColor(
            GetTabGroupDialogColorId(group->visual_data()->color()));
      }
    }
  }

  if (browser_view_ && browser_view_->browser_widget()) {
    browser_view_->browser_widget()->SetUserColorOverride(color);
  }
}

#if BUILDFLAG(IS_CHROMEOS)
bool TabStripCollectionController::IsLockedForOnTask() const {
  return ash::boca::OnTaskLockedController::From(browser_view_->browser())
      ->is_locked_for_on_task();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void TabStripCollectionController::TabKeyboardFocusChangedTo(
    const tabs::TabInterface* tab) {
  std::optional<int> tab_index;
  if (tab) {
    tab_index = model_->GetIndexOfTab(tab);
  }

  chrome::BrowserCommandController::From(browser_view_->browser())
      ->TabKeyboardFocusChangedTo(tab_index);
}

void TabStripCollectionController::RecordMetricsOnTabSelectionChange(
    std::optional<tab_groups::TabGroupId> group) {
  base::UmaHistogramEnumeration("TabStrip.Tab.Views.ActivationAction",
                                TabActivationTypes::kTab);

  if (!group) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("TabGroups_SwitchGroupedTab"));

  if (!tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups()) {
    return;
  }

  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser_view_->browser()->GetProfile());

  if (!tab_group_service) {
    return;
  }

  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_service->GetGroup(group.value());
  if (saved_group && saved_group->collaboration_id()) {
    base::RecordAction(
        base::UserMetricsAction("TabGroups.Shared.SwitchGroupedTab"));
  }
}

void TabStripCollectionController::ShiftTabRelative(
    const tabs::TabInterface* tab_interface,
    int offset) {
  CHECK_EQ(1, std::abs(offset))
      << "Offset must be 1 or -1 to shift tab up or down.";
  const std::optional<int> maybe_start_index =
      model_->GetIndexOfTab(tab_interface);
  if (!maybe_start_index.has_value()) {
    return;
  }

  const int start_index = maybe_start_index.value();
  int target_index = start_index + offset;

  const auto old_group = tab_interface->GetGroup();
  std::optional<tab_groups::TabGroupId> focused_group = GetFocusedGroup();
  if (!model_->ContainsIndex(target_index) ||
      model_->IsTabPinned(start_index) != model_->IsTabPinned(target_index)) {
    // Even if we've reached the boundary of where the tab could go, it may
    // still be able to "move" out of its current group.
    if (old_group.has_value() && old_group != focused_group) {
      AnnounceTabRemovedFromGroup(old_group.value());
      model_->RemoveFromGroup({start_index});
    }
    return;
  }

  // If the tab is at a group boundary and the group is expanded, instead of
  // actually moving the tab just change its group membership.
  std::optional<tab_groups::TabGroupId> target_group =
      model_->GetTabGroupForTab(target_index);
  if (old_group != target_group) {
    // Do not allow tabs to enter or exit the focused tab group.
    if (focused_group.has_value() &&
        (old_group == focused_group || target_group == focused_group)) {
      return;
    }

    if (old_group.has_value()) {
      AnnounceTabRemovedFromGroup(old_group.value());
      model_->RemoveFromGroup({start_index});
      return;
    } else if (target_group.has_value()) {
      // If the tab is at a group boundary and the group is collapsed, treat the
      // collapsed group as a tab and find the next available slot for the tab
      // to move to.
      const TabGroup* group =
          model_->group_model()->GetTabGroup(target_group.value());
      if (group && group->visual_data()->is_collapsed()) {
        int candidate_index = target_index + offset;
        while (model_->ContainsIndex(candidate_index) &&
               model_->GetTabGroupForTab(candidate_index) == target_group) {
          candidate_index += offset;
        }
        if (model_->ContainsIndex(candidate_index)) {
          target_index = candidate_index - offset;
        } else {
          target_index = offset < 0 ? 0 : model_->count() - 1;
        }
      } else {
        tabs::TabInterface* tab = model_->GetTabAtIndex(start_index);
        views::View* tab_view =
            tab ? root_node_->GetNodeForHandle(tab->GetHandle())->view()
                : nullptr;
        // Read before adding the tab to the group so that the group description
        // isn't the tab we just added.
        AnnounceTabAddedToGroup(target_group.value());
        model_->AddToExistingGroup({start_index}, target_group.value());
        if (tab_view) {
          views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
              kTabGroupedCustomEventId, tab_view);
        }
        return;
      }
    }
  }

  model_->MoveWebContentsAt(start_index, target_index, false);
  browser_view_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16((offset > 0) ? IDS_TAB_AX_ANNOUNCE_MOVED_DOWN
                                             : IDS_TAB_AX_ANNOUNCE_MOVED_UP));
}

void TabStripCollectionController::ShiftGroupRelative(
    const tab_groups::TabGroupId& group,
    int offset) {
  if (GetFocusedGroup() == group) {
    return;
  }

  CHECK_EQ(1, std::abs(offset))
      << "Offset must be 1 or -1 to shift the group up or down.";

  const TabGroup* tab_group = model_->group_model()->GetTabGroup(group);
  if (!tab_group) {
    return;
  }
  gfx::Range tabs_in_group = tab_group->ListTabs();

  const int start_index = tabs_in_group.start();
  const int index_of_skipped_over_tab =
      offset == 1 ? tabs_in_group.end() : start_index - 1;

  if (!model_->ContainsIndex(start_index) ||
      !model_->ContainsIndex(index_of_skipped_over_tab)) {
    return;
  }

  if (model_->IsTabPinned(index_of_skipped_over_tab)) {
    return;
  }

  // Avoid moving into the middle of another group by accounting for its size.
  std::optional<tab_groups::TabGroupId> target_group =
      model_->GetTabGroupForTab(index_of_skipped_over_tab);
  if (target_group.has_value()) {
    CHECK_NE(target_group.value(), group)
        << "The target group must be different from the current group to move.";
  }

  const int num_skipped_tabs = target_group.has_value()
                                   ? model_->group_model()
                                         ->GetTabGroup(target_group.value())
                                         ->ListTabs()
                                         .length()
                                   : 1;

  const int target_index = start_index + offset * num_skipped_tabs;
  model_->MoveGroupTo(group, target_index);
}

void TabStripCollectionController::AnnounceTabAddedToGroup(
    tab_groups::TabGroupId group_id) {
  auto* group = model_->group_model()->GetTabGroup(group_id);
  const std::u16string group_title = group->visual_data()->title();
  const std::u16string contents_string =
      tab_groups::GetGroupContentString(group);
  browser_view_->GetViewAccessibility().AnnounceText(
      group_title.empty()
          ? l10n_util::GetStringFUTF16(
                IDS_TAB_AX_ANNOUNCE_TAB_ADDED_TO_UNNAMED_GROUP, contents_string)
          : l10n_util::GetStringFUTF16(
                IDS_TAB_AX_ANNOUNCE_TAB_ADDED_TO_NAMED_GROUP, group_title,
                contents_string));
}

void TabStripCollectionController::AnnounceTabRemovedFromGroup(
    tab_groups::TabGroupId group_id) {
  auto* group = model_->group_model()->GetTabGroup(group_id);
  const std::u16string group_title = group->visual_data()->title();
  const std::u16string contents_string =
      tab_groups::GetGroupContentString(group);
  browser_view_->GetViewAccessibility().AnnounceText(
      group_title.empty()
          ? l10n_util::GetStringFUTF16(
                IDS_TAB_AX_ANNOUNCE_TAB_REMOVED_FROM_UNNAMED_GROUP,
                contents_string)
          : l10n_util::GetStringFUTF16(
                IDS_TAB_AX_ANNOUNCE_TAB_REMOVED_FROM_NAMED_GROUP, group_title,
                contents_string));
}

BrowserFrameView* TabStripCollectionController::GetBrowserFrameView() const {
  return browser_view_->browser_widget()
             ? browser_view_->browser_widget()->GetFrameView()
             : nullptr;
}

void TabStripCollectionController::OnGlassFrameEligibilityChanged(
    bool is_eligible) {
  is_glass_frame_ = is_eligible;
  browser_view_->tab_strip_view()->OnGlassFrameEligibilityChanged(is_eligible);
}

int TabStripCollectionController::GetStrokeThickness() const {
  return browser_view_ && browser_view_->ShouldDrawTabStrokes() ? 1 : 0;
}
