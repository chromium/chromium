// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/controllers/tab_strip_ui_controller_impl.h"

#include <optional>
#include <utility>

#include "base/types/expected.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/context_menu_adapter.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

TabStripUIControllerImpl::TabStripUIControllerImpl(
    std::unique_ptr<TabStripUIControllerInjector> injector)
    : injector_(std::move(injector)) {}

TabStripUIControllerImpl::~TabStripUIControllerImpl() = default;

void TabStripUIControllerImpl::Bind(
    mojo::PendingReceiver<mojom::TabStripUIController> receiver) {
  receivers_.Add(&bridge_, std::move(receiver));
}

mojom::TabStripUIController::ShowTabContextMenuResult
TabStripUIControllerImpl::ShowTabContextMenu(const tabs_api::NodeId& id,
                                             const gfx::Point& location) {
  std::optional<tabs::TabHandle> tab_handle = id.ToTabHandle();
  if (!tab_handle.has_value()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "invalid tab id"));
  }

  tabs_api::ContextMenuAdapter& adapter = injector_->context_menu_adapter();
  base::expected<void, mojo_base::mojom::ErrorPtr> result =
      adapter.ShowTabContextMenu(tab_handle.value(), location);
  if (!result.has_value()) {
    return base::unexpected(std::move(result.error()));
  }

  return std::monostate();
}

}  // namespace tabs_api
