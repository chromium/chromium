// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/media_route_provider_helper.h"

#include <ostream>
#include <string_view>

#include "base/notreached.h"
#include "components/media_router/common/mojom/media_route_provider_id.mojom.h"

constexpr const char kWiredDisplay[] = "WIRED_DISPLAY";
constexpr const char kDial[] = "DIAL";
constexpr const char kCast[] = "CAST";
constexpr const char kAndroidCaf[] = "ANDROID_CAF";
constexpr const char kTest[] = "TEST";

namespace media_router {

const char* ProviderIdToString(mojom::MediaRouteProviderId provider_id) {
  switch (provider_id) {
    case mojom::MediaRouteProviderId::WIRED_DISPLAY:
      return kWiredDisplay;
    case mojom::MediaRouteProviderId::CAST:
      return kCast;
    case mojom::MediaRouteProviderId::DIAL:
      return kDial;
    case mojom::MediaRouteProviderId::ANDROID_CAF:
      return kAndroidCaf;
    case mojom::MediaRouteProviderId::TEST:
      return kTest;
  }

  NOTREACHED_IN_MIGRATION()
      << "Unknown provider_id " << static_cast<int>(provider_id);
  return "Unknown provider_id";
}

std::optional<mojom::MediaRouteProviderId> ProviderIdFromString(
    std::string_view provider_id) {
  if (provider_id == kWiredDisplay) {
    return mojom::MediaRouteProviderId::WIRED_DISPLAY;
  } else if (provider_id == kCast) {
    return mojom::MediaRouteProviderId::CAST;
  } else if (provider_id == kDial) {
    return mojom::MediaRouteProviderId::DIAL;
  } else if (provider_id == kAndroidCaf) {
    return mojom::MediaRouteProviderId::ANDROID_CAF;
  } else if (provider_id == kTest) {
    return mojom::MediaRouteProviderId::TEST;
  } else {
    return std::nullopt;
  }
}

}  // namespace media_router
