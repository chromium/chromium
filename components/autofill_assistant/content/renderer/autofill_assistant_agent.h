// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_AGENT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_AGENT_H_

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/content/common/autofill_assistant_agent.mojom.h"
#include "components/autofill_assistant/content/common/autofill_assistant_driver.mojom.h"
#include "components/autofill_assistant/content/common/node_data.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace autofill_assistant {

// Autofill assistant agent to control a web page.
class AutofillAssistantAgent : public content::RenderFrameObserver,
                               public mojom::AutofillAssistantAgent {
 public:
  AutofillAssistantAgent(content::RenderFrame* render_frame,
                         blink::AssociatedInterfaceRegistry* registry);
  ~AutofillAssistantAgent() override;

  AutofillAssistantAgent(const AutofillAssistantAgent&) = delete;
  void operator=(const AutofillAssistantAgent&) = delete;

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutofillAssistantAgent>
          pending_receiver);

  base::WeakPtr<AutofillAssistantAgent> GetWeakPtr();

  // mojom::AutofillAssistantAgent:
  void GetSemanticNodes(int32_t role,
                        int32_t objective,
                        GetSemanticNodesCallback callback) override;

  void GetAnnotateDomModel(base::OnceCallback<void(base::File)> callback);

 private:
  // content::RenderFrameObserver:
  void OnDestruct() override;

  const mojo::Remote<mojom::AutofillAssistantDriver>& GetDriver();

  mojo::Remote<mojom::AutofillAssistantDriver> driver_;

  mojo::AssociatedReceiver<mojom::AutofillAssistantAgent> receiver_{this};
  base::WeakPtrFactory<AutofillAssistantAgent> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_AGENT_H_
