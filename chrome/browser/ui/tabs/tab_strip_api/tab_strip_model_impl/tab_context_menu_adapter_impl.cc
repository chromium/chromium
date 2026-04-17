// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_context_menu_adapter_impl.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace tabs_api {

TabContextMenuAdapterImpl::TabContextMenuAdapterImpl(
    BrowserWindowInterface* browser)
    : browser_(browser) {}

TabContextMenuAdapterImpl::~TabContextMenuAdapterImpl() = default;

base::expected<void, mojo_base::mojom::ErrorPtr>
TabContextMenuAdapterImpl::ShowTabContextMenu(tabs::TabHandle handle,
                                              const gfx::Point& location) {
  return base::unexpected(mojo_base::mojom::Error::New(
      mojo_base::mojom::Code::kUnimplemented, "Not implemented"));
}

bool TabContextMenuAdapterImpl::IsContextMenuCommandChecked(
    TabStripModel::ContextMenuCommand command_id) {
  NOTIMPLEMENTED();
  return false;
}

bool TabContextMenuAdapterImpl::IsContextMenuCommandEnabled(
    int index,
    TabStripModel::ContextMenuCommand command_id) {
  NOTIMPLEMENTED();
  return false;
}

bool TabContextMenuAdapterImpl::IsContextMenuCommandAlerted(
    TabStripModel::ContextMenuCommand command_id) {
  NOTIMPLEMENTED();
  return false;
}

void TabContextMenuAdapterImpl::ExecuteContextMenuCommand(
    int index,
    TabStripModel::ContextMenuCommand command_id,
    int event_flags) {
  NOTIMPLEMENTED();
}

bool TabContextMenuAdapterImpl::GetContextMenuAccelerator(
    int command_id,
    ui::Accelerator* accelerator) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace tabs_api
