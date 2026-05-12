// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"

#include <variant>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_menu_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_accessibility.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

VerticalTabStripController::VerticalTabStripController(
    TabStripModel* model,
    BrowserView* browser_view,
    VerticalTabDragHandler& drag_handler,
    TabHoverCardController* hover_card_controller,
    std::unique_ptr<TabMenuModelFactory> menu_model_factory_override)
    : model_(model),
      browser_view_(browser_view),
      drag_handler_(drag_handler),
      hover_card_controller_(hover_card_controller) {
  if (menu_model_factory_override) {
    menu_model_factory_ = std::move(menu_model_factory_override);
  } else {
    menu_model_factory_ = std::make_unique<TabMenuModelFactory>();
  }
}

VerticalTabStripController::~VerticalTabStripController() {
  if (context_menu_controller_.get()) {
    context_menu_controller_.reset();
  }
}

void VerticalTabStripController::ShowContextMenuForNode(
    TabCollectionNode* collection_node,
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  tabs::ConstChildPtr node_data = collection_node->GetNodeData();
  CHECK(std::holds_alternative<const tabs::TabInterface*>(node_data));
  const tabs::TabInterface* tab =
      std::get<const tabs::TabInterface*>(node_data);
  std::optional<int> tab_index =
      tab->GetBrowserWindowInterface()->GetTabStripModel()->GetIndexOfTab(tab);

  if (!tab_index.has_value()) {
    return;
  }

  context_menu_controller_ =
      std::make_unique<TabContextMenuController>(tab->GetHandle(), this);

  auto model = menu_model_factory_->Create(
      context_menu_controller_.get(),
      browser_view_->browser()->GetFeatures().tab_menu_model_delegate(), model_,
      tab_index.value());

  CHECK(browser_view_->tab_strip_view());
  expand_on_hover_lock_ = browser_view_->tab_strip_view()->GetExpandOnHoverLock(
      ExpandOnHoverLockType::kKeepCurrentState);

  // `base::Unretained(this)` is safe because `context_menu_controller_` is
  // owned by `this`, ensuring the callback cannot outlive `this`.
  auto on_menu_closed =
      base::BindRepeating(&VerticalTabStripController::OnTabContextMenuClosed,
                          base::Unretained(this));

  context_menu_controller_->LoadModel(std::move(model),
                                      std::move(on_menu_closed));

  context_menu_controller_->RunMenuAt(point, source_type, source->GetWidget());
}

void VerticalTabStripController::ShiftTabNext(
    const tabs::TabInterface* tab_interface) {
  ShiftTabRelative(tab_interface, 1);
}

void VerticalTabStripController::ShiftTabPrevious(
    const tabs::TabInterface* tab_interface) {
  ShiftTabRelative(tab_interface, -1);
}

void VerticalTabStripController::ShiftGroupUp(
    const tab_groups::TabGroupId& group) {
  ShiftGroupRelative(group, -1);
}

void VerticalTabStripController::ShiftGroupDown(
    const tab_groups::TabGroupId& group) {
  ShiftGroupRelative(group, 1);
}

void VerticalTabStripController::MoveTabFirst(
    const tabs::TabInterface* tab_interface) {
  const std::optional<int> start_index = model_->GetIndexOfTab(tab_interface);
  if (!start_index.has_value()) {
    return;
  }

  int target_index = 0;
  if (!model_->IsTabPinned(start_index.value())) {
    while (target_index < start_index && model_->IsTabPinned(target_index)) {
      ++target_index;
    }
  }

  if (!model_->ContainsIndex(target_index)) {
    return;
  }

  if (target_index != start_index) {
    model_->MoveWebContentsAt(start_index.value(), target_index,
                              /*select_after_move=*/false);
  }

  // The tab may unintentionally land in the first group in the tab strip, so we
  // remove the group to ensure consistent behavior. Even if the tab is already
  // at the front, it should "move" out of its current group.
  if (tab_interface->GetGroup().has_value()) {
    model_->RemoveFromGroup({target_index});
  }

  browser_view_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_TAB_AX_ANNOUNCE_MOVED_FIRST));
}

void VerticalTabStripController::MoveTabLast(
    const tabs::TabInterface* tab_interface) {
  const std::optional<int> maybe_start_index =
      model_->GetIndexOfTab(tab_interface);
  if (!maybe_start_index.has_value()) {
    return;
  }

  const int start_index = maybe_start_index.value();

  int target_index;
  if (model_->IsTabPinned(start_index)) {
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
  if (tab_interface->GetGroup().has_value()) {
    model_->RemoveFromGroup({target_index});
  }

  browser_view_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_TAB_AX_ANNOUNCE_MOVED_LAST));
}

