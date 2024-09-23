// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_WIN_H_

#include <shobjidl.h>

#include <wrl/client.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host.h"
#include "chrome/browser/ui/views/frame/minimize_button_metrics_win.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

class BrowserFrame;
class BrowserView;
class BrowserWindowPropertyManager;
class VirtualDesktopHelper;

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
  BrowserDesktopWindowTreeHostWin(const BrowserDesktopWindowTreeHostWin&) =
      delete;
  BrowserDesktopWindowTreeHostWin& operator=(
      const BrowserDesktopWindowTreeHostWin&) = delete;
  ~BrowserDesktopWindowTreeHostWin() override;

 private:
  views::NativeMenuWin* GetSystemMenu();

  // Overridden from BrowserDesktopWindowTreeHost:
  DesktopWindowTreeHost* AsDesktopWindowTreeHost() override;
  int GetMinimizeButtonOffset() const override;
  bool UsesNativeSystemMenu() const override;

  // Overridden from DesktopWindowTreeHostWin:
  void Init(const views::Widget::InitParams& params) override;
  void Show(ui::mojom::WindowShowState show_state,
            const gfx::Rect& restore_bounds) override;
  std::string GetWorkspace() const override;
  int GetInitialShowState() const override;
  bool GetClientAreaInsets(gfx::Insets* insets,
                           HMONITOR monitor) const override;
  bool GetDwmFrameInsetsInPixels(gfx::Insets* insets) const override;
  void HandleCreate() override;
  void HandleDestroying() override;
  void HandleWindowScaleFactorChanged(float window_scale_factor) override;
  bool PreHandleMSG(UINT message,
                    WPARAM w_param,
                    LPARAM l_param,
                    LRESULT* result) override;
  void PostHandleMSG(UINT message, WPARAM w_param, LPARAM l_param) override;
  views::FrameMode GetFrameMode() const override;
  bool ShouldUseNativeFrame() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void HandleWindowMinimizedOrRestored(bool restored) override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override;

  // Kicks off an asynchronous update of |workspace_|, and notifies
  // WindowTreeHost of its value.
  void UpdateWorkspace();

  void SetWindowIcon(bool badged);

  raw_ptr<BrowserView> browser_view_;
  raw_ptr<BrowserFrame> browser_frame_;

  MinimizeButtonMetrics minimize_button_metrics_;

  std::unique_ptr<BrowserWindowPropertyManager>
      browser_window_property_manager_;

  // The wrapped system menu itself.
  std::unique_ptr<views::NativeMenuWin> system_menu_;

  // This is used to monitor when the window icon needs to be updated because
  // the icon badge has changed (e.g., avatar icon changed).
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};

  base::win::ScopedHICON icon_handle_;

  // This will be null pre Win10.
  scoped_refptr<VirtualDesktopHelper> virtual_desktop_helper_;

  base::WeakPtrFactory<BrowserDesktopWindowTreeHostWin> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_WINDOW_TREE_HOST_WIN_H_
