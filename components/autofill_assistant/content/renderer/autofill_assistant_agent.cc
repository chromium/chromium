// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/renderer/autofill_assistant_agent.h"

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

}  // namespace autofill_assistant
