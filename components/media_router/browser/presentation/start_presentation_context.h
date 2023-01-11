// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_START_PRESENTATION_CONTEXT_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_START_PRESENTATION_CONTEXT_H_

#include "base/functional/callback.h"
#include "components/media_router/common/mojom/media_router.mojom-forward.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/presentation_service_delegate.h"

namespace content {
struct PresentationRequest;
}  // namespace content

namespace media_router {

class MediaRoute;
class RouteRequestResult;

// Helper data structure to hold information for a request from the
// Presentation API. Contains information on the PresentationRequest, and
// success / error callbacks. Depending on the route creation outcome,
// only one of the callbacks will be invoked exactly once.
class StartPresentationContext {
 public:
  using PresentationConnectionCallback =
      base::OnceCallback<void(const blink::mojom::PresentationInfo&,
                              mojom::RoutePresentationConnectionPtr,
                              const MediaRoute&)>;
  using PresentationConnectionErrorCallback =
      content::PresentationConnectionErrorCallback;

  StartPresentationContext(
      const content::PresentationRequest& presentation_request,
      PresentationConnectionCallback success_cb,
      PresentationConnectionErrorCallback error_cb);
  ~StartPresentationContext();

  const content::PresentationRequest& presentation_request() const {
    return presentation_request_;
  }

  // Invokes |success_cb_| or |error_cb_| with the given arguments.
  void InvokeSuccessCallback(const std::string& presentation_id,
                             const GURL& presentation_url,
                             const MediaRoute& route,
                             mojom::RoutePresentationConnectionPtr connection);
  void InvokeErrorCallback(const blink::mojom::PresentationError& error);

  // Handle route creation/joining response by invoking the right callback.
  void HandleRouteResponse(mojom::RoutePresentationConnectionPtr connection,
                           const RouteRequestResult& result);

 private:
  const content::PresentationRequest presentation_request_;
  PresentationConnectionCallback success_cb_;
  PresentationConnectionErrorCallback error_cb_;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_START_PRESENTATION_CONTEXT_H_
