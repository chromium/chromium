// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_manager_default.h"

#include "base/memory/ptr_util.h"
#include "chromecast/graphics/cast_touch_activity_observer.h"

namespace chromecast {

CastWindowManagerDefault::CastWindowManagerDefault() {}

CastWindowManagerDefault::~CastWindowManagerDefault() {}

void CastWindowManagerDefault::TearDown() {}
void CastWindowManagerDefault::AddWindow(gfx::NativeView window) {}

gfx::NativeView CastWindowManagerDefault::GetRootWindow() {
  return nullptr;
}
std::vector<CastWindowManager::WindowId>
CastWindowManagerDefault::GetWindowOrder() {
  return std::vector<CastWindowManager::WindowId>();
}

void CastWindowManagerDefault::InjectEvent(ui::Event* event) {}
void CastWindowManagerDefault::SetZOrder(gfx::NativeView window,
                                         mojom::ZOrder z_order) {}
void CastWindowManagerDefault::AddObserver(Observer* observer) {}
void CastWindowManagerDefault::RemoveObserver(Observer* observer) {}

// Register a new handler for system gesture events.
void CastWindowManagerDefault::AddGestureHandler(CastGestureHandler* handler) {}
// Remove the registration of a system gesture events handler.
void CastWindowManagerDefault::RemoveGestureHandler(
    CastGestureHandler* handler) {}

void CastWindowManagerDefault::SetTouchInputDisabled(bool disabled) {}

void CastWindowManagerDefault::AddTouchActivityObserver(
    CastTouchActivityObserver* observer) {}
void CastWindowManagerDefault::RemoveTouchActivityObserver(
    CastTouchActivityObserver* observer) {}

void CastWindowManagerDefault::SetEnableRoundedCorners(bool enable) {}
void CastWindowManagerDefault::NotifyColorInversionEnabled(bool enabled) {}

}  // namespace chromecast
