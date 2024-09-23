// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CONTENT_RENDERER_FACILITATED_PAYMENTS_AGENT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CONTENT_RENDERER_FACILITATED_PAYMENTS_AGENT_H_

#include "base/memory/weak_ptr.h"
#include "components/facilitated_payments/core/mojom/facilitated_payments_agent.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace blink {
class AssociatedInterfaceRegistry;
}  // namespace blink

namespace payments::facilitated {

class FacilitatedPaymentsAgent : public content::RenderFrameObserver,
                                 public mojom::FacilitatedPaymentsAgent {
 public:
  FacilitatedPaymentsAgent(content::RenderFrame* render_frame,
                           blink::AssociatedInterfaceRegistry* registry);

  FacilitatedPaymentsAgent(const FacilitatedPaymentsAgent&) = delete;
  FacilitatedPaymentsAgent& operator=(const FacilitatedPaymentsAgent&) = delete;

  ~FacilitatedPaymentsAgent() override;

 private:
  // mojom::FacilitatedPaymentsAgent:
  void TriggerPixCodeDetection(
      base::OnceCallback<void(mojom::PixCodeDetectionResult,
                              const std::string&)> callback) override;

  // content::RenderFrameObserver:
  void OnDestruct() override;

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::FacilitatedPaymentsAgent>
          pending_receiver);

  bool will_destruct_ = false;
  mojo::AssociatedReceiver<mojom::FacilitatedPaymentsAgent> receiver_{this};
  base::WeakPtrFactory<FacilitatedPaymentsAgent> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CONTENT_RENDERER_FACILITATED_PAYMENTS_AGENT_H_
