// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/presentation/start_presentation_context.h"

#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/route_request_result.h"

namespace media_router {

StartPresentationContext::StartPresentationContext(
    const content::PresentationRequest& presentation_request,
    PresentationConnectionCallback success_cb,
    PresentationConnectionErrorCallback error_cb)
    : presentation_request_(presentation_request),
      success_cb_(std::move(success_cb)),
      error_cb_(std::move(error_cb)) {
  DCHECK(success_cb_);
  DCHECK(error_cb_);
}

StartPresentationContext::~StartPresentationContext() {
  if (success_cb_ && error_cb_) {
    std::move(error_cb_).Run(blink::mojom::PresentationError(
        blink::mojom::PresentationErrorType::UNKNOWN, "Unknown error."));
  }
}

void StartPresentationContext::InvokeSuccessCallback(
    const std::string& presentation_id,
    const GURL& presentation_url,
    const MediaRoute& route,
    mojom::RoutePresentationConnectionPtr connection) {
  if (success_cb_ && error_cb_) {
    std::move(success_cb_)
        .Run(blink::mojom::PresentationInfo(presentation_url, presentation_id),
             std::move(connection), route);
  }
}

void StartPresentationContext::InvokeErrorCallback(
    const blink::mojom::PresentationError& error) {
  if (success_cb_ && error_cb_) {
    std::move(error_cb_).Run(error);
  }
}

void StartPresentationContext::HandleRouteResponse(
    mojom::RoutePresentationConnectionPtr connection,
    const RouteRequestResult& result) {
  if (!result.route()) {
    InvokeErrorCallback(blink::mojom::PresentationError(
        blink::mojom::PresentationErrorType::UNKNOWN, result.error()));
  } else {
    InvokeSuccessCallback(result.presentation_id(), result.presentation_url(),
                          *result.route(), std::move(connection));
  }
}

}  // namespace media_router
