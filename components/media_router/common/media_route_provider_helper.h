// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_ROUTE_PROVIDER_HELPER_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_ROUTE_PROVIDER_HELPER_H_

#include "base/strings/string_piece_forward.h"

namespace media_router {

// Each MediaRouteProvider is associated with a unique ID. This enum must be
// kept in sync with mojom::MediaRouteProvider::Id, except for |UNKNOWN|, which
// is not present in the Mojo enum.
// FIXME: Can we just use the mojo enum instead?
enum MediaRouteProviderId {
  EXTENSION,
  WIRED_DISPLAY,
  CAST,
  DIAL,
  ANDROID_CAF,
  TEST,
  UNKNOWN  // New values must be added above this value.
};

const char* ProviderIdToString(MediaRouteProviderId provider_id);
MediaRouteProviderId ProviderIdFromString(base::StringPiece provider_id);

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_ROUTE_PROVIDER_HELPER_H_
