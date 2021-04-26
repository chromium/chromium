// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_ACCESSIBILITY_MAGNIFICATION_CONTROLLER_H_
#define CHROMECAST_GRAPHICS_ACCESSIBILITY_MAGNIFICATION_CONTROLLER_H_

namespace chromecast {

// Interface for implementations of the screen magnification feature.
class MagnificationController {
 public:
  MagnificationController() = default;
  virtual ~MagnificationController() = default;

  // Turns magnifier feature on or off.
  virtual void SetEnabled(bool enabled) = 0;

  // Returns true if magnification feature is on.
  virtual bool IsEnabled() const = 0;

  // Adjust the ratio of the scale of magnification.
  virtual void SetMagnificationScale(float magnification_scale) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_ACCESSIBILITY_MAGNIFICATION_CONTROLLER_H_
