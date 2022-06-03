// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_HELPER_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_HELPER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/common/media_source.h"
#include "url/origin.h"

namespace extensions {
class ExtensionRegistry;
}

class GURL;

namespace media_router {

// Returns the extension name for |url|, so that it can be displayed for
// extension-initiated presentations.
std::string GetExtensionName(const GURL& url,
                             extensions::ExtensionRegistry* registry);

std::string GetHostFromURL(const GURL& gurl);

// Returns the duration to wait for route creation result before we time out.
base::TimeDelta GetRouteRequestTimeout(MediaCastMode cast_mode);

// A variation of MediaRouteResponseCallback that doesn't require the
// PresentationConnection objects.
using MediaRouteResultCallback =
    base::OnceCallback<void(const RouteRequestResult&)>;

// Contains common parameters for route requests to MediaRouter.
struct RouteParameters {
 public:
  RouteParameters();
  RouteParameters(RouteParameters&& other);
  ~RouteParameters();

  RouteParameters& operator=(RouteParameters&& other);

  // A string identifying the media source, which should be the source for this
  // route (e.g. a presentation url, tab mirroring id, etc.).
  MediaSource::Id source_id;

  // The origin of the page requesting the route.
  url::Origin origin;

  // This callback will be null if the route request is not for a presentation
  // (e.g. it is for tab mirroring).
  MediaRouteResponseCallback presentation_callback;

  // Callbacks which should be invoked on both success and failure of the route
  // creation.
  std::vector<MediaRouteResultCallback> route_result_callbacks;

  // A timeout value, after which the request should be considered to have
  // failed.
  base::TimeDelta timeout;

  // Whether the route is for an off-the-record profile.
  bool off_the_record;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_HELPER_H_
