// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_WIN_H_

#include <shobjidl.h>
#include <wrl/client.h>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
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

class BrowserDesktopWindowTreeHostWin
    : public BrowserDesktopWindowTreeHost,
      public views::DesktopWindowTreeHostWin,
      public ProfileAttributesStorage::Observer {
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
  void Init(const views::Widget::InitParams& params) override;
  std::string GetWorkspace() const override;
  int GetInitialShowState() const override;
  bool GetClientAreaInsets(gfx::Insets* insets,
                           HMONITOR monitor) const override;
  bool GetDwmFrameInsetsInPixels(gfx::Insets* insets) const override;
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

  // ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;

  bool IsOpaqueHostedAppFrame() const;

  void SetWindowIcon(bool badged);

  BrowserView* browser_view_;
  BrowserFrame* browser_frame_;

  MinimizeButtonMetrics minimize_button_metrics_;

  std::unique_ptr<BrowserWindowPropertyManager>
      browser_window_property_manager_;

  // The wrapped system menu itself.
  std::unique_ptr<views::NativeMenuWin> system_menu_;

  // On Windows10, this is the virtual desktop the browser window was on,
  // last we checked. This is used to tell if the window has moved to a
  // different desktop, and notify listeners. It will only be set if
  // we created |virtual_desktop_manager_|.
  mutable base::Optional<std::string> workspace_;

  // Only set on Windows10. Set by GetOrCreateVirtualDesktopManager().
  mutable Microsoft::WRL::ComPtr<IVirtualDesktopManager>
      virtual_desktop_manager_;

  // This is used to monitor when the window icon needs to be updated because
  // the icon badge has changed (e.g., avatar icon changed).
  ScopedObserver<ProfileAttributesStorage, ProfileAttributesStorage::Observer>
      profile_observer_{this};

  base::win::ScopedHICON icon_handle_;

  DISALLOW_COPY_AND_ASSIGN(BrowserDesktopWindowTreeHostWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_WIN_H_
