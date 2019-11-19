// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_AURA_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_AURA_H_

#include "base/macros.h"
#include "chromecast/browser/cast_content_gesture_handler.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/ui/media_control_ui.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace chromecast {

class TouchBlocker;

class CastContentWindowAura : public CastContentWindow,
                              public CastWebContents::Observer,
                              public aura::WindowObserver {
 public:
  CastContentWindowAura(const CastContentWindow::CreateParams& params,
                        CastWindowManager* window_manager);
  ~CastContentWindowAura() override;

  // CastContentWindow implementation:
  void CreateWindowForWebContents(
      CastWebContents* cast_web_contents,
      mojom::ZOrder z_order,
      VisibilityPriority visibility_priority) override;
  void GrantScreenAccess() override;
  void RevokeScreenAccess() override;
  void RequestVisibility(VisibilityPriority visibility_priority) override;
  void SetActivityContext(base::Value activity_context) override;
  void SetHostContext(base::Value host_context) override;
  void NotifyVisibilityChange(VisibilityType visibility_type) override;
  void RequestMoveOut() override;
  void EnableTouchInput(bool enabled) override;
  mojom::MediaControlUi* media_controls() override;

  // CastWebContents::Observer implementation:
  void MainFrameResized(const gfx::Rect& bounds) override;

  // aura::WindowObserver implementation:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroyed(aura::Window* window) override;

 private:
  CastWindowManager* const window_manager_;

  // Utility class for detecting and dispatching gestures to delegates.
  std::unique_ptr<CastContentGestureHandler> gesture_dispatcher_;
  CastContentGestureHandler::Priority const gesture_priority_;

  const bool is_touch_enabled_;
  std::unique_ptr<TouchBlocker> touch_blocker_;

  std::unique_ptr<MediaControlUi> media_controls_;

  aura::Window* window_;
  bool has_screen_access_;

  DISALLOW_COPY_AND_ASSIGN(CastContentWindowAura);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_AURA_H_
