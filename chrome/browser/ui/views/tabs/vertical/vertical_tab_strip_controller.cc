// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"

#include <variant>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_menu_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_context_menu_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_interface.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

VerticalTabStripController::VerticalTabStripController(
    TabStripModel* model,
    BrowserView* browser_view,
    std::unique_ptr<TabMenuModelFactory> menu_model_factory_override)
    : model_(model), browser_view_(browser_view) {
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

  context_menu_controller_ = std::make_unique<TabContextMenuController>(
      base::BindRepeating(
          &VerticalTabStripController::IsContextMenuCommandChecked,
          base::Unretained(this)),
      base::BindRepeating(
          &VerticalTabStripController::IsContextMenuCommandEnabled,
          base::Unretained(this), tab_index.value()),
      base::BindRepeating(
          &VerticalTabStripController::IsContextMenuCommandAlerted,
          base::Unretained(this)),
      base::BindRepeating(
          &VerticalTabStripController::ExecuteContextMenuCommand,
          base::Unretained(this), tab_index.value()),
      base::BindRepeating(
          &VerticalTabStripController::GetContextMenuAccelerator,
          base::Unretained(this)));

  auto model = menu_model_factory_->Create(
      context_menu_controller_.get(),
      browser_view_->browser()->GetFeatures().tab_menu_model_delegate(), model_,
      tab_index.value());

  context_menu_controller_->LoadModel(std::move(model));

  context_menu_controller_->RunMenuAt(point, source_type, source->GetWidget());
}

std::optional<int> VerticalTabStripController::GetIndexFromMojomTab(
    const tabs_api::mojom::Tab& mojom_tab) {
  const auto tab_handle = mojom_tab.id.ToTabHandle();

  const int index = model_->GetIndexOfTab(tab_handle->Get());

  return (index != TabStripModel::kNoTab) ? std::optional<int>(index)
                                          : std::nullopt;
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
