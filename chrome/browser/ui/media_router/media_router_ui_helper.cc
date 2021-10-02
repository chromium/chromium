// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_router_ui_helper.h"

#include "base/time/time.h"
#include "extensions/browser/extension_registry.h"
#include "url/gurl.h"

namespace media_router {

namespace {

// The amount of time to wait for a response when creating a new route.
const int kCreateRouteTimeoutSeconds = 20;
const int kCreateRouteTimeoutSecondsForTab = 60;
const int kCreateRouteTimeoutSecondsForLocalFile = 60;
const int kCreateRouteTimeoutSecondsForDesktop = 120;

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
    case LOCAL_FILE:
      return base::Seconds(kCreateRouteTimeoutSecondsForLocalFile);
    default:
      NOTREACHED();
      return base::TimeDelta();
  }
}

RouteParameters::RouteParameters() = default;

RouteParameters::RouteParameters(RouteParameters&& other) = default;

RouteParameters::~RouteParameters() = default;

RouteParameters& RouteParameters::operator=(RouteParameters&& other) = default;

}  // namespace media_router
