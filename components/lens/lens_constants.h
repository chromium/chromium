// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_CONSTANTS_H_
#define COMPONENTS_LENS_LENS_CONSTANTS_H_

namespace lens {

// The max area for the image to be sent to Lens.
inline constexpr int kMaxAreaForImageSearch = 1'000'000;

// The max pixel width/height for the image to be sent to Lens.
inline constexpr int kMaxPixelsForImageSearch = 1000;

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_CONSTANTS_H_
