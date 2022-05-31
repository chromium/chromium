// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_AUTOFILL_ASSISTANT_AGENT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_AUTOFILL_ASSISTANT_AGENT_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill_assistant/content/common/autofill_assistant_agent.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockAutofillAssistantAgent : public mojom::AutofillAssistantAgent {
 public:
  MockAutofillAssistantAgent();
  ~MockAutofillAssistantAgent() override;

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle);
  static void RegisterForAllFrames(content::WebContents* web_contents,
                                   MockAutofillAssistantAgent* agent);

  MOCK_METHOD(void,
              GetSemanticNodes,
              (int32_t role,
               int32_t objective,
               bool ignore_objective,
               base::TimeDelta model_timeout,
               base::OnceCallback<void(mojom::NodeDataStatus,
                                       const std::vector<NodeData>&)> callback),
              (override));
  MOCK_METHOD(void,
              SetElementValue,
              (int32_t backend_node_id,
               const std::u16string& value,
               bool send_events,
               base::OnceCallback<void(bool)> callback),
              (override));

 private:
  mojo::AssociatedReceiverSet<mojom::AutofillAssistantAgent> receivers_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_MOCK_AUTOFILL_ASSISTANT_AGENT_H_
