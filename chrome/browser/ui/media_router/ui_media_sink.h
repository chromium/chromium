// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_UI_MEDIA_SINK_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_UI_MEDIA_SINK_H_

#include <optional>
#include <string>

#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "components/media_router/common/issue.h"
#include "components/media_router/common/media_route_provider_helper.h"
#include "components/media_router/common/media_sink.h"
#include "url/gurl.h"

namespace media_router {

enum class UIMediaSinkState {
  // Sink is available to be Cast to.
  AVAILABLE,
  // Sink is starting a new Casting activity. A sink temporarily enters this
  // state when transitioning from AVAILABLE to CONNECTED.
  CONNECTING,
  // Sink has a media route.
  CONNECTED,
  // Sink is still connected but is in the process of disconnecting. A sink
  // temporarily enters this state when transitioning from CONNECTED to
  // AVAILABLE.
  DISCONNECTING,
  // Sink is disconnected/cached (not available right now).
  UNAVAILABLE
};

struct FreezeInfo {
  // Whether the route supports freeze / unfreeze.
  bool can_freeze = false;

  // The current freeze state of the route.
  bool is_frozen = false;
};

struct UIMediaSink {
 public:
  explicit UIMediaSink(mojom::MediaRouteProviderId provider);
  UIMediaSink(const UIMediaSink& other);
  ~UIMediaSink();

  // Gets the status text that should be shown in the UI based on `status_text`,
  // `state`, and `issue`.
  std::u16string GetStatusTextForDisplay() const;

  // The unique ID for the media sink.
  std::string id;

  // Name that can be used by the user to identify the sink.
  std::u16string friendly_name;

  // Normally the sink status text is set from |state|. This field allows it
  // to be overridden for error states or to show route descriptions.
  std::u16string status_text;

  // Presentation URL to use when initiating a new casting activity for this
  // sink. For sites that integrate with the Presentation API, this is the
  // top frame presentation URL. Mirroring, Cast apps, and DIAL apps use a
  // non-https scheme.
  GURL presentation_url;

  // Active route associated with the sink.
  std::optional<MediaRoute> route;

  // The icon to use for the sink.
  SinkIconType icon_type = SinkIconType::GENERIC;

  // The provider of the sink.
  mojom::MediaRouteProviderId provider;

  // The current state of the media sink.
  UIMediaSinkState state = UIMediaSinkState::AVAILABLE;

  // An issue the sink is having. This is a nullopt when there are no issues
  // with the sink.
  std::optional<Issue> issue;

  // Set of Cast Modes (e.g. presentation, desktop mirroring) supported by the
  // sink.
  CastModeSet cast_modes;

  // The current information related to freeze.
  FreezeInfo freeze_info;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_UI_MEDIA_SINK_H_
