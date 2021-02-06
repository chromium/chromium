// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_BROWSER_FRAME_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_BROWSER_FRAME_AURA_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

class BrowserDesktopWindowTreeHost;
class BrowserFrame;
class BrowserView;

namespace wm {
class VisibilityController;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopBrowserFrameAura
//
//  DesktopBrowserFrameAura is a DesktopNativeWidgetAura subclass that provides
//  the window frame for the Chrome browser window.
//
class DesktopBrowserFrameAura : public views::DesktopNativeWidgetAura,
                                public NativeBrowserFrame {
 public:
  DesktopBrowserFrameAura(BrowserFrame* browser_frame,
                          BrowserView* browser_view);

  BrowserView* browser_view() const { return browser_view_; }
  BrowserFrame* browser_frame() const { return browser_frame_; }

 protected:
  ~DesktopBrowserFrameAura() override;

  // Overridden from views::DesktopNativeWidgetAura:
  void OnHostClosed() override;
  void InitNativeWidget(views::Widget::InitParams params) override;

  // Overridden from NativeBrowserFrame:
  views::Widget::InitParams GetWidgetParams() override;
  bool UseCustomFrame() const override;
  bool UsesNativeSystemMenu() const override;
  int GetMinimizeButtonOffset() const override;
  bool ShouldSaveWindowPlacement() const override;
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;

 private:
  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;
  BrowserFrame* browser_frame_;

  // Owned by the RootWindow.
  BrowserDesktopWindowTreeHost* browser_desktop_window_tree_host_;

  std::unique_ptr<wm::VisibilityController> visibility_controller_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBrowserFrameAura);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_BROWSER_FRAME_AURA_H_
