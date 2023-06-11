// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TABBED_CONTROLLER_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TABBED_CONTROLLER_H_

#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"

#import "components/remote_cocoa/app_shim/bridged_content_view.h"

@class TabTitlebarViewController;

namespace remote_cocoa {

class REMOTE_COCOA_APP_SHIM_EXPORT ImmersiveModeTabbedController
    : public ImmersiveModeController {
 public:
  explicit ImmersiveModeTabbedController(NSWindow* browser_window,
                                         NSWindow* overlay_window,
                                         NSWindow* tab_window);
  ImmersiveModeTabbedController(const ImmersiveModeTabbedController&) = delete;
  ImmersiveModeTabbedController& operator=(
      const ImmersiveModeTabbedController&) = delete;
  ~ImmersiveModeTabbedController() override;

  // ImmersiveModeController overrides
  // TODO(https://crbug.com/1426944): Enable() does not add the controller. It
  // will be added / removed from the view controller tree during
  // UpdateToolbarVisibility(). Remove this comment once the bug has been
  // resolved.
  void Enable() override;
  void UpdateToolbarVisibility(mojom::ToolbarVisibilityStyle style) override;
  void OnTopViewBoundsChanged(const gfx::Rect& bounds) override;
  void RevealLock() override;
  void RevealUnlock() override;
  void OnTitlebarFrameDidChange(NSRect frame) override;
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

  // TODO(https://crbug.com/1280317): Merge the contents back into the header
  // file once all files that include this header are compiled with ARC.
  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TABBED_CONTROLLER_H_
