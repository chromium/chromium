// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_PROCESSOR_IMPL_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_PROCESSOR_IMPL_H_

#include "components/no_state_prefetch/browser/no_state_prefetch_processor_impl_delegate.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace prerender {

// NoStatePrefetchProcessorImpl implements
// blink::mojom::NoStatePrefetchProcessor and works as the browser-side entry
// point of NoStatePrefetch for <link rel=prerender>. This is a self-owned
// object and deletes itself when the mojo connection is lost.
class NoStatePrefetchProcessorImpl
    : public blink::mojom::NoStatePrefetchProcessor {
 public:
  NoStatePrefetchProcessorImpl(
      int render_process_id,
      int render_frame_id,
      const url::Origin& initiator_origin,
      mojo::PendingReceiver<blink::mojom::NoStatePrefetchProcessor> receiver,
      std::unique_ptr<NoStatePrefetchProcessorImplDelegate> delegate);
  ~NoStatePrefetchProcessorImpl() override;

  static void Create(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<blink::mojom::NoStatePrefetchProcessor> receiver,
      std::unique_ptr<NoStatePrefetchProcessorImplDelegate> delegate);

  // blink::mojom::NoStatePrefetchProcessor implementation
  void Start(blink::mojom::PrerenderAttributesPtr attributes) override;
  void Cancel() override;

 private:
  // Abandons prefetch and deletes `this`. Called from the mojo disconnect
  // handler.
  void Abandon();

  NoStatePrefetchLinkManager* GetNoStatePrefetchLinkManager();

  const int render_process_id_;
  const int render_frame_id_;
  const url::Origin initiator_origin_;
  const std::unique_ptr<NoStatePrefetchProcessorImplDelegate> delegate_;

  // The ID of NoStatePrefetchLinkManager::LinkTrigger. Used for canceling or
  // abandoning prefetch.
  std::optional<int> link_trigger_id_;

  mojo::Receiver<blink::mojom::NoStatePrefetchProcessor> receiver_{this};
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_PROCESSOR_IMPL_H_