void VerticalTabStripController::SelectTab(
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

void VerticalTabStripController::CloseTab(
    const tabs::TabInterface* tab_interface) {
  std::optional<int> tab_index = model_->GetIndexOfTab(tab_interface);
  if (!tab_index.has_value()) {
    return;
  }

  model_->CloseWebContentsAt(tab_index.value(),
                             TabCloseTypes::CLOSE_USER_GESTURE |
                                 TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

void VerticalTabStripController::ToggleSelected(
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

void VerticalTabStripController::AddSelectionFromAnchorTo(
    const tabs::TabInterface* tab_interface) {
  std::optional<int> tab_index = model_->GetIndexOfTab(tab_interface);
  if (!tab_index.has_value()) {
    return;
  }

  model_->AddSelectionFromAnchorTo(tab_index.value());
}

void VerticalTabStripController::ExtendSelectionTo(
    const tabs::TabInterface* tab_interface) {
  std::optional<int> tab_index = model_->GetIndexOfTab(tab_interface);
  if (!tab_index.has_value()) {
    return;
  }

  model_->ExtendSelectionTo(tab_index.value());
}

const ui::ListSelectionModel& VerticalTabStripController::GetSelectionModel()
    const {
  return model_->selection_model().GetListSelectionModel();
}

void VerticalTabStripController::ToggleTabGroupCollapsedState(
    const TabGroup* group,
    ToggleTabGroupCollapsedStateOrigin origin) {
  if (model_->closing_all()) {
    return;
  }

  bool is_currently_collapsed = group->visual_data()->is_collapsed();
  bool should_toggle_group = true;

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
        // We intentionally do not call CreateNewTab() here because it
        // respects the IsNewTabAddsToActiveGroupEnabled() feature, which would
        // add the new tab to the same group as the currently active tab.
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

  if (origin != ToggleTabGroupCollapsedStateOrigin::kMenuAction ||
      should_toggle_group) {
    model_->ChangeTabGroupVisuals(
        group->id(),
        tab_groups::TabGroupVisualData(group->visual_data()->title(),
                                       group->visual_data()->color(),
                                       !is_currently_collapsed),
        true);
  }

  if (should_toggle_group &&
      base::FeatureList::IsEnabled(features::kTabGroupsCollapseFreezing)) {
    gfx::Range tabs_in_group = group->ListTabs();
    for (uint32_t i = tabs_in_group.start(); i < tabs_in_group.end(); ++i) {
      views::View* const view =
          browser_view_->tab_strip_view()->GetTabAnchorViewAt(i);
      CHECK(views::IsViewClass<VerticalTabView>(view));
      VerticalTabView* const tab_view =
          views::AsViewClass<VerticalTabView>(view);
      if (is_currently_collapsed) {
        tab_view->ReleaseFreezingVote();
      } else {
        tab_view->CreateFreezingVote();
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

void VerticalTabStripController::ShowGroupEditorBubble(
    const TabCollectionNode* group_node) {
  auto* group_header_view =
      views::AsViewClass<VerticalTabGroupView>(group_node->view())
          ->group_header();
  group_header_view->ShowContextMenuForViewImpl(
      group_header_view, gfx::Point(), ui::mojom::MenuSourceType::kNone);
}

views::Widget* VerticalTabStripController::ShowGroupEditorBubble(
    const tab_groups::TabGroupId& group_id,
    views::View* anchor_view,
    bool stop_context_menu_propagation) {
  return TabGroupEditorBubbleView::Show(
      browser_view_->browser(), group_id,
      /*anchor_view=*/anchor_view, /*anchor_rect=*/std::nullopt,
      /*stop_context_menu_propagation=*/stop_context_menu_propagation);
}

tab_groups::TabGroupSyncService*
VerticalTabStripController::GetTabGroupSyncService() {
  return tab_groups::TabGroupSyncServiceFactory::GetForProfile(
      browser_view_->GetProfile());
}

tabs::VerticalTabStripStateController*
VerticalTabStripController::GetStateController() {
  return const_cast<tabs::VerticalTabStripStateController*>(
      std::as_const(*this).GetStateController());
}

const tabs::VerticalTabStripStateController*
VerticalTabStripController::GetStateController() const {
  return tabs::VerticalTabStripStateController::From(browser_view_->browser());
}

const tabs::TabInterface* VerticalTabStripController::GetActiveTab() const {
  return model_->GetActiveTab();
}

bool VerticalTabStripController::IsContextMenuCommandChecked(
    TabStripModel::ContextMenuCommand command_id) {
  return false;
}

bool VerticalTabStripController::IsContextMenuCommandEnabled(
    tabs::TabInterface* tab,
    TabStripModel::ContextMenuCommand command_id) {
  return model_->IsContextMenuCommandEnabled(model_->GetIndexOfTab(tab),
                                             command_id);
}

bool VerticalTabStripController::IsContextMenuCommandAlerted(
    TabStripModel::ContextMenuCommand command_id) {
  return false;
}

void VerticalTabStripController::ExecuteContextMenuCommand(
    tabs::TabInterface* tab,
    TabStripModel::ContextMenuCommand command_id,
    int event_flags) {
  model_->ExecuteContextMenuCommand(model_->GetIndexOfTab(tab), command_id);
}

bool VerticalTabStripController::GetContextMenuAccelerator(
    int command_id,
    ui::Accelerator* accelerator) {
#if BUILDFLAG(IS_CHROMEOS)
  auto* browser = browser_view_->browser();
  auto* system_app = browser->app_controller()
                         ? browser->app_controller()->system_app()
                         : nullptr;
  if (system_app && !system_app->ShouldShowTabContextMenuShortcut(
                        browser->profile(), command_id)) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  int browser_cmd;
  return TabStripModel::ContextMenuCommandToBrowserCommand(command_id,
                                                           &browser_cmd) &&
         browser_view_->GetWidget()->GetAccelerator(browser_cmd, accelerator);
}

void VerticalTabStripController::OnTabContextMenuClosed() {
  expand_on_hover_lock_.reset();
}

void VerticalTabStripController::TabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  browser_view_->tab_strip_view()->OnTabGroupFocusChanged(new_focused_group_id,
                                                          old_focused_group_id);

  std::optional<SkColor> color;
  if (new_focused_group_id.has_value()) {
    const TabGroup* group =
        model_->group_model()->GetTabGroup(new_focused_group_id.value());
    const tab_groups::TabGroupVisualData* visual_data = group->visual_data();
    const auto* color_provider = browser_view_->GetColorProvider();
    color = color_provider->GetColor(
        GetTabGroupDialogColorId(visual_data->color()));
  }

  browser_view_->browser_widget()->SetUserColorOverride(color);
  browser_view_->browser_widget()->ThemeChanged();
  browser_view_->GetWidget()->non_client_view()->frame_view()->SchedulePaint();
}

void VerticalTabStripController::TabKeyboardFocusChangedTo(
    const tabs::TabInterface* tab) {
  std::optional<int> tab_index = std::nullopt;
  if (tab) {
    tab_index = model_->GetIndexOfTab(tab);
  }

  browser_view_->browser()->command_controller()->TabKeyboardFocusChangedTo(
      tab_index);
}

void VerticalTabStripController::RecordMetricsOnTabSelectionChange(
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

void VerticalTabStripController::ShiftTabRelative(
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
  if (!model_->ContainsIndex(target_index) ||
      model_->IsTabPinned(start_index) != model_->IsTabPinned(target_index)) {
    // Even if we've reached the boundary of where the tab could go, it may
    // still be able to "move" out of its current group.
    if (old_group.has_value()) {
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
        views::View* tab_view =
            browser_view_->tab_strip_view()->GetTabAnchorViewAt(start_index);
        // Read before adding the tab to the group so that the group description
        // isn't the tab we just added.
        AnnounceTabAddedToGroup(target_group.value());
        model_->AddToExistingGroup({start_index}, target_group.value());
        views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
            kTabGroupedCustomEventId, tab_view);
        return;
      }
    }
  }

  model_->MoveWebContentsAt(start_index, target_index, false);
  browser_view_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16((offset > 0) ? IDS_TAB_AX_ANNOUNCE_MOVED_DOWN
                                             : IDS_TAB_AX_ANNOUNCE_MOVED_UP));
}

void VerticalTabStripController::ShiftGroupRelative(
    const tab_groups::TabGroupId& group,
    int offset) {
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

void VerticalTabStripController::AnnounceTabAddedToGroup(
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

void VerticalTabStripController::AnnounceTabRemovedFromGroup(
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
