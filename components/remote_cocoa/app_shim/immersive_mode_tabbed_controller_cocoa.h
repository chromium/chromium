// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TABBED_CONTROLLER_COCOA_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TABBED_CONTROLLER_COCOA_H_

#include "components/remote_cocoa/app_shim/immersive_mode_controller_cocoa.h"

#import "components/remote_cocoa/app_shim/bridged_content_view.h"

@class TabTitlebarViewController;

namespace remote_cocoa {

class REMOTE_COCOA_APP_SHIM_EXPORT ImmersiveModeTabbedControllerCocoa
    : public ImmersiveModeControllerCocoa {
 public:
  explicit ImmersiveModeTabbedControllerCocoa(
      NativeWidgetMacNSWindow* browser_window,
      NativeWidgetMacOverlayNSWindow* overlay_window,
      NativeWidgetMacOverlayNSWindow* tab_window);
  ImmersiveModeTabbedControllerCocoa(
      const ImmersiveModeTabbedControllerCocoa&) = delete;
  ImmersiveModeTabbedControllerCocoa& operator=(
      const ImmersiveModeTabbedControllerCocoa&) = delete;
  ~ImmersiveModeTabbedControllerCocoa() override;

  // ImmersiveModeController overrides
  // TODO(crbug.com/40261565): Init() does not add the controller. It
  // will be added / removed from the view controller tree during
  // UpdateToolbarVisibility(). Remove this comment once the bug has been
  // resolved.
  void Init() override;
  void UpdateToolbarVisibility(
      std::optional<mojom::ToolbarVisibilityStyle> style) override;
  void OnTopViewBoundsChanged(const gfx::Rect& bounds) override;
  void RevealLocked() override;
  void RevealUnlocked() override;
  void Reanchor() override;
  void OnChildWindowAdded(NSWindow* child) override;
  void OnChildWindowRemoved(NSWindow* child) override;
  bool ShouldObserveChildWindow(NSWindow* child) override;
  bool IsTabbed() override;

 private:
  void TitlebarReveal();
  void TitlebarHide();
  void AddController();
  void RemoveController();

  // Ensure tab window is z-order on top of any siblings. Tab window will be
  // parented to overlay window regardless of the current parent.
  void OrderTabWindowZOrderOnTop();

  NativeWidgetMacOverlayNSWindow* __weak tab_window_;
  BridgedContentView* __weak tab_content_view_;
  NSTitlebarAccessoryViewController* __strong tab_titlebar_view_controller_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TABBED_CONTROLLER_COCOA_H_
