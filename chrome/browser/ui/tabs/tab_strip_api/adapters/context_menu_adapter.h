// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_CONTEXT_MENU_ADAPTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_CONTEXT_MENU_ADAPTER_H_

#include "base/types/expected.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

class ContextMenuAdapter {
 public:
  virtual ~ContextMenuAdapter() = default;

  virtual base::expected<void, mojo_base::mojom::ErrorPtr> ShowTabContextMenu(
      tabs::TabHandle handle,
      const gfx::Point& location) = 0;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_CONTEXT_MENU_ADAPTER_H_
