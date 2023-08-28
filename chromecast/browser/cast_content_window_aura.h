// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_AURA_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_AURA_H_

#include "chromecast/browser/cast_content_gesture_handler.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_contents_observer.h"
#include "chromecast/browser/mojom/cast_web_service.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace chromecast {

class CastWindowManager;
class TouchBlocker;

class CastContentWindowAura : public CastContentWindow,
                              public CastWebContentsObserver,
                              public content::WebContentsObserver,
                              public aura::WindowObserver {
 public:
  CastContentWindowAura(mojom::CastWebViewParamsPtr params,
                        CastWindowManager* window_manager);

  CastContentWindowAura(const CastContentWindowAura&) = delete;
  CastContentWindowAura& operator=(const CastContentWindowAura&) = delete;

  ~CastContentWindowAura() override;

  // CastContentWindow implementation:
  void CreateWindow(mojom::ZOrder z_order,
                    VisibilityPriority visibility_priority) override;
  void GrantScreenAccess() override;
  void RevokeScreenAccess() override;
  void RequestVisibility(VisibilityPriority visibility_priority) override;
  void EnableTouchInput(bool enabled) override;

  // content::WebContentsObserver implementation:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // aura::WindowObserver implementation:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroyed(aura::Window* window) override;

 private:
  void SetFullWindowBounds();
  void SetHiddenWindowBounds();

  CastWindowManager* const window_manager_;

  // Utility class for detecting and dispatching gestures to delegates.
  std::unique_ptr<CastContentGestureHandler> gesture_dispatcher_;
  std::unique_ptr<TouchBlocker> touch_blocker_;

  aura::Window* window_;
  bool has_screen_access_;
  bool resize_window_when_navigation_starts_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_AURA_H_
