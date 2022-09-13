// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ICON_ICON_SIZE_H_
#define COMPONENTS_PAYMENTS_CONTENT_ICON_ICON_SIZE_H_

#include "ui/gfx/native_widget_types.h"

namespace payments {

// Calculates the ideal height in pixels for payment icons depending on the
// screen resolution (32 * device_scale_factor).
class IconSizeCalculator final {
 public:
  IconSizeCalculator() = delete;
  IconSizeCalculator(const IconSizeCalculator&) = delete;
  IconSizeCalculator& operator=(const IconSizeCalculator&) = delete;

  static int IdealIconHeight(gfx::NativeView view);
  static int MinimumIconHeight();
  static constexpr int kPaymentAppDeviceIndependentIdealIconHeight = 32;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ICON_ICON_SIZE_H_
