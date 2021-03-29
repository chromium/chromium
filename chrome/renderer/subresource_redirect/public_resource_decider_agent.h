// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_PUBLIC_RESOURCE_DECIDER_AGENT_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_PUBLIC_RESOURCE_DECIDER_AGENT_H_

#include "base/bind.h"
#include "base/optional.h"
#include "chrome/common/subresource_redirect_service.mojom.h"
#include "chrome/renderer/subresource_redirect/public_resource_decider.h"
#include "components/subresource_redirect/common/subresource_redirect_result.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "url/gurl.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace subresource_redirect {

// Base class for the decider agent classes that decide whether a resource is
// considered public and eligible for redirection for compression. Also allows
// coverage metrics to be recorded for the resource load. This class is also the
// point of contact for browser mojo.
class PublicResourceDeciderAgent
    : public content::RenderFrameObserver,
      public mojom::SubresourceRedirectHintsReceiver,
      public content::RenderFrameObserverTracker<PublicResourceDeciderAgent>,
      public PublicResourceDecider {
 public:
  PublicResourceDeciderAgent(
      blink::AssociatedInterfaceRegistry* associated_interfaces,
      content::RenderFrame* render_frame);
  ~PublicResourceDeciderAgent() override;

  void NotifyCompressedResourceFetchFailed(
      base::TimeDelta retry_after) override;
  base::TimeTicks GetNavigationStartTime() const override;

  mojo::AssociatedRemote<
      subresource_redirect::mojom::SubresourceRedirectService>&
  GetSubresourceRedirectServiceRemote();

  // content::RenderFrameObserver:
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;

 private:
  // content::RenderFrameObserver:
  void OnDestruct() override;

  // Binds the mojo hints receiver pipe.
  void BindHintsReceiver(
      mojo::PendingAssociatedReceiver<mojom::SubresourceRedirectHintsReceiver>
          receiver);

  // Maintains the time the current navigation started.
  base::TimeTicks navigation_start_;

  mojo::AssociatedReceiver<mojom::SubresourceRedirectHintsReceiver>
      subresource_redirect_hints_receiver_{this};

  mojo::AssociatedRemote<
      subresource_redirect::mojom::SubresourceRedirectService>
      subresource_redirect_service_remote_;
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_PUBLIC_RESOURCE_DECIDER_AGENT_H_
