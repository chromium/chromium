// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/payments/content/icon/icon_size.h"

#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace payments {

namespace {

float DeviceScaleFactor(gfx::NativeView view) {
  display::Screen* screen = display::Screen::GetScreen();
  DCHECK(screen);
  return screen->GetDisplayNearestView(view).device_scale_factor();
}

}  // namespace

// static
int IconSizeCalculator::IdealIconHeight(gfx::NativeView view) {
  return DeviceScaleFactor(view) * kPaymentAppDeviceIndependentIdealIconHeight;
}

// static
int IconSizeCalculator::MinimumIconHeight() {
  return 0;
}

}  // namespace payments
