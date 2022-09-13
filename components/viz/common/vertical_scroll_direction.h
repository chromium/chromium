// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_VERTICAL_SCROLL_DIRECTION_H_
#define COMPONENTS_VIZ_COMMON_VERTICAL_SCROLL_DIRECTION_H_

namespace viz {

// Used to indicate the vertical scroll direction of the root layer. Note that
// |kNull| is only used to represent the absence of a vertical scroll direction.
// See services/viz/public/mojom/vertical_scroll_position.mojom.
enum class VerticalScrollDirection { kNull, kDown, kUp };

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_VERTICAL_SCROLL_DIRECTION_H_
