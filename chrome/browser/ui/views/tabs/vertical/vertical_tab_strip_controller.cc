// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"

#include <variant>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_menu_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_context_menu_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_interface.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

VerticalTabStripController::VerticalTabStripController(
    TabStripModel* model,
    BrowserView* browser_view,
    VerticalTabDragHandler& drag_handler,
    std::unique_ptr<TabMenuModelFactory> menu_model_factory_override)
    : model_(model), browser_view_(browser_view), drag_handler_(drag_handler) {
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
