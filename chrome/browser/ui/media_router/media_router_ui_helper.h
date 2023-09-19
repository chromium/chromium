// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_HELPER_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_HELPER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"
#include "url/origin.h"

namespace extensions {
class ExtensionRegistry;
}

class GURL;

namespace media_router {

class StartPresentationContext;

// Returns the extension name for |url|, so that it can be displayed for
// extension-initiated presentations.
std::string GetExtensionName(const GURL& url,
                             extensions::ExtensionRegistry* registry);

std::string GetHostFromURL(const GURL& gurl);

// Returns the duration to wait for route creation result before we time out.
base::TimeDelta GetRouteRequestTimeout(MediaCastMode cast_mode);

// Determines if the specified cast mode requires permission from the user
// in order to proceed.
bool RequiresScreenCapturePermission(MediaCastMode cast_mode);

// Requests permission for screen capturing.
bool GetScreenCapturePermission();

void set_screen_capture_allowed_for_testing(bool allowed);
void clear_screen_capture_allowed_for_testing();

// A variation of MediaRouteResponseCallback that doesn't require the
// PresentationConnection objects.
using MediaRouteResultCallback =
    base::OnceCallback<void(const RouteRequestResult&)>;

struct RouteRequest {
 public:
  explicit RouteRequest(const MediaSink::Id& sink_id);
  ~RouteRequest();

  int id;
  MediaSink::Id sink_id;
};

// Contains parameters passed to MediaRouterUI's constructor.
struct MediaRouterUIParameters {
  MediaRouterUIParameters(
      CastModeSet initial_modes,
      content::WebContents* initiator,
      std::unique_ptr<StartPresentationContext> start_presentation_context =
          nullptr,
      media::VideoCodec video_codec = media::VideoCodec::kUnknown,
      media::AudioCodec audio_codec = media::AudioCodec::kUnknown);

  MediaRouterUIParameters(const MediaRouterUIParameters& other) = delete;
  MediaRouterUIParameters(MediaRouterUIParameters&& other);
  ~MediaRouterUIParameters();
  MediaRouterUIParameters& operator=(MediaRouterUIParameters&) = delete;
  MediaRouterUIParameters& operator=(MediaRouterUIParameters&&) = default;

  CastModeSet initial_modes;
  raw_ptr<content::WebContents> initiator;
  // Used to initialize MediaRouterUI with a PresentationRequest.
  std::unique_ptr<StartPresentationContext> start_presentation_context;
  // Used to initialize MediaRouterUI with RemotePlayback Media Source.
  media::VideoCodec video_codec;
  media::AudioCodec audio_codec;
};

// Contains common parameters for route requests to MediaRouter.
struct RouteParameters {
 public:
  RouteParameters();
  RouteParameters(RouteParameters&& other);
  ~RouteParameters();

  RouteParameters& operator=(RouteParameters&& other);

  MediaCastMode cast_mode;

  // A string identifying the media source, which should be the source for this
  // route (e.g. a presentation url, tab mirroring id, etc.).
  MediaSource::Id source_id;

  // Unique id identifying the attempt to connect to a specific sink
  std::unique_ptr<RouteRequest> request;

  // The origin of the page requesting the route.
  url::Origin origin;

  // Callbacks which should be invoked on both success and failure of the route
  // creation.
  std::vector<MediaRouteResultCallback> route_result_callbacks;

  // A timeout value, after which the request should be considered to have
  // failed.
  base::TimeDelta timeout;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_HELPER_H_
