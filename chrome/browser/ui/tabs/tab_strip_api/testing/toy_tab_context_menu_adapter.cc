// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_context_menu_adapter.h"

#include "mojo/public/mojom/base/error.mojom.h"

namespace tabs_api::testing {

ToyTabContextMenuAdapter::ToyTabContextMenuAdapter() = default;

ToyTabContextMenuAdapter::~ToyTabContextMenuAdapter() = default;

base::expected<void, mojo_base::mojom::ErrorPtr>
ToyTabContextMenuAdapter::ShowTabContextMenu(tabs::TabHandle handle,
                                             const gfx::Point& location) {
  return base::unexpected(mojo_base::mojom::Error::New(
      mojo_base::mojom::Code::kUnimplemented, "Not implemented"));
}

}  // namespace tabs_api::testing
