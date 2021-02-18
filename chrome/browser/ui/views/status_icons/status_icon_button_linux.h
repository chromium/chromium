// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_BUTTON_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_BUTTON_LINUX_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/linux_ui/status_icon_linux.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget.h"

namespace aura {
class WindowTreeHost;
}

// A button that is internally mapped as a status icon if the underlaying
// platform supports that kind of windows. Otherwise, calls
// OnImplInitializationFailed.
class StatusIconButtonLinux : public views::StatusIconLinux,
                              public views::Button,
                              public views::ContextMenuController {
 public:
  METADATA_HEADER(StatusIconButtonLinux);

  StatusIconButtonLinux();
  StatusIconButtonLinux(const StatusIconButtonLinux&) = delete;
  StatusIconButtonLinux& operator=(const StatusIconButtonLinux&) = delete;
  ~StatusIconButtonLinux() override;

  // views::StatusIcon:
  void SetIcon(const gfx::ImageSkia& image) override;
  void SetToolTip(const base::string16& tool_tip) override;
  void UpdatePlatformContextMenu(ui::MenuModel* model) override;
  void OnSetDelegate() override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  std::unique_ptr<views::Widget> widget_;

  aura::WindowTreeHost* host_ = nullptr;

  std::unique_ptr<views::MenuRunner> menu_runner_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_BUTTON_LINUX_H_
