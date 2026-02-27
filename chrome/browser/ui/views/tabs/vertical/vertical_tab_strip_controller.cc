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
#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"

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
      std::make_unique<TabContextMenuController>(tab_index.value(), this);

  auto model = menu_model_factory_->Create(
      context_menu_controller_.get(),
      browser_view_->browser()->GetFeatures().tab_menu_model_delegate(), model_,
      tab_index.value());

  context_menu_controller_->LoadModel(std::move(model));

  context_menu_controller_->RunMenuAt(point, source_type, source->GetWidget());
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

void VerticalTabStripController::ToggleTabGroupCollapsedState(
    const TabGroup* group,
    ToggleTabGroupCollapsedStateOrigin origin) {
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
      static_cast<VerticalTabGroupView*>(group_node->view())->group_header();
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

bool VerticalTabStripController::IsCollapsed() const {
  const tabs::VerticalTabStripStateController* state_controller =
      tabs::VerticalTabStripStateController::From(browser_view_->browser());
  return state_controller && state_controller->IsCollapsed();
}

tab_groups::TabGroupSyncService*
VerticalTabStripController::GetTabGroupSyncService() {
  return tab_groups::TabGroupSyncServiceFactory::GetForProfile(
      browser_view_->GetProfile());
}

tabs::VerticalTabStripStateController*
VerticalTabStripController::GetStateController() {
  return tabs::VerticalTabStripStateController::From(browser_view_->browser());
}

bool VerticalTabStripController::IsContextMenuCommandChecked(
    TabStripModel::ContextMenuCommand command_id) {
  return false;
}

bool VerticalTabStripController::IsContextMenuCommandEnabled(
    int index,
    TabStripModel::ContextMenuCommand command_id) {
  return model_->IsContextMenuCommandEnabled(index, command_id);
}

bool VerticalTabStripController::IsContextMenuCommandAlerted(
    TabStripModel::ContextMenuCommand command_id) {
  return false;
}

void VerticalTabStripController::ExecuteContextMenuCommand(
    int index,
    TabStripModel::ContextMenuCommand command_id,
    int event_flags) {
  model_->ExecuteContextMenuCommand(index, command_id);
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
