// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/rounded_window_corners.h"

namespace chromecast {

namespace {

// A no-op default implementation of RoundedWindowCorners.
class RoundedWindowCornersDefault : public RoundedWindowCorners {
 public:
  RoundedWindowCornersDefault() {}

  RoundedWindowCornersDefault(const RoundedWindowCornersDefault&) = delete;
  RoundedWindowCornersDefault& operator=(const RoundedWindowCornersDefault&) =
      delete;

  ~RoundedWindowCornersDefault() override {}

  void SetEnabled(bool enable) override {}
  bool IsEnabled() const override { return false; }
  void SetColorInversion(bool enable) override {}
};

}  // namespace

// static
std::unique_ptr<RoundedWindowCorners> RoundedWindowCorners::Create(
    CastWindowManager* window_manager) {
  return std::make_unique<RoundedWindowCornersDefault>();
}

}  // namespace chromecast
