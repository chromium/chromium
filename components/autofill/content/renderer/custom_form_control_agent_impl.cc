// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_agent_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {

CustomFormControlAgentImpl::CustomFormControlAgentImpl(
    content::RenderFrame* render_frame,
    int32_t world_id,
    blink::AssociatedInterfaceRegistry& registry)
    : executor_(render_frame, world_id) {
  // The registry belongs to the frame. The weak pointer drops requests that
  // arrive after this is destroyed.
  registry.AddInterface<mojom::CustomFormControlAgent>(
      base::BindRepeating(&CustomFormControlAgentImpl::BindReceiver,
                          weak_ptr_factory_.GetWeakPtr()));
}

CustomFormControlAgentImpl::~CustomFormControlAgentImpl() = default;

void CustomFormControlAgentImpl::LoadUnsafeScript(
    const blink::DocumentToken& document,
    const std::string& script,
    LoadUnsafeScriptCallback callback) {
  if (!IsCurrentDocument(document)) {
    std::move(callback).Run(false);
    return;
  }
  // If the script detaches the frame, `this` may be destroyed during the call,
  // so only the local `callback` is used afterwards (the reply is dropped).
  std::move(callback).Run(executor_.LoadScript(script));
}

bool CustomFormControlAgentImpl::IsCurrentDocument(
    const blink::DocumentToken& document) const {
  // The executor observes the frame, so this is null once the frame is
  // destroyed. Then calls fail, as they do in the executor.
  content::RenderFrame* render_frame = executor_.render_frame();
  return render_frame &&
         render_frame->GetWebFrame()->GetDocument().Token() == document;
}

void CustomFormControlAgentImpl::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::CustomFormControlAgent>
        pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

}  // namespace autofill
