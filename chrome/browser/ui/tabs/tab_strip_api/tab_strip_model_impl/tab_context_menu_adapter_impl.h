// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_TAB_CONTEXT_MENU_ADAPTER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_TAB_CONTEXT_MENU_ADAPTER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/context_menu_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"

class BrowserWindowInterface;

namespace tabs_api {

// Handles the lifecycle and delegation for a tab context menu.
class TabContextMenuAdapterImpl : public ContextMenuAdapter,
                                  public TabContextMenuController::Delegate {
 public:
  explicit TabContextMenuAdapterImpl(BrowserWindowInterface* browser,
                                     TabStripModel* tab_strip_model);
  TabContextMenuAdapterImpl(const TabContextMenuAdapterImpl&) = delete;
  TabContextMenuAdapterImpl operator=(const TabContextMenuAdapterImpl&) =
      delete;
  ~TabContextMenuAdapterImpl() override;

  // ContextMenuAdapter:
  base::expected<void, mojo_base::mojom::ErrorPtr> ShowTabContextMenu(
      tabs::TabHandle handle,
      const gfx::Point& location) override;

  // TabContextMenuController::Delegate:
  bool IsContextMenuCommandChecked(
      TabStripModel::ContextMenuCommand command_id) override;
  bool IsContextMenuCommandEnabled(
      tabs::TabInterface* tab,
      TabStripModel::ContextMenuCommand command_id) override;
  bool IsContextMenuCommandAlerted(
      TabStripModel::ContextMenuCommand command_id) override;
  void ExecuteContextMenuCommand(tabs::TabInterface* tab,
                                 TabStripModel::ContextMenuCommand command_id,
                                 int event_flags) override;
  bool GetContextMenuAccelerator(int command_id,
                                 ui::Accelerator* accelerator) override;

 private:
  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<TabContextMenuController> context_menu_controller_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_MODEL_IMPL_TAB_CONTEXT_MENU_ADAPTER_IMPL_H_
