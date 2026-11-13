// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_agent.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/custom_form_control_util.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"

namespace autofill {

CustomFormControlAgent::CustomFormControlAgent(
    AutofillAgent* autofill_agent,
    int32_t world_id,
    blink::AssociatedInterfaceRegistry& registry)
    : autofill_agent_(CHECK_DEREF(autofill_agent)),
      executor_(autofill_agent_->unsafe_render_frame(), world_id) {
  // The registry belongs to the frame. The weak pointer drops requests that
  // arrive after this is destroyed.
  registry.AddInterface<mojom::CustomFormControlAgent>(base::BindRepeating(
      &CustomFormControlAgent::BindReceiver, weak_ptr_factory_.GetWeakPtr()));
}

CustomFormControlAgent::~CustomFormControlAgent() = default;

void CustomFormControlAgent::OnDestruct() {
  receiver_.reset();
}

void CustomFormControlAgent::LoadUnsafeScript(
    const blink::DocumentToken& document,
    const std::string& script,
    LoadUnsafeScriptCallback callback) {
  if (!IsCurrentDocument(document)) {
    std::move(callback).Run(false);
    return;
  }
  const bool success = executor_.LoadScript(script);
  if (success && base::FeatureList::IsEnabled(
                     features::kAutofillCustomFormControlExtraction)) {
    // `force=false` suffices because any newly detected custom form controls
    // change the extracted `FormData`, invalidating the `FormCache` entry for
    // affected forms.
    autofill_agent_->TriggerFormExtraction(/*force=*/false);
  }
  std::move(callback).Run(success);
}

void CustomFormControlAgent::DetectCustomFormControls(
    const blink::DocumentToken& document,
    DetectCustomFormControlsCallback callback) {
  if (!IsCurrentDocument(document)) {
    std::move(callback).Run({});
    return;
  }
  // If the script detaches the frame, no controls are detected.
  std::vector<CustomFormControl> controls =
      executor_.DetectCustomFormControls();
  // The script's result is untrusted, so only send controls that meet the
  // requirements of `mojom::DetectedCustomFormControl`.
  std::vector<mojom::DetectedCustomFormControlPtr> detected;
  base::flat_set<FieldRendererId> field_ids;
  for (const CustomFormControl& control : controls) {
    // Send at most `kMaxExtractableFields` controls.
    if (detected.size() >= kMaxExtractableFields) {
      break;
    }
    // Send each element at most once.
    const FieldRendererId field_id =
        form_util::GetFieldRendererId(control.canonical_dom_node);
    if (!field_ids.insert(field_id).second) {
      continue;
    }
    detected.push_back(mojom::DetectedCustomFormControl::New(
        field_id, base::UTF16ToUTF8(control.field_type), control.is_active));
  }
  std::move(callback).Run(std::move(detected));
}

void CustomFormControlAgent::ExtractOptions(
    const blink::DocumentToken& document,
    FieldRendererId field,
    ExtractOptionsCallback callback) {
  executor_.ExtractOptions(
      GetElement(document, field),
      base::BindOnce(
          [](ExtractOptionsCallback callback,
             std::optional<CustomFormControlOptions> options) {
            // The script's options are untrusted, so only send valid ones.
            if (!options || !IsValidOptionVector(options->options)) {
              std::move(callback).Run(nullptr);
              return;
            }
            std::move(callback).Run(
                mojom::ExtractedCustomFormControlOptions::New(
                    base::ToVector(options->options,
                                   [](const SelectOption& option) {
                                     return mojom::SelectOption::New(
                                         option.value, option.text);
                                   }),
                    options->popover_needs_to_be_closed));
          },
          std::move(callback)));
}

void CustomFormControlAgent::ClosePopover(const blink::DocumentToken& document,
                                          FieldRendererId field,
                                          ClosePopoverCallback callback) {
  executor_.ClosePopover(GetElement(document, field),
                         base::IgnoreArgs<bool>(std::move(callback)));
}

void CustomFormControlAgent::SetValue(const blink::DocumentToken& document,
                                      FieldRendererId field,
                                      mojom::SelectOptionPtr option,
                                      SetValueCallback callback) {
  executor_.SetValue(
      GetElement(document, field),
      SelectOption{.value = std::move(option->value),
                   .text = std::move(option->text)},
      base::IgnoreArgs<CustomFormControlSetValueResult>(std::move(callback)));
}

bool CustomFormControlAgent::IsCurrentDocument(
    const blink::DocumentToken& document) const {
  // The executor observes the frame, so this is null once the frame is
  // destroyed. Then calls fail, as they do in the executor.
  content::RenderFrame* render_frame = executor_.render_frame();
  return render_frame &&
         render_frame->GetWebFrame()->GetDocument().Token() == document;
}

blink::WebElement CustomFormControlAgent::GetElement(
    const blink::DocumentToken& document,
    FieldRendererId field) const {
  if (!field || !IsCurrentDocument(document)) {
    return blink::WebElement();
  }
  blink::WebElement element =
      blink::WebNode::FromDomNodeId(*field).DynamicTo<blink::WebElement>();
  // The renderer id may name an element of another document of the renderer,
  // or an element that isn't accessible to scripts (e.g. in a user agent
  // shadow tree).
  return element && element.GetDocument().Token() == document &&
                 form_util::IsAccessible(element)
             ? element
             : blink::WebElement();
}

void CustomFormControlAgent::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::CustomFormControlAgent>
        pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

}  // namespace autofill
