// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/accelerators/accelerator.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/accelerators.h"
#endif

namespace chrome {

bool IsChromeAccelerator(const ui::Accelerator& accelerator) {
#if defined(OS_CHROMEOS)
  for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
    const ash::AcceleratorData& accel_data = ash::kAcceleratorData[i];
    if (accel_data.keycode == accelerator.key_code() &&
        accel_data.modifiers == accelerator.modifiers()) {
      return true;
    }
  }
#endif

  const std::vector<AcceleratorMapping> accelerators = GetAcceleratorList();
  for (const auto& entry : accelerators) {
    if (entry.keycode == accelerator.key_code() &&
        entry.modifiers == accelerator.modifiers())
      return true;
  }

  return false;
}

ui::AcceleratorProvider* AcceleratorProviderForBrowser(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser);
}

}  // namespace chrome
