// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/display/use_layered_window.h"

#include "ui/base/win/internal_constants.h"

namespace viz {

bool NeedsToUseLayerWindow(HWND hwnd) {
  // TODO(kylechar): Revisit if we can not use layered windows on Windows 8 and
  // higher. With DWM enabled HWNDs seem to support an alpha channel natively.
  // However, when touch highlight or pointer trails are enabled Windows ends up
  // blending the highlight/trail with old content for non-layered HWNDs. See
  // https://crbug.com/843974 for more details.
  return GetProp(hwnd, ui::kWindowTranslucent);
}

}  // namespace viz
