// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_MOCK_CUSTOM_FORM_CONTROL_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_MOCK_CUSTOM_FORM_CONTROL_AGENT_H_

#include <string>

#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace autofill {

// Mocks the renderer's `mojom::CustomFormControlAgent` of a frame. Tests set
// up the replies they need.
//
// To bind a frame's requests for the agent to the mock, e.g.:
//   rfh->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
//       mojom::CustomFormControlAgent::Name_,
//       base::BindRepeating(&MockCustomFormControlAgent::BindPendingReceiver,
//                           base::Unretained(&agent)));
class MockCustomFormControlAgent : public mojom::CustomFormControlAgent {
 public:
  MockCustomFormControlAgent();
  MockCustomFormControlAgent(const MockCustomFormControlAgent&) = delete;
  MockCustomFormControlAgent& operator=(const MockCustomFormControlAgent&) =
      delete;
  ~MockCustomFormControlAgent() override;

  // Binds a frame's request for the agent to this, so that the calls through
  // the frame's `mojo::AssociatedRemote` reach the mocked methods.
  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle);

  // mojom::CustomFormControlAgent:
  MOCK_METHOD(void,
              LoadUnsafeScript,
              (const blink::DocumentToken& document,
               const std::string& script,
               LoadUnsafeScriptCallback callback),
              (override));
  MOCK_METHOD(void,
              DetectCustomFormControls,
              (const blink::DocumentToken& document,
               DetectCustomFormControlsCallback callback),
              (override));

 private:
  mojo::AssociatedReceiverSet<mojom::CustomFormControlAgent> receivers_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_MOCK_CUSTOM_FORM_CONTROL_AGENT_H_
