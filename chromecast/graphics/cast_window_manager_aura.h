// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_WINDOW_MANAGER_AURA_H_
#define CHROMECAST_GRAPHICS_CAST_WINDOW_MANAGER_AURA_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window_tree_host_platform.h"

namespace aura {
namespace client {
class ScreenPositionClient;
}  // namespace client
}  // namespace aura

namespace chromecast {

class CastTouchEventGate;
class CastFocusClientAura;
class CastGestureHandler;
class CastSystemGestureEventHandler;
class CastSystemGestureDispatcher;
class SideSwipeDetector;
class CastWindowTreeHostAura;
class RoundedWindowCorners;

class CastWindowManagerAura : public CastWindowManager,
                              public aura::client::WindowParentingClient {
 public:
  explicit CastWindowManagerAura(bool enable_input);
  ~CastWindowManagerAura() override;

  void Setup();
  void OnWindowOrderChanged(std::vector<WindowId> window_order);

  // CastWindowManager implementation:
  void TearDown() override;
  void AddWindow(gfx::NativeView window) override;
  gfx::NativeView GetRootWindow() override;
  std::vector<WindowId> GetWindowOrder() override;
  void SetZOrder(gfx::NativeView window, mojom::ZOrder z_order) override;
  void InjectEvent(ui::Event* event) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void AddGestureHandler(CastGestureHandler* handler) override;
  void RemoveGestureHandler(CastGestureHandler* handler) override;
  void SetTouchInputDisabled(bool disabled) override;
  void AddTouchActivityObserver(CastTouchActivityObserver* observer) override;
  void RemoveTouchActivityObserver(
      CastTouchActivityObserver* observer) override;
  void SetEnableRoundedCorners(bool enable) override;
  void NotifyColorInversionEnabled(bool enabled) override;

  // aura::client::WindowParentingClient implementation:
  aura::Window* GetDefaultParent(aura::Window* window,
                                 const gfx::Rect& bounds) override;

  CastWindowTreeHostAura* window_tree_host() const;
  CastGestureHandler* GetGestureHandler() const;
  aura::client::CaptureClient* capture_client() const {
    return capture_client_.get();
  }

 private:
  const bool enable_input_;
  std::unique_ptr<CastWindowTreeHostAura> window_tree_host_;
  std::unique_ptr<aura::client::DefaultCaptureClient> capture_client_;
  std::unique_ptr<CastFocusClientAura> focus_client_;
  std::unique_ptr<aura::client::ScreenPositionClient> screen_position_client_;
  std::unique_ptr<CastTouchEventGate> event_gate_;
  std::unique_ptr<CastSystemGestureDispatcher> system_gesture_dispatcher_;
  std::unique_ptr<CastSystemGestureEventHandler> system_gesture_event_handler_;
  std::unique_ptr<SideSwipeDetector> side_swipe_detector_;
  std::unique_ptr<RoundedWindowCorners> rounded_window_corners_;

  std::vector<WindowId> window_order_;
  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(CastWindowManagerAura);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_WINDOW_MANAGER_AURA_H_
