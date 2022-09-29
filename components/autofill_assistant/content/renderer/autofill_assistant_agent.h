// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_AGENT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_AGENT_H_

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill_assistant/content/common/autofill_assistant_agent.mojom.h"
#include "components/autofill_assistant/content/common/autofill_assistant_driver.mojom.h"
#include "components/autofill_assistant/content/common/node_data.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace blink {
class WebLocalFrame;
}

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
                        bool ignore_objective,
                        base::TimeDelta model_timeout,
                        GetSemanticNodesCallback callback) override;
  void SetElementValue(int32_t backend_node_id,
                       const std::u16string& value,
                       bool send_events,
                       SetElementValueCallback callback) override;
  void SetElementChecked(int32_t backend_node_id,
                         bool checked,
                         bool send_events,
                         SetElementCheckedCallback callback) override;

 private:
  // content::RenderFrameObserver:
  void OnDestruct() override;

  void GetAnnotateDomModel(
      base::TimeDelta model_timeout,
      base::OnceCallback<
          void(mojom::ModelStatus, base::File, const std::string&)> callback);

  mojo::AssociatedRemote<mojom::AutofillAssistantDriver>& GetDriver();

  void OnGetModelFile(base::Time start_time,
                      blink::WebLocalFrame* frame,
                      int32_t role,
                      int32_t objective,
                      bool ignore_objective,
                      GetSemanticNodesCallback callback,
                      mojom::ModelStatus model_status,
                      base::File model,
                      const std::string& overrides_policy);

  void SetElementAttribute(int32_t backend_node_id,
                           const std::u16string& attribute_value,
                           const std::u16string& value,
                           bool send_events);

  mojo::AssociatedRemote<mojom::AutofillAssistantDriver> driver_;

  mojo::AssociatedReceiver<mojom::AutofillAssistantAgent> receiver_{this};
  base::WeakPtrFactory<AutofillAssistantAgent> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_AGENT_H_
