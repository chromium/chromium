// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_H_

#include <AppKit/AppKit.h>

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"

@class ImmersiveModeMapper;
@class ImmersiveModeTitlebarViewController;

namespace gfx {
class Rect;
}

namespace remote_cocoa {

// TODO(mek): This should not be exported and used outside of remote_cocoa. So
// figure out how to restructure code so callers outside of remote_cocoa can
// stop existing.
REMOTE_COCOA_APP_SHIM_EXPORT bool IsNSToolbarFullScreenWindow(NSWindow* window);

class REMOTE_COCOA_APP_SHIM_EXPORT ImmersiveModeController {
 public:
  class Delegate {
   public:
    virtual void TopViewWillAppear() = 0;
    virtual void TopViewDidAppear(NSView* content_view) = 0;
  };

  explicit ImmersiveModeController(NSWindow* browser_widget,
                                   NSWindow* overlay_widget,
                                   base::OnceCallback<void()> callback);
  ~ImmersiveModeController();

  void Enable();
  void OnTopViewBoundsChanged(const gfx::Rect& bounds);
  void UpdateToolbarVisibility(bool always_show);

 private:
  bool enabled_ = false;

  NSWindow* const browser_widget_;
  NSWindow* const overlay_widget_;

  base::scoped_nsobject<ImmersiveModeTitlebarViewController>
      immersive_mode_titlebar_view_controller_;
  base::scoped_nsobject<ImmersiveModeMapper> immersive_mode_mapper_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_H_
