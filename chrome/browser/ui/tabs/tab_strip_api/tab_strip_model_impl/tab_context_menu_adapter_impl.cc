// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_context_menu_adapter_impl.h"

#include "base/notimplemented.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/widget/widget.h"

namespace tabs_api {

TabContextMenuAdapterImpl::TabContextMenuAdapterImpl(
    BrowserWindowInterface* browser,
    TabStripModel* tab_strip_model)
    : browser_(browser), tab_strip_model_(tab_strip_model) {}

TabContextMenuAdapterImpl::~TabContextMenuAdapterImpl() = default;

base::expected<void, mojo_base::mojom::ErrorPtr>
TabContextMenuAdapterImpl::ShowTabContextMenu(tabs::TabHandle handle,
                                              const gfx::Point& location) {
  if (!tab_strip_model_) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kNotFound, "TabStripModel not found"));
  }

  int tab_index = tab_strip_model_->GetIndexOfTab(handle.Get());
  if (tab_index == TabStripModel::kNoTab) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kNotFound, "Tab not found"));
  }

  context_menu_controller_ =
      std::make_unique<TabContextMenuController>(handle, this);

  auto menu_model = std::make_unique<TabMenuModel>(
      context_menu_controller_.get(),
      browser_->GetFeatures().tab_menu_model_delegate(), tab_strip_model_,
      tab_index);

  context_menu_controller_->LoadModel(std::move(menu_model));

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_->GetWindow()->GetNativeWindow());
  context_menu_controller_->RunMenuAt(location,
                                      ui::mojom::MenuSourceType::kNone, widget);
  return base::ok();
}

bool TabContextMenuAdapterImpl::IsContextMenuCommandChecked(
    TabStripModel::ContextMenuCommand command_id) {
  return false;
}

bool TabContextMenuAdapterImpl::IsContextMenuCommandEnabled(
    tabs::TabInterface* tab,
    TabStripModel::ContextMenuCommand command_id) {
  return tab_strip_model_->IsContextMenuCommandEnabled(
      tab_strip_model_->GetIndexOfTab(tab), command_id);
}

bool TabContextMenuAdapterImpl::IsContextMenuCommandAlerted(
    TabStripModel::ContextMenuCommand command_id) {
  return false;
}

void TabContextMenuAdapterImpl::ExecuteContextMenuCommand(
    tabs::TabInterface* tab,
    TabStripModel::ContextMenuCommand command_id,
    int event_flags) {
  tab_strip_model_->ExecuteContextMenuCommand(
      tab_strip_model_->GetIndexOfTab(tab), command_id);
}

bool TabContextMenuAdapterImpl::GetContextMenuAccelerator(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

}  // namespace tabs_api
