// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_router_ui_helper.h"

#include "base/atomic_sequence_num.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "extensions/browser/extension_registry.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/cocoa/permissions_utils.h"
#endif

namespace media_router {

namespace {

// The amount of time to wait for a response when creating a new route.
const int kCreateRouteTimeoutSeconds = 20;
const int kCreateRouteTimeoutSecondsForTab = 60;
const int kCreateRouteTimeoutSecondsForDesktop = 120;
const int kCreateRouteTimeoutSecondsForRemotePlayback = 60;

#if BUILDFLAG(IS_MAC)
std::optional<bool> g_screen_capture_allowed_for_testing;
#endif

}  // namespace

std::string GetExtensionName(const GURL& gurl,
                             extensions::ExtensionRegistry* registry) {
  if (gurl.is_empty() || !registry)
    return std::string();

  const extensions::Extension* extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(gurl);

  return extension ? extension->name() : std::string();
}

std::string GetHostFromURL(const GURL& gurl) {
  if (gurl.is_empty())
    return std::string();
  std::string host = gurl.host();
  if (base::StartsWith(host, "www.", base::CompareCase::INSENSITIVE_ASCII))
    host = host.substr(4);
  return host;
}

base::TimeDelta GetRouteRequestTimeout(MediaCastMode cast_mode) {
  switch (cast_mode) {
    case PRESENTATION:
      return base::Seconds(kCreateRouteTimeoutSeconds);
    case TAB_MIRROR:
      return base::Seconds(kCreateRouteTimeoutSecondsForTab);
    case DESKTOP_MIRROR:
      return base::Seconds(kCreateRouteTimeoutSecondsForDesktop);
    case REMOTE_PLAYBACK:
      return base::Seconds(kCreateRouteTimeoutSecondsForRemotePlayback);
    default:
      NOTREACHED_IN_MIGRATION();
      return base::TimeDelta();
  }
}

bool RequiresScreenCapturePermission(MediaCastMode cast_mode) {
#if BUILDFLAG(IS_MAC)
  return cast_mode == MediaCastMode::DESKTOP_MIRROR;
#else
  return false;
#endif
}

bool GetScreenCapturePermission() {
#if BUILDFLAG(IS_MAC)
  return g_screen_capture_allowed_for_testing.has_value()
             ? *g_screen_capture_allowed_for_testing
             : (ui::IsScreenCaptureAllowed() ||
                ui::TryPromptUserForScreenCapture());
#else
  return true;
#endif
}

void set_screen_capture_allowed_for_testing(bool allowed) {
#if BUILDFLAG(IS_MAC)
  g_screen_capture_allowed_for_testing = allowed;
#endif
}

void clear_screen_capture_allowed_for_testing() {
#if BUILDFLAG(IS_MAC)
  g_screen_capture_allowed_for_testing.reset();
#endif
}

RouteRequest::RouteRequest(const MediaSink::Id& sink_id) : sink_id(sink_id) {
  static base::AtomicSequenceNumber g_next_request_id;
  id = g_next_request_id.GetNext();
}

RouteRequest::~RouteRequest() = default;

RouteParameters::RouteParameters() = default;

RouteParameters::RouteParameters(RouteParameters&& other) = default;

RouteParameters::~RouteParameters() = default;

RouteParameters& RouteParameters::operator=(RouteParameters&& other) = default;

MediaRouterUIParameters::MediaRouterUIParameters(
    CastModeSet initial_modes,
    content::WebContents* initiator,
    std::unique_ptr<StartPresentationContext> start_presentation_context,
    media::VideoCodec video_codec,
    media::AudioCodec audio_codec)
    : initial_modes(initial_modes),
      initiator(initiator),
      start_presentation_context(std::move(start_presentation_context)),
      video_codec(video_codec),
      audio_codec(audio_codec) {}

MediaRouterUIParameters::MediaRouterUIParameters(
    MediaRouterUIParameters&& other) = default;

MediaRouterUIParameters::~MediaRouterUIParameters() = default;

}  // namespace media_router
