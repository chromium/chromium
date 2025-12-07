// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_RENDERER_GEOMETRY_CHANGE_HANDLER_H_
#define CHROMECAST_STARBOARD_MEDIA_RENDERER_GEOMETRY_CHANGE_HANDLER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "chromecast/media/service/mojom/video_geometry_setter.mojom.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace chromecast {
namespace media {

class VideoGeometrySetterService;

// Receives notifications of geometry changes and sets an SbPlayer's bounds
// accordingly. If no SbPlayer is available when a geometry change occurs, the
// player's bounds will be set once an SbPlayer is assigned via SetSbPlayer.
//
// If no bounds have been set by the VideoGeometrySetterService, this class will
// use the display resolution to set the bounds to fullscreen by default.
//
// A GeometryChangeHandler must be used on a single sequence.
class GeometryChangeHandler : public mojom::VideoGeometryChangeClient {
 public:
  // `geometry_setter_service` and `starboard` must outlive this object, and
  // cannot be null.
  GeometryChangeHandler(VideoGeometrySetterService* geometry_setter_service,
                        StarboardApiWrapper* starboard,
                        const base::UnguessableToken& overlay_plane_id);

  // Disallow copy and assign.
  GeometryChangeHandler(const GeometryChangeHandler&) = delete;
  GeometryChangeHandler& operator=(const GeometryChangeHandler&) = delete;

  ~GeometryChangeHandler() override;

  // Sets the SbPlayer that GeometryChangeHandler will notify of bounds changes.
  // SbPlayerSetBounds will be called immediately. If a bounds change was
  // pending, that geometry will be used. Otherwise, the bounds will be set to
  // fullscreen.
  //
  // `sb_player` must not be null.
  void SetSbPlayer(void* sb_player);

  // mojom::VideoGeometryChangeClient implementation.
  void OnVideoGeometryChange(const gfx::RectF& rect_f,
                             gfx::OverlayTransform transform) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Remote<mojom::VideoGeometryChangeSubscriber>
      geometry_change_subscriber_remote_;
  mojo::Receiver<mojom::VideoGeometryChangeClient>
      geometry_change_client_receiver_{this};

  raw_ptr<StarboardApiWrapper> starboard_ = nullptr;
  // This is nullopt if a geometry has not yet been set.
  std::optional<gfx::RectF> current_geometry_;

  raw_ptr<void> sb_player_ = nullptr;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_RENDERER_GEOMETRY_CHANGE_HANDLER_H_
