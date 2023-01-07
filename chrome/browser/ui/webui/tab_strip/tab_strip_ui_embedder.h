// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_EMBEDDER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_EMBEDDER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/menu_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/point.h"

// Interface to be implemented by the embedder. Provides native UI
// functionality such as showing context menus.
class TabStripUIEmbedder {
 public:
  TabStripUIEmbedder() = default;
  virtual ~TabStripUIEmbedder() = default;

  virtual const ui::AcceleratorProvider* GetAcceleratorProvider() const = 0;

  virtual void CloseContainer() = 0;

  virtual void ShowContextMenuAtPoint(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model,
      base::RepeatingClosure on_menu_closed_callback) = 0;

  virtual void CloseContextMenu() = 0;

  virtual void ShowEditDialogForGroupAtPoint(gfx::Point point,
                                             gfx::Rect rect,
                                             tab_groups::TabGroupId group) = 0;
  virtual void HideEditDialogForGroup() = 0;

  virtual TabStripUILayout GetLayout() = 0;

  virtual SkColor GetColorProviderColor(ui::ColorId id) const = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_EMBEDDER_H_
