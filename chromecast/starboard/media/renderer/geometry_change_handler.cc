// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/geometry_change_handler.h"

#include "base/check.h"
#include "base/logging.h"
#include "chromecast/media/service/video_geometry_setter_service.h"
#include "ui/display/screen.h"

namespace chromecast {
namespace media {

namespace {

// Sets the SbPlayer's bounds as specified. `starboard` and `sb_player` must not
// be null.
void SetPlayerBounds(const gfx::RectF& bounds,
                     StarboardApiWrapper* starboard,
                     void* sb_player) {
  CHECK(starboard);
  CHECK(sb_player);

  LOG(INFO) << "Setting SbPlayer's bounds to z=0, x=" << bounds.x()
            << ", y=" << bounds.y() << ", width=" << bounds.width()
            << ", height=" << bounds.height();
  starboard->SetPlayerBounds(
      sb_player, /*z_index=*/0, static_cast<int>(bounds.x()),
      static_cast<int>(bounds.y()), static_cast<int>(bounds.width()),
      static_cast<int>(bounds.height()));
}

}  // namespace

GeometryChangeHandler::GeometryChangeHandler(
    VideoGeometrySetterService* geometry_setter_service,
    StarboardApiWrapper* starboard,
    const base::UnguessableToken& overlay_plane_id)
    : starboard_(starboard) {
  CHECK(starboard_);
  CHECK(geometry_setter_service);
  geometry_setter_service->GetVideoGeometryChangeSubscriber(
      geometry_change_subscriber_remote_.BindNewPipeAndPassReceiver());
  geometry_change_subscriber_remote_->SubscribeToVideoGeometryChange(
      overlay_plane_id,
      geometry_change_client_receiver_.BindNewPipeAndPassRemote(),
      base::DoNothing());
}

GeometryChangeHandler::~GeometryChangeHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GeometryChangeHandler::SetSbPlayer(void* sb_player) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(sb_player);
  sb_player_ = sb_player;

  // Update the player's bounds.
  if (current_geometry_.has_value()) {
    // Use the bounds specified by a mojo call.
    SetPlayerBounds(*current_geometry_, starboard_, sb_player_);
  } else {
    // Default to fullscreen.
    const gfx::Size display_size =
        display::Screen::Get()->GetPrimaryDisplay().GetSizeInPixel();
    SetPlayerBounds(
        gfx::RectF(0, 0, display_size.width(), display_size.height()),
        starboard_, sb_player_);
  }
}

void GeometryChangeHandler::OnVideoGeometryChange(
    const gfx::RectF& rect_f,
    gfx::OverlayTransform transform) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_geometry_.has_value() && rect_f == *current_geometry_) {
    return;
  }

  current_geometry_ = rect_f;
  if (sb_player_) {
    SetPlayerBounds(*current_geometry_, starboard_, sb_player_);
  }
}

}  // namespace media
}  // namespace chromecast
