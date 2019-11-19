// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_WINDOW_TREE_HOST_AURA_H_
#define CHROMECAST_GRAPHICS_CAST_WINDOW_TREE_HOST_AURA_H_

#include "ui/aura/window_tree_host_platform.h"

namespace chromecast {

// An aura::WindowTreeHost that correctly converts input events.
class CastWindowTreeHostAura : public aura::WindowTreeHostPlatform {
 public:
  CastWindowTreeHostAura(bool enable_input,
                         ui::PlatformWindowInitProperties properties,
                         bool use_external_frame_control = false);
  ~CastWindowTreeHostAura() override;

  // aura::WindowTreeHostPlatform implementation:
  void DispatchEvent(ui::Event* event) override;

  // aura::WindowTreeHost implementation
  gfx::Rect GetTransformedRootWindowBoundsInPixels(
      const gfx::Size& size_in_pixels) const override;

 private:
  const bool enable_input_;

  DISALLOW_COPY_AND_ASSIGN(CastWindowTreeHostAura);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_WINDOW_TREE_HOST_AURA_H_