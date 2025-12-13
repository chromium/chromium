// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_SERVICE_CREATE_MOJO_MEDIA_CLIENT_H_
#define CHROMECAST_MEDIA_SERVICE_CREATE_MOJO_MEDIA_CLIENT_H_

#include "base/functional/callback.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

namespace mojom {
class FrameInterfaceFactory;
}  // namespace mojom

class CdmFactory;

}  // namespace media

namespace chromecast {
namespace media {

class CmaBackendFactory;
class VideoGeometrySetterService;
class VideoModeSwitcher;
class VideoResolutionPolicy;

using CreateCdmFactoryCB =
    base::RepeatingCallback<std::unique_ptr<::media::CdmFactory>(
        ::media::mojom::FrameInterfaceFactory*)>;
using EnableBufferingCB = base::RepeatingCallback<bool()>;

// Returns a MojoMediaClient that can be used to create a ::media::Renderer for
// cast. Current implementations provide either a CMA-based renderer
// (CastRenderer) or a StarboardRenderer.
std::unique_ptr<::media::MojoMediaClient> CreateMojoMediaClientForCast(
    CmaBackendFactory* backend_factory,
    CreateCdmFactoryCB create_cdm_factory_cb,
    VideoModeSwitcher* video_mode_switcher,
    VideoResolutionPolicy* video_resolution_policy,
    VideoGeometrySetterService* video_geometry_setter,
    EnableBufferingCB enable_buffering_cb);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_SERVICE_CREATE_MOJO_MEDIA_CLIENT_H_
