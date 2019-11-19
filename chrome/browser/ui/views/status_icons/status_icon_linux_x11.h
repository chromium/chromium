// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_X11_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_X11_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/linux_ui/status_icon_linux.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_observer_x11.h"
#include "ui/views/widget/widget.h"

namespace aura {
class WindowTreeHost;
}

// A status icon that uses the XEmbed protocol.
// https://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html
class StatusIconLinuxX11 : public views::StatusIconLinux,
                           public views::Button,
                           public views::ContextMenuController,
                           public views::ButtonListener,
                           public views::DesktopWindowTreeHostObserverX11 {
 public:
  StatusIconLinuxX11();
  ~StatusIconLinuxX11() override;

  // views::StatusIcon:
  void SetIcon(const gfx::ImageSkia& image) override;
  void SetToolTip(const base::string16& tool_tip) override;
  void UpdatePlatformContextMenu(ui::MenuModel* model) override;
  void OnSetDelegate() override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // views::ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // views::DesktopWindowTreeHostObserverX11:
  void OnWindowMapped(unsigned long xid) override;
  void OnWindowUnmapped(unsigned long xid) override;

 private:
  std::unique_ptr<views::Widget> widget_;

  aura::WindowTreeHost* host_ = nullptr;

  std::unique_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconLinuxX11);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_X11_H_
