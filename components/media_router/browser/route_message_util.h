// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_ROUTE_MESSAGE_UTIL_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_ROUTE_MESSAGE_UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "components/media_router/common/mojom/media_router.mojom.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace base {
class Value;
}  // namespace base

namespace media_router {
namespace message_util {

media_router::mojom::RouteMessagePtr RouteMessageFromValue(base::Value value);

media_router::mojom::RouteMessagePtr RouteMessageFromString(
    std::string message);

media_router::mojom::RouteMessagePtr RouteMessageFromData(
    std::vector<uint8_t> data);

blink::mojom::PresentationConnectionMessagePtr
PresentationConnectionFromRouteMessage(
    media_router::mojom::RouteMessagePtr route_message);

}  // namespace message_util
}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_ROUTE_MESSAGE_UTIL_H_
