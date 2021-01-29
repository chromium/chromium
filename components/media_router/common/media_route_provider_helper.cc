// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/media_route_provider_helper.h"

#include "base/notreached.h"
#include "base/strings/string_piece.h"

constexpr const char kExtension[] = "EXTENSION";
constexpr const char kWiredDisplay[] = "WIRED_DISPLAY";
constexpr const char kDial[] = "DIAL";
constexpr const char kCast[] = "CAST";
constexpr const char kAndroidCaf[] = "ANDROID_CAF";
constexpr const char kTest[] = "TEST";
constexpr const char kUnknown[] = "UNKNOWN";

namespace media_router {

const char* ProviderIdToString(MediaRouteProviderId provider_id) {
  switch (provider_id) {
    case EXTENSION:
      return kExtension;
    case WIRED_DISPLAY:
      return kWiredDisplay;
    case CAST:
      return kCast;
    case DIAL:
      return kDial;
    case ANDROID_CAF:
      return kAndroidCaf;
    case TEST:
      return kTest;
    case UNKNOWN:
      return kUnknown;
  }

  NOTREACHED() << "Unknown provider_id " << static_cast<int>(provider_id);
  return "Unknown provider_id";
}

MediaRouteProviderId ProviderIdFromString(base::StringPiece provider_id) {
  if (provider_id == kExtension) {
    return MediaRouteProviderId::EXTENSION;
  } else if (provider_id == kWiredDisplay) {
    return MediaRouteProviderId::WIRED_DISPLAY;
  } else if (provider_id == kCast) {
    return MediaRouteProviderId::CAST;
  } else if (provider_id == kDial) {
    return MediaRouteProviderId::DIAL;
  } else if (provider_id == kAndroidCaf) {
    return MediaRouteProviderId::ANDROID_CAF;
  } else if (provider_id == kTest) {
    return MediaRouteProviderId::TEST;
  } else {
    return MediaRouteProviderId::UNKNOWN;
  }
}

}  // namespace media_router
