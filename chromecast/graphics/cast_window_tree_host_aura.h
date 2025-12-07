// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_WINDOW_TREE_HOST_AURA_H_
#define CHROMECAST_GRAPHICS_CAST_WINDOW_TREE_HOST_AURA_H_

#include <memory>

#include "chromecast/starboard/chromecast/events/ui_event_source.h"
#include "ui/aura/window_tree_host_platform.h"

namespace chromecast {

// An aura::WindowTreeHost that correctly converts input events.
class CastWindowTreeHostAura : public aura::WindowTreeHostPlatform {
 public:
  CastWindowTreeHostAura(bool enable_input,
                         ui::PlatformWindowInitProperties properties);

  CastWindowTreeHostAura(const CastWindowTreeHostAura&) = delete;
  CastWindowTreeHostAura& operator=(const CastWindowTreeHostAura&) = delete;

  ~CastWindowTreeHostAura() override;

  // aura::WindowTreeHostPlatform implementation:
  void DispatchEvent(ui::Event* event) override;

  // aura::WindowTreeHost implementation
  gfx::Rect GetTransformedRootWindowBoundsFromPixelSize(
      const gfx::Size& size_in_pixels) const override;

 private:
  const bool enable_input_;
  std::unique_ptr<UiEventSource> ui_event_source_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_WINDOW_TREE_HOST_AURA_H_
