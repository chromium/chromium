// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TABBED_CONTROLLER_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TABBED_CONTROLLER_H_

#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"

#include "base/mac/scoped_nsobject.h"

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
  void Enable() override;
  void UpdateToolbarVisibility(mojom::ToolbarVisibilityStyle style) override;
  void RevealLock() override;
  void RevealUnlock() override;
  void TitlebarLock() override;
  void TitlebarUnlock() override;

 private:
  void TitlebarReveal();
  void TitlebarHide();

  NSWindow* const tab_window_;
  base::scoped_nsobject<TabTitlebarViewController>
      tab_titlebar_view_controller_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TABBED_CONTROLLER_H_
