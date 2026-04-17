// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_CONTEXT_MENU_ADAPTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_CONTEXT_MENU_ADAPTER_H_

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/context_menu_adapter.h"

namespace tabs_api::testing {

class ToyTabContextMenuAdapter : public ContextMenuAdapter {
 public:
  ToyTabContextMenuAdapter();
  ToyTabContextMenuAdapter(const ToyTabContextMenuAdapter&) = delete;
  ToyTabContextMenuAdapter operator=(const ToyTabContextMenuAdapter&) = delete;
  ~ToyTabContextMenuAdapter() override;

  // ContextMenuAdapter:
  base::expected<void, mojo_base::mojom::ErrorPtr> ShowTabContextMenu(
      tabs::TabHandle handle,
      const gfx::Point& location) override;
};

}  // namespace tabs_api::testing

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_CONTEXT_MENU_ADAPTER_H_
