// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_router.h"

#include <algorithm>

#include "base/functional/invoke.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"

namespace autofill {

ContentAutofillRouter::ContentAutofillRouter() = default;
ContentAutofillRouter::~ContentAutofillRouter() = default;

void ContentAutofillRouter::UnregisterDriver(ContentAutofillDriver* driver) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes))
    return;
  NOTREACHED();
}

void ContentAutofillRouter::RegisterKeyPressHandler(
    ContentAutofillDriver* source,
    const content::RenderWidgetHost::KeyPressEventCallback& handler) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->RegisterKeyPressHandlerImpl(handler);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::RemoveKeyPressHandler(
    ContentAutofillDriver* source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->RemoveKeyPressHandlerImpl();
    return;
  }
  NOTREACHED();
}

// Routing of events called by the renderer:

void ContentAutofillRouter::FormsSeen(ContentAutofillDriver* source,
                                      const std::vector<FormData>& forms) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->FormsSeenImpl(forms);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::SetFormToBeProbablySubmitted(
    ContentAutofillDriver* source,
    const absl::optional<FormData>& form) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->SetFormToBeProbablySubmittedImpl(form);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::FormSubmitted(
    ContentAutofillDriver* source,
    const FormData& form,
    bool known_success,
    mojom::SubmissionSource submission_source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->FormSubmittedImpl(form, known_success, submission_source);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::TextFieldDidChange(ContentAutofillDriver* source,
                                               const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box,
                                               base::TimeTicks timestamp) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->TextFieldDidChangeImpl(form, field, bounding_box, timestamp);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::TextFieldDidScroll(ContentAutofillDriver* source,
                                               const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->TextFieldDidScrollImpl(form, field, bounding_box);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::SelectControlDidChange(
    ContentAutofillDriver* source,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->SelectControlDidChangeImpl(form, field, bounding_box);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::QueryFormFieldAutofill(
    ContentAutofillDriver* source,
    int32_t id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->QueryFormFieldAutofillImpl(id, form, field, bounding_box,
                                       autoselect_first_suggestion);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::HidePopup(ContentAutofillDriver* source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->HidePopupImpl();
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::FocusNoLongerOnForm(ContentAutofillDriver* source,
                                                bool had_interacted_form) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->FocusNoLongerOnFormImpl(had_interacted_form);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::FocusOnFormField(ContentAutofillDriver* source,
                                             const FormData& form,
                                             const FormFieldData& field,
                                             const gfx::RectF& bounding_box) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->FocusOnFormFieldImpl(form, field, bounding_box);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::DidFillAutofillFormData(
    ContentAutofillDriver* source,
    const FormData& form,
    base::TimeTicks timestamp) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->DidFillAutofillFormDataImpl(form, timestamp);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::DidPreviewAutofillFormData(
    ContentAutofillDriver* source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->DidPreviewAutofillFormDataImpl();
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::DidEndTextFieldEditing(
    ContentAutofillDriver* source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->DidEndTextFieldEditingImpl();
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::SelectFieldOptionsDidChange(
    ContentAutofillDriver* source,
    const FormData& form) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->SelectFieldOptionsDidChangeImpl(form);
    return;
  }
  NOTREACHED();
}

// Routing of events called by the browser.

void ContentAutofillRouter::SendFormDataToRenderer(
    ContentAutofillDriver* source,
    int query_id,
    AutofillDriver::RendererFormDataAction action,
    const FormData& data,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->SendFormDataToRendererImpl(query_id, action, data);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::SendAutofillTypePredictionsToRenderer(
    ContentAutofillDriver* source,
    std::vector<FormDataPredictions> type_predictions) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->SendAutofillTypePredictionsToRendererImpl(type_predictions);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::SendFieldsEligibleForManualFillingToRenderer(
    ContentAutofillDriver* source,
    const std::vector<FieldGlobalId>& fields) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    std::vector<FieldRendererId> renderer_ids;
    renderer_ids.reserve(renderer_ids.size());
    for (FieldGlobalId field : fields)
      renderer_ids.push_back(field.renderer_id);
    source->SendFieldsEligibleForManualFillingToRendererImpl(renderer_ids);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::RendererShouldAcceptDataListSuggestion(
    ContentAutofillDriver* source,
    const FieldGlobalId& field,
    const std::u16string& value) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->RendererShouldAcceptDataListSuggestionImpl(field, value);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::RendererShouldClearFilledSection(
    ContentAutofillDriver* source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->RendererShouldClearFilledSectionImpl();
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::RendererShouldClearPreviewedForm(
    ContentAutofillDriver* source) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->RendererShouldClearPreviewedFormImpl();
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::RendererShouldFillFieldWithValue(
    ContentAutofillDriver* source,
    const FieldGlobalId& field,
    const std::u16string& value) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->RendererShouldFillFieldWithValueImpl(field, value);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::RendererShouldPreviewFieldWithValue(
    ContentAutofillDriver* source,
    const FieldGlobalId& field,
    const std::u16string& value) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->RendererShouldPreviewFieldWithValueImpl(field, value);
    return;
  }
  NOTREACHED();
}

void ContentAutofillRouter::RendererShouldSetSuggestionAvailability(
    ContentAutofillDriver* source,
    const FieldGlobalId& field,
    const mojom::AutofillState state) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    source->RendererShouldSetSuggestionAvailabilityImpl(field, state);
    return;
  }
  NOTREACHED();
}

}  // namespace autofill
