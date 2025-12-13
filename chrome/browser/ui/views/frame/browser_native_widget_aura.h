// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NATIVE_WIDGET_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NATIVE_WIDGET_AURA_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_native_widget.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

class BrowserDesktopWindowTreeHost;
class BrowserWidget;
class BrowserView;

namespace wm {
class VisibilityController;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserNativeWidgetAura
//
//  BrowserNativeWidgetAura is a DesktopNativeWidgetAura subclass that provides
//  the window frame for the Chrome browser window.
//
class BrowserNativeWidgetAura : public views::DesktopNativeWidgetAura,
                                public BrowserNativeWidget {
 public:
  BrowserNativeWidgetAura(BrowserWidget* browser_widget,
                          BrowserView* browser_view);

  BrowserNativeWidgetAura(const BrowserNativeWidgetAura&) = delete;
  BrowserNativeWidgetAura& operator=(const BrowserNativeWidgetAura&) = delete;

  BrowserView* browser_view() const { return browser_view_; }
  BrowserWidget* browser_widget() const { return browser_widget_; }

 protected:
  ~BrowserNativeWidgetAura() override;

  // Overridden from views::DesktopNativeWidgetAura:
  void OnHostClosed() override;
  void InitNativeWidget(views::Widget::InitParams params) override;
  void OnOcclusionStateChanged(aura::WindowTreeHost* host,
                               aura::Window::OcclusionState new_state,
                               const SkRegion& occluded_region) override;

  // Overridden from BrowserNativeWidget:
  views::Widget::InitParams GetWidgetParams(
      views::Widget::InitParams::Ownership ownership) override;
  bool UseCustomFrame() const override;
  bool UsesNativeSystemMenu() const override;
  bool ShouldSaveWindowPlacement() const override;
  void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  bool ShouldRestorePreviousBrowserWidgetState() const override;
  bool ShouldUseInitialVisibleOnAllWorkspaces() const override;
  void ClientDestroyedWidget() override;

 private:
  // The BrowserView is our ClientView. This is a pointer to it.
  raw_ptr<BrowserView> browser_view_;
  raw_ptr<BrowserWidget> browser_widget_;

  // Owned by the RootWindow.
  raw_ptr<BrowserDesktopWindowTreeHost, AcrossTasksDanglingUntriaged>
      browser_desktop_window_tree_host_;

  std::unique_ptr<wm::VisibilityController> visibility_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NATIVE_WIDGET_AURA_H_
