// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_H_

namespace ash {

enum class ArcGraphicsTracingMode {
  // Full tracing mode.
  kFull,
  // Overview tracing mode with ability to compare different runs.
  kOverview,
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_H_
