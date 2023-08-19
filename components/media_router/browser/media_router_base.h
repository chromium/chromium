// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_BASE_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_BASE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/common/media_route.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace media_router {

class MediaRouterBase : public MediaRouter {
 public:
  MediaRouterBase(const MediaRouterBase&) = delete;
  MediaRouterBase(MediaRouterBase&&) = delete;
  MediaRouterBase& operator=(const MediaRouterBase&) = delete;
  MediaRouterBase& operator=(MediaRouterBase&&) = delete;

  ~MediaRouterBase() override;

  // MediaRouter:
  base::CallbackListSubscription AddPresentationConnectionStateChangedCallback(
      const MediaRoute::Id& route_id,
      const content::PresentationConnectionStateChangedCallback& callback)
      override;

 protected:
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest,
                           PresentationConnectionStateChangedCallback);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest,
                           PresentationConnectionStateChangedCallbackRemoved);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterBaseTest, CreatePresentationIds);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterBaseTest, NotifyCallbacks);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest,
                           ListenForConnectionStateChange);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest, GetMediaRoutes);

  MediaRouterBase();

  // Generates a unique presentation ID.
  static std::string CreatePresentationId();

  void NotifyPresentationConnectionStateChange(
      const MediaRoute::Id& route_id,
      blink::mojom::PresentationConnectionState state);
  void NotifyPresentationConnectionClose(
      const MediaRoute::Id& route_id,
      blink::mojom::PresentationConnectionCloseReason reason,
      const std::string& message);

  using PresentationConnectionStateChangedCallbacks =
      base::RepeatingCallbackList<void(
          const content::PresentationConnectionStateChangeInfo&)>;

  std::unordered_map<
      MediaRoute::Id,
      std::unique_ptr<PresentationConnectionStateChangedCallbacks>>
      presentation_connection_state_callbacks_;

 private:
  friend class MediaRouterBaseTest;
  friend class MediaRouterMojoTest;

  // Called when a PresentationConnectionStateChangedCallback associated with
  // |route_id| is removed from |presentation_connection_state_callbacks_|.
  void OnPresentationConnectionStateCallbackRemoved(
      const MediaRoute::Id& route_id);
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_BASE_H_
