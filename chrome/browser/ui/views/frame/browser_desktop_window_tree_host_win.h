// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_WIN_H_

#include <windows.h>
#include <uxtheme.h>

#include "base/macros.h"
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host.h"
#include "chrome/browser/ui/views/frame/minimize_button_metrics_win.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

class BrowserFrame;
class BrowserView;
class BrowserWindowPropertyManager;

namespace views {
class DesktopNativeWidgetAura;
class NativeMenuWin;
}

class BrowserDesktopWindowTreeHostWin : public BrowserDesktopWindowTreeHost,
                                        public views::DesktopWindowTreeHostWin {
 public:
  BrowserDesktopWindowTreeHostWin(
      views::internal::NativeWidgetDelegate* native_widget_delegate,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura,
      BrowserView* browser_view,
      BrowserFrame* browser_frame);
  ~BrowserDesktopWindowTreeHostWin() override;

 private:
  views::NativeMenuWin* GetSystemMenu();

  // Overridden from BrowserDesktopWindowTreeHost:
  DesktopWindowTreeHost* AsDesktopWindowTreeHost() override;
  int GetMinimizeButtonOffset() const override;
  bool UsesNativeSystemMenu() const override;

  // Overridden from DesktopWindowTreeHostWin:
  int GetInitialShowState() const override;
  bool GetClientAreaInsets(gfx::Insets* insets,
                           HMONITOR monitor) const override;
  void HandleCreate() override;
  void HandleDestroying() override;
  void HandleFrameChanged() override;
  void HandleWindowScaleFactorChanged(float window_scale_factor) override;
  bool PreHandleMSG(UINT message,
                    WPARAM w_param,
                    LPARAM l_param,
                    LRESULT* result) override;
  void PostHandleMSG(UINT message, WPARAM w_param, LPARAM l_param) override;
  views::FrameMode GetFrameMode() const override;
  bool ShouldUseNativeFrame() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void FrameTypeChanged() override;

  void UpdateDWMFrame();
  MARGINS GetDWMFrameMargins() const;

  bool IsOpaqueHostedAppFrame() const;

  BrowserView* browser_view_;
  BrowserFrame* browser_frame_;

  MinimizeButtonMetrics minimize_button_metrics_;

  std::unique_ptr<BrowserWindowPropertyManager>
      browser_window_property_manager_;

  // The wrapped system menu itself.
  std::unique_ptr<views::NativeMenuWin> system_menu_;

  // Necessary to avoid corruption on NC paint in Aero mode.
  bool did_gdi_clear_;

  DISALLOW_COPY_AND_ASSIGN(BrowserDesktopWindowTreeHostWin);
};


#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_WIN_H_
