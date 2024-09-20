// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_BROWSER_FRAME_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_BROWSER_FRAME_AURA_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
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

  DesktopBrowserFrameAura(const DesktopBrowserFrameAura&) = delete;
  DesktopBrowserFrameAura& operator=(const DesktopBrowserFrameAura&) = delete;

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
  void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  bool ShouldRestorePreviousBrowserWidgetState() const override;
  bool ShouldUseInitialVisibleOnAllWorkspaces() const override;

 private:
  // The BrowserView is our ClientView. This is a pointer to it.
  raw_ptr<BrowserView> browser_view_;
  raw_ptr<BrowserFrame> browser_frame_;

  // Owned by the RootWindow.
  raw_ptr<BrowserDesktopWindowTreeHost, AcrossTasksDanglingUntriaged>
      browser_desktop_window_tree_host_;

  std::unique_ptr<wm::VisibilityController> visibility_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_BROWSER_FRAME_AURA_H_
