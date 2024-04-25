// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_ROUTE_PROVIDER_HELPER_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_ROUTE_PROVIDER_HELPER_H_

#include <optional>
#include <string_view>

#include "components/media_router/common/mojom/media_route_provider_id.mojom-forward.h"

namespace media_router {

const char* ProviderIdToString(mojom::MediaRouteProviderId provider_id);
std::optional<mojom::MediaRouteProviderId> ProviderIdFromString(
    std::string_view provider_id);

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_ROUTE_PROVIDER_HELPER_H_
