// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/mock_autofill_assistant_agent.h"

#include "base/test/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace autofill_assistant {

MockAutofillAssistantAgent::MockAutofillAssistantAgent() = default;
MockAutofillAssistantAgent::~MockAutofillAssistantAgent() = default;

void MockAutofillAssistantAgent::BindPendingReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receivers_.Add(this,
                 mojo::PendingAssociatedReceiver<mojom::AutofillAssistantAgent>(
                     std::move(handle)));
}

// static
void MockAutofillAssistantAgent::RegisterForAllFrames(
    content::WebContents* web_contents,
    MockAutofillAssistantAgent* agent) {
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      base::BindLambdaForTesting([agent](content::RenderFrameHost* host) {
        host->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
            mojom::AutofillAssistantAgent::Name_,
            base::BindRepeating(
                &MockAutofillAssistantAgent::BindPendingReceiver,
                base::Unretained(agent)));
      }));
}

}  // namespace autofill_assistant
