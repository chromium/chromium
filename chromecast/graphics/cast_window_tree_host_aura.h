// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_WINDOW_TREE_HOST_AURA_H_
#define CHROMECAST_GRAPHICS_CAST_WINDOW_TREE_HOST_AURA_H_

#include <memory>
#include <optional>

#include "chromecast/starboard/chromecast/events/ui_event_source.h"
#include "ui/aura/window_tree_host_platform.h"

namespace chromecast {

// An aura::WindowTreeHost that correctly converts input events.
class CastWindowTreeHostAura : public aura::WindowTreeHostPlatform {
 public:
  explicit CastWindowTreeHostAura(ui::PlatformWindowInitProperties properties);

  CastWindowTreeHostAura(const CastWindowTreeHostAura&) = delete;
  CastWindowTreeHostAura& operator=(const CastWindowTreeHostAura&) = delete;

  ~CastWindowTreeHostAura() override;

  // aura::WindowTreeHostPlatform implementation:
  void DispatchEvent(ui::Event* event) override;
  void OnBoundsChanged(const BoundsChange& change) override;

  // Called by UiEventSource when window size changes
  void OnWindowSizeChanged(const gfx::Size& new_size);

  // Called by UiEventSource when Starboard render root transform changes
  void OnTransformChanged(const gfx::Transform& transform);

  // Sets the initial primary display rotation transform (in DIPs).
  void SetDefaultRootTransform(const gfx::Transform& transform);

  // Enables or disables applying Starboard screen rotation transforms.
  void SetRotationSupported(bool supported);

  // aura::WindowTreeHost implementation
  gfx::Rect GetTransformedRootWindowBoundsFromPixelSize(
      const gfx::Size& size_in_pixels) const override;

 private:
  void UpdateDisplayMetrics();

  bool rotation_supported_ = false;
  gfx::Transform default_root_transform_;
  std::optional<gfx::Transform> last_starboard_transform_;
  std::unique_ptr<UiEventSource> ui_event_source_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_WINDOW_TREE_HOST_AURA_H_
