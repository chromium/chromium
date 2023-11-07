// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_ROUTE_PROVIDER_HELPER_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_ROUTE_PROVIDER_HELPER_H_

#include "base/strings/string_piece.h"
#include "components/media_router/common/mojom/media_route_provider_id.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media_router {

const char* ProviderIdToString(mojom::MediaRouteProviderId provider_id);
absl::optional<mojom::MediaRouteProviderId> ProviderIdFromString(
    base::StringPiece provider_id);

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_ROUTE_PROVIDER_HELPER_H_
