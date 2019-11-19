// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_ROUNDED_WINDOW_CORNERS_H_
#define CHROMECAST_GRAPHICS_ROUNDED_WINDOW_CORNERS_H_

#include <memory>

namespace chromecast {

class CastWindowManager;

// A class that draws rounded borders on the corners of the screen.
class RoundedWindowCorners {
 public:
  static std::unique_ptr<RoundedWindowCorners> Create(
      CastWindowManager* window_manager);

  RoundedWindowCorners();
  virtual ~RoundedWindowCorners();

  virtual void SetEnabled(bool enable) = 0;

  virtual void SetColorInversion(bool enable) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_ROUNDED_WINDOW_CORNERS_H_
