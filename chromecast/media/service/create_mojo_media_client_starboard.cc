// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_features.h"
#include "chromecast/media/service/cast_mojo_media_client.h"
#include "chromecast/media/service/cast_starboard_mojo_media_client.h"
#include "chromecast/media/service/create_mojo_media_client.h"

namespace chromecast {
namespace media {

std::unique_ptr<::media::MojoMediaClient> CreateMojoMediaClientForCast(
    CmaBackendFactory* backend_factory,
    CreateCdmFactoryCB create_cdm_factory_cb,
    VideoModeSwitcher* video_mode_switcher,
    VideoResolutionPolicy* video_resolution_policy,
    VideoGeometrySetterService* video_geometry_setter,
    EnableBufferingCB enable_buffering_cb) {
  if (base::FeatureList::IsEnabled(kEnableStarboardRenderer)) {
    return std::make_unique<CastStarboardMojoMediaClient>(
        std::move(create_cdm_factory_cb), std::move(enable_buffering_cb),
        video_geometry_setter);
  }

  // Fall back to CMA.
  return std::make_unique<CastMojoMediaClient>(
      backend_factory, std::move(create_cdm_factory_cb), video_mode_switcher,
      video_resolution_policy, video_geometry_setter,
      std::move(enable_buffering_cb));
}

}  // namespace media
}  // namespace chromecast
