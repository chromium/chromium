// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_
#define COMPONENTS_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_

#include "components/capture_mode/capture_mode_export.h"

namespace ui {
class ContextFactory;
}  // namespace ui

namespace capture_mode {

// Returns true if GPU rasterization is supported by the given
// `context_factory`.
CAPTURE_MODE_EXPORT bool IsGpuRasterizationSupported(
    ui::ContextFactory* context_factory);

}  // namespace capture_mode

#endif  // COMPONENTS_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_
