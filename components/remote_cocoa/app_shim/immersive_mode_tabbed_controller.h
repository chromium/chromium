// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TABBED_CONTROLLER_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TABBED_CONTROLLER_H_

#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"

#include "base/mac/scoped_nsobject.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"

@class TabTitlebarViewController;

namespace remote_cocoa {

class REMOTE_COCOA_APP_SHIM_EXPORT ImmersiveModeTabbedController
    : public ImmersiveModeController {
 public:
  explicit ImmersiveModeTabbedController(NSWindow* browser_window,
                                         NSWindow* overlay_window,
                                         NSWindow* tab_window,
                                         base::OnceClosure callback);
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
  void FullscreenTransitionCompleted() override;
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

  NSWindow* const tab_window_;
  BridgedContentView* tab_content_view_;
  base::scoped_nsobject<NSTitlebarAccessoryViewController>
      tab_titlebar_view_controller_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TABBED_CONTROLLER_H_
