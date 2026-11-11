// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_AGENT_H_

#include <cstdint>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/renderer/custom_form_control_script_executor.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace blink {
class AssociatedInterfaceRegistry;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace autofill {

// Implements `mojom::CustomFormControlAgent` for a frame. The browser calls
// `LoadUnsafeScript()` to load the custom form control script of the frame's
// current document into a `CustomFormControlScriptExecutor`, which is owned by
// this `CustomFormControlAgent`.
//
// The frame's `AutofillAgent` owns the `CustomFormControlAgent`.
class CustomFormControlAgent : public mojom::CustomFormControlAgent {
 public:
  // Runs scripts in the isolated world `world_id` of `render_frame` (see
  // `CustomFormControlScriptExecutor`), and registers the interface in
  // `registry`, the frame's registry.
  CustomFormControlAgent(content::RenderFrame* render_frame,
                         int32_t world_id,
                         blink::AssociatedInterfaceRegistry& registry);
  CustomFormControlAgent(const CustomFormControlAgent&) = delete;
  CustomFormControlAgent& operator=(const CustomFormControlAgent&) = delete;
  ~CustomFormControlAgent() override;

  void OnDestruct();

  CustomFormControlScriptExecutor& executor() { return executor_; }

  // mojom::CustomFormControlAgent:
  void LoadUnsafeScript(const blink::DocumentToken& document,
                        const std::string& script,
                        LoadUnsafeScriptCallback callback) override;

 private:
  // Returns whether `document` is the frame's current document.
  //
  // The browser may send a call after it sent a navigation to the renderer,
  // but before it learned that the navigation committed. Then the call names
  // the previous document, and the call fails.
  bool IsCurrentDocument(const blink::DocumentToken& document) const;

  // Binds `pending_receiver`, replacing any previous connection.
  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::CustomFormControlAgent>
          pending_receiver);

  CustomFormControlScriptExecutor executor_;

  mojo::AssociatedReceiver<mojom::CustomFormControlAgent> receiver_{this};

  base::WeakPtrFactory<CustomFormControlAgent> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_AGENT_H_
