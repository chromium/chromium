// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_WINDOW_MANAGER_DEFAULT_H_
#define CHROMECAST_GRAPHICS_CAST_WINDOW_MANAGER_DEFAULT_H_

#include <memory>

#include "chromecast/graphics/cast_window_manager.h"

namespace chromecast {

class CastWindowManagerDefault : public CastWindowManager {
 public:
  CastWindowManagerDefault();

  CastWindowManagerDefault(const CastWindowManagerDefault&) = delete;
  CastWindowManagerDefault& operator=(const CastWindowManagerDefault&) = delete;

  ~CastWindowManagerDefault() override;

  // CastWindowManager implementation:
  void TearDown() override;
  void AddWindow(gfx::NativeView window) override;
  void SetZOrder(gfx::NativeView window, mojom::ZOrder z_order) override;
  gfx::NativeView GetRootWindow() override;
  std::vector<WindowId> GetWindowOrder() override;
  void InjectEvent(ui::Event* event) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void AddGestureHandler(CastGestureHandler* handler) override;

  void RemoveGestureHandler(CastGestureHandler* handler) override;

  void SetTouchInputDisabled(bool disabled) override;
  void AddTouchActivityObserver(CastTouchActivityObserver* observer) override;
  void RemoveTouchActivityObserver(
      CastTouchActivityObserver* observer) override;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_WINDOW_MANAGER_DEFAULT_H_
