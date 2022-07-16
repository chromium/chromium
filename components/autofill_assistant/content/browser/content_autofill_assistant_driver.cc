// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/browser/content_autofill_assistant_driver.h"

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace autofill_assistant {

DOCUMENT_USER_DATA_KEY_IMPL(ContentAutofillAssistantDriver);

ContentAutofillAssistantDriver::ContentAutofillAssistantDriver(
    content::RenderFrameHost* render_frame_host)
    : content::DocumentUserData<ContentAutofillAssistantDriver>(
          render_frame_host) {}
ContentAutofillAssistantDriver::~ContentAutofillAssistantDriver() = default;

void ContentAutofillAssistantDriver::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillAssistantDriver>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

const mojo::AssociatedRemote<mojom::AutofillAssistantAgent>&
ContentAutofillAssistantDriver::GetAutofillAssistantAgent() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!autofill_assistant_agent_) {
    render_frame_host().GetRemoteAssociatedInterfaces()->GetInterface(
        &autofill_assistant_agent_);
  }

  return autofill_assistant_agent_;
}

}  // namespace autofill_assistant
