// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/renderer/autofill_assistant_agent.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/autofill_assistant/node_signals.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill_assistant {

AutofillAssistantAgent::AutofillAssistantAgent(
    content::RenderFrame* render_frame,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame) {
  registry->AddInterface(base::BindRepeating(
      &AutofillAssistantAgent::BindPendingReceiver, base::Unretained(this)));
}

// The destructor is not guaranteed to be called. Destruction happens (only)
// through the OnDestruct() event, which posts a task to delete this object.
// The process may be killed before this deletion can happen.
AutofillAssistantAgent::~AutofillAssistantAgent() = default;

void AutofillAssistantAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillAssistantAgent>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void AutofillAssistantAgent::OnDestruct() {
  delete this;
}

base::WeakPtr<AutofillAssistantAgent> AutofillAssistantAgent::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillAssistantAgent::GetSemanticNodes(
    int32_t role,
    int32_t objective,
    GetSemanticNodesCallback callback) {
  std::vector<NodeData> nodes;

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame) {
    std::move(callback).Run(nodes);
    return;
  }

  blink::WebVector<blink::AutofillAssistantNodeSignals> node_signals =
      blink::GetAutofillAssistantNodeSignals(frame->GetDocument());

  // TODO(sandromaggi): Run the model on the collected signals and filter
  // accordingly.

  for (const auto& node_signal : node_signals) {
    NodeData node_data;
    node_data.backend_node_id = node_signal.backend_node_id;
    nodes.push_back(node_data);
  }

  std::move(callback).Run(nodes);
}

void AutofillAssistantAgent::GetAnnotateDomModel(
    base::OnceCallback<void(base::File)> callback) {
  GetDriver()->GetAnnotateDomModel(std::move(callback));
}

const mojo::Remote<mojom::AutofillAssistantDriver>&
AutofillAssistantAgent::GetDriver() {
  if (!driver_) {
    render_frame()->GetBrowserInterfaceBroker()->GetInterface(
        driver_.BindNewPipeAndPassReceiver());
    return driver_;
  }

  // The driver_ can become unbound or disconnected in testing so this catches
  // that case and reconnects so `this` can connect to the driver in the
  // browser.
  if (driver_.is_bound() && driver_.is_connected()) {
    return driver_;
  }

  driver_.reset();
  render_frame()->GetBrowserInterfaceBroker()->GetInterface(
      driver_.BindNewPipeAndPassReceiver());
  return driver_;
}

}  // namespace autofill_assistant
