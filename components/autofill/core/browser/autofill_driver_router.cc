// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_driver_router.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/functional/invoke.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

namespace {

// Calls |fun| for all drivers in |form_forest|.
template <typename UnaryFunction>
void ForEachFrame(internal::FormForest& form_forest, UnaryFunction fun) {
  for (const std::unique_ptr<internal::FormForest::FrameData>& some_frame :
       form_forest.frame_datas()) {
    if (some_frame->driver) {
      base::invoke(fun, some_frame->driver);
    }
  }
}

}  // namespace

AutofillDriverRouter::AutofillDriverRouter() = default;
AutofillDriverRouter::~AutofillDriverRouter() = default;

AutofillDriver* AutofillDriverRouter::DriverOfFrame(LocalFrameToken frame) {
  const auto& frames = form_forest_.frame_datas();
  auto it = frames.find(frame);
  return it != frames.end() ? static_cast<AutofillDriver*>((*it)->driver.get())
                            : nullptr;
}

void AutofillDriverRouter::UnregisterDriver(AutofillDriver* driver,
                                            bool driver_is_dying) {
  CHECK(driver);
  for (const std::unique_ptr<internal::FormForest::FrameData>& frame :
       form_forest_.frame_datas()) {
    if (frame->driver == driver) {
      form_forest_.EraseFormsOfFrame(frame->frame_token,
                                     /*keep_frame=*/!driver_is_dying);
      break;
    }
  }
}

// Routing of events called by the renderer:

// Calls TriggerFormExtraction() on all ContentAutofillDrivers in |form_forest_|
// as well as their ancestor ContentAutofillDrivers.
//
// An ancestor might not be contained in the form tree known to FormForest: if
// the ancestor contained only invisible iframe(s) and no interesting fields, it
// would not be sent to the browser. In the meantime, these frames may have
// become visible. Therefore, we also call TriggerFormExtraction() in all
// ancestors.
//
// The typical use case is that some frame triggers form extractions on its own
// initiative and triggers an event. Then AutofillDriverRouter's event handler
// tells the other frames to form extraction, too, using
// TriggerFormExtractionExcept(source).
void AutofillDriverRouter::TriggerFormExtractionExcept(
    AutofillDriver* exception) {
  base::flat_set<AutofillDriver*> already_triggered;
  ForEachFrame(form_forest_, [&](AutofillDriver* driver) mutable {
    do {
      if (!already_triggered.insert(driver).second) {
        // An earlier invocation of this lambda has executed the rest of this
        // loop's body for `driver` and hence also for all its ancestors.
        break;
      }
      if (driver == exception) {
        continue;
      }
      driver->TriggerFormExtraction();
    } while ((driver = driver->GetParent()) != nullptr);
  });
}

void AutofillDriverRouter::FormsSeen(
    AutofillDriver* source,
    std::vector<FormData> renderer_forms,
    const std::vector<FormGlobalId>& removed_forms,
    void (*callback)(AutofillDriver* target,
                     const std::vector<FormData>& updated_forms,
                     const std::vector<FormGlobalId>& removed_forms)) {
  base::flat_set<FormGlobalId> forms_with_removed_fields =
      form_forest_.EraseForms(removed_forms);

  std::vector<FormGlobalId> renderer_form_ids;
  renderer_form_ids.reserve(renderer_forms.size());
  for (const FormData& renderer_form : renderer_forms) {
    renderer_form_ids.push_back(renderer_form.global_id());
  }

  for (FormData& form : std::move(renderer_forms)) {
    form_forest_.UpdateTreeOfRendererForm(std::move(form), source);
  }

  // Collects the browser forms of the |renderer_forms_ids|. If all forms in
  // |renderer_forms_ids| are root forms, each of them has a different browser
  // form. Otherwise, all forms in |renderer_forms_ids| are non-root forms in
  // the same tree, and |browser_forms| will contain the flattened root of this
  // tree.
  std::vector<FormData> browser_forms;
  browser_forms.reserve(renderer_form_ids.size());
  for (FormGlobalId renderer_form_id : renderer_form_ids) {
    const FormData& browser_form =
        form_forest_.GetBrowserForm(renderer_form_id);
    if (!base::Contains(browser_forms, browser_form.global_id(),
                        &FormData::global_id)) {
      browser_forms.push_back(browser_form);
    }
  }
  DCHECK(browser_forms.size() == renderer_form_ids.size() ||
         browser_forms.size() == 1);

  for (const FormGlobalId form_id : forms_with_removed_fields) {
    const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
    if (!base::Contains(browser_forms, browser_form.global_id(),
                        &FormData::global_id)) {
      browser_forms.push_back(browser_form);
    }
  }

  // Send the browser forms to the individual frames.
  if (!browser_forms.empty()) {
    LocalFrameToken frame = browser_forms.front().host_frame;
    DCHECK(base::ranges::all_of(browser_forms, [frame](const FormData& f) {
      return f.host_frame == frame;
    }));
    AutofillDriver* target = DriverOfFrame(frame);
    CHECK(target);
    callback(target, browser_forms, removed_forms);
  } else if (!removed_forms.empty()) {
    callback(source, {}, removed_forms);
  }
}

void AutofillDriverRouter::SetFormToBeProbablySubmitted(
    AutofillDriver* source,
    absl::optional<FormData> form,
    void (*callback)(AutofillDriver* target, const FormData* optional_form)) {
  if (!form) {
    callback(source, nullptr);
    return;
  }

  FormGlobalId form_id = form->global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form).value(), source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  CHECK(target);
  callback(target, &browser_form);
}

void AutofillDriverRouter::FormSubmitted(
    AutofillDriver* source,
    FormData form,
    bool known_success,
    mojom::SubmissionSource submission_source,
    void (*callback)(AutofillDriver* target,
                     const FormData& form,
                     bool known_success,
                     mojom::SubmissionSource submission_source)) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  CHECK(target);
  callback(target, browser_form, known_success, submission_source);
}

void AutofillDriverRouter::TextFieldDidChange(
    AutofillDriver* source,
    FormData form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    base::TimeTicks timestamp,
    void (*callback)(AutofillDriver* target,
                     const FormData& form,
                     const FormFieldData& field,
                     const gfx::RectF& bounding_box,
                     base::TimeTicks timestamp)) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  CHECK(target);
  callback(target, browser_form, field, bounding_box, timestamp);
}

void AutofillDriverRouter::TextFieldDidScroll(
    AutofillDriver* source,
    FormData form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    void (*callback)(AutofillDriver* target,
                     const FormData& form,
                     const FormFieldData& field,
                     const gfx::RectF& bounding_box)) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  CHECK(target);
  callback(target, browser_form, field, bounding_box);
}

void AutofillDriverRouter::SelectControlDidChange(
    AutofillDriver* source,
    FormData form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    void (*callback)(AutofillDriver* target,
                     const FormData& form,
                     const FormFieldData& field,
                     const gfx::RectF& bounding_box)) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  CHECK(target);
  callback(target, browser_form, field, bounding_box);
}

void AutofillDriverRouter::AskForValuesToFill(
    AutofillDriver* source,
    FormData form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    AutofillSuggestionTriggerSource trigger_source,
    void (*callback)(AutofillDriver* target,
                     const FormData& form,
                     const FormFieldData& field,
                     const gfx::RectF& bounding_box,
                     AutofillSuggestionTriggerSource trigger_source)) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  CHECK(target);
  callback(target, browser_form, field, bounding_box, trigger_source);
}

void AutofillDriverRouter::HidePopup(AutofillDriver* source,
                                     void (*callback)(AutofillDriver* target)) {
  ForEachFrame(form_forest_, callback);
}

void AutofillDriverRouter::FocusNoLongerOnForm(
    AutofillDriver* source,
    bool had_interacted_form,
    void (*callback)(AutofillDriver* target, bool had_interacted_form)) {
  // Suppresses FocusNoLongerOnForm() if the focus has already moved to a
  // different frame.
  if (focused_frame_ != source->GetFrameToken()) {
    return;
  }

  // Prevents FocusOnFormField() from calling FocusNoLongerOnForm().
  focus_no_longer_on_form_has_fired_ = true;

  TriggerFormExtractionExcept(source);

  // TODO(crbug/1228706): Retrofit event with the FormGlobalId and unicast
  // event.
  ForEachFrame(form_forest_, [&](AutofillDriver* some_driver) {
    callback(some_driver, had_interacted_form);
  });
}

void AutofillDriverRouter::FocusOnFormField(
    AutofillDriver* source,
    FormData form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    void (*callback)(AutofillDriver* target,
                     const FormData& form,
                     const FormFieldData& field,
                     const gfx::RectF& bounding_box),
    void (*focus_no_longer_on_form)(AutofillDriver* target)) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  // Calls FocusNoLongerOnForm() if the focus has already moved from a
  // different frame and FocusNoLongerOnForm() hasn't been called yet.
  if (focused_frame_ != source->GetFrameToken() &&
      !focus_no_longer_on_form_has_fired_) {
    ForEachFrame(form_forest_, focus_no_longer_on_form);
  }

  // Suppresses late FocusNoLongerOnForm().
  focused_frame_ = source->GetFrameToken();
  focus_no_longer_on_form_has_fired_ = false;

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  CHECK(target);
  callback(target, browser_form, field, bounding_box);
}

void AutofillDriverRouter::DidFillAutofillFormData(
    AutofillDriver* source,
    FormData form,
    base::TimeTicks timestamp,
    void (*callback)(AutofillDriver* target,
                     const FormData& form,
                     base::TimeTicks timestamp)) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  CHECK(target);
  callback(target, browser_form, timestamp);
}

void AutofillDriverRouter::DidEndTextFieldEditing(
    AutofillDriver* source,
    void (*callback)(AutofillDriver* target)) {
  TriggerFormExtractionExcept(source);

  // TODO(crbug/1228706): Retrofit event with the FormGlobalId and FieldGlobalId
  // and unicast event.
  ForEachFrame(form_forest_, callback);
}

void AutofillDriverRouter::SelectOrSelectListFieldOptionsDidChange(
    AutofillDriver* source,
    FormData form,
    void (*callback)(AutofillDriver* target, const FormData& form)) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  CHECK(target);
  callback(target, browser_form);
}

void AutofillDriverRouter::JavaScriptChangedAutofilledValue(
    AutofillDriver* source,
    FormData form,
    const FormFieldData& field,
    const std::u16string& old_value,
    void (*callback)(AutofillDriver* target,
                     const FormData& form,
                     const FormFieldData& field,
                     const std::u16string& old_value)) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  CHECK(target);
  callback(target, browser_form, field, old_value);
}

void AutofillDriverRouter::OnContextMenuShownInField(
    AutofillDriver* source,
    const FormGlobalId& form_global_id,
    const FieldGlobalId& field_global_id,
    void (*callback)(AutofillDriver* target,
                     const FormGlobalId& form_global_id,
                     const FieldGlobalId& field_global_id)) {
  TriggerFormExtractionExcept(source);

  ForEachFrame(form_forest_, [&](AutofillDriver* some_driver) {
    callback(some_driver, form_global_id, field_global_id);
  });
}

// Routing of events triggered by the browser.
//
// Below, `DriverOfFrame() == nullptr` does not necessarily indicate a bug and
// is therefore not NOTREACHED().
// The reason is that browser forms may be outdated and hence refer to frames
// that do not exist anymore.

std::vector<FieldGlobalId> AutofillDriverRouter::ApplyAutofillAction(
    AutofillDriver* source,
    mojom::AutofillActionType action_type,
    mojom::AutofillActionPersistence action_persistence,
    const FormData& data,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map,
    void (*callback)(AutofillDriver* target,
                     mojom::AutofillActionType action_type,
                     mojom::AutofillActionPersistence action_persistence,
                     const FormData& form)) {
  internal::FormForest::RendererForms renderer_forms =
      form_forest_.GetRendererFormsOfBrowserForm(
          data, {&triggered_origin, &field_type_map});
  for (const FormData& renderer_form : renderer_forms.renderer_forms) {
    if (auto* target = DriverOfFrame(renderer_form.host_frame)) {
      callback(target, action_type, action_persistence, renderer_form);
    }
  }
  return renderer_forms.safe_fields;
}

void AutofillDriverRouter::SendAutofillTypePredictionsToRenderer(
    AutofillDriver* source,
    const std::vector<FormDataPredictions>& browser_fdps,
    void (*callback)(AutofillDriver* target,
                     const std::vector<FormDataPredictions>& predictions)) {
  // Splits each FrameDataPredictions according to the respective FormData's
  // renderer forms, and groups these FormDataPredictions by the renderer form's
  // frame. We uso "fdp" as abbreviation of FormDataPredictions.
  std::map<LocalFrameToken, std::vector<FormDataPredictions>> renderer_fdps;
  for (const FormDataPredictions& browser_fdp : browser_fdps) {
    // Builds an index of the field predictions by the field's global ID.
    std::map<FieldGlobalId, FormFieldDataPredictions> field_predictions;
    DCHECK_EQ(browser_fdp.data.fields.size(), browser_fdp.fields.size());
    for (size_t i = 0; i < std::min(browser_fdp.data.fields.size(),
                                    browser_fdp.fields.size());
         ++i) {
      field_predictions.emplace(browser_fdp.data.fields[i].global_id(),
                                browser_fdp.fields[i]);
    }

    // Builds the FormDataPredictions of each renderer form and groups them by
    // the renderer form's frame in |renderer_fdps|.
    internal::FormForest::RendererForms renderer_forms =
        form_forest_.GetRendererFormsOfBrowserForm(
            browser_fdp.data,
            {&browser_fdp.data.main_frame_origin, /*field_type_map=*/nullptr});
    for (FormData& renderer_form : renderer_forms.renderer_forms) {
      LocalFrameToken frame = renderer_form.host_frame;
      FormDataPredictions renderer_fdp;
      renderer_fdp.data = std::move(renderer_form);
      renderer_fdp.signature = browser_fdp.signature;
      for (const FormFieldData& field : renderer_fdp.data.fields) {
        renderer_fdp.fields.push_back(
            std::move(field_predictions[field.global_id()]));
      }
      renderer_fdps[frame].push_back(std::move(renderer_fdp));
    }
  }

  // Sends the predictions of the renderer forms to the individual frames.
  for (const auto& p : renderer_fdps) {
    LocalFrameToken frame = p.first;
    const std::vector<FormDataPredictions>& renderer_fdp = p.second;
    if (auto* target = DriverOfFrame(frame)) {
      callback(target, renderer_fdp);
    }
  }
}

void AutofillDriverRouter::SendFieldsEligibleForManualFillingToRenderer(
    AutofillDriver* source,
    const std::vector<FieldGlobalId>& fields,
    void (*callback)(AutofillDriver* target,
                     const std::vector<FieldRendererId>& fields)) {
  // Splits FieldGlobalIds by their frames and reduce them to the
  // FieldRendererIds.
  std::map<LocalFrameToken, std::vector<FieldRendererId>> fields_by_frame;
  for (FieldGlobalId field : fields) {
    fields_by_frame[field.frame_token].push_back(field.renderer_id);
  }

  // Send the FieldRendererIds to the individual frames.
  for (const auto& p : fields_by_frame) {
    LocalFrameToken frame = p.first;
    const std::vector<FieldRendererId>& frame_fields = p.second;
    if (auto* target = DriverOfFrame(frame)) {
      callback(target, frame_fields);
    }
  }
}

void AutofillDriverRouter::RendererShouldAcceptDataListSuggestion(
    AutofillDriver* source,
    const FieldGlobalId& field,
    const std::u16string& value,
    void (*callback)(AutofillDriver* target,
                     const FieldRendererId& field,
                     const std::u16string& value)) {
  if (auto* target = DriverOfFrame(field.frame_token)) {
    callback(target, field.renderer_id, value);
  }
}

void AutofillDriverRouter::RendererShouldClearFilledSection(
    AutofillDriver* source,
    void (*callback)(AutofillDriver* target)) {
  ForEachFrame(form_forest_, callback);
}

void AutofillDriverRouter::RendererShouldClearPreviewedForm(
    AutofillDriver* source,
    void (*callback)(AutofillDriver* target)) {
  ForEachFrame(form_forest_, callback);
}

void AutofillDriverRouter::RendererShouldTriggerSuggestions(
    AutofillDriver* source,
    const FieldGlobalId& field,
    AutofillSuggestionTriggerSource trigger_source,
    void (*callback)(AutofillDriver* target,
                     const FieldRendererId& field,
                     AutofillSuggestionTriggerSource trigger_source)) {
  if (AutofillDriver* target = DriverOfFrame(field.frame_token)) {
    callback(target, field.renderer_id, trigger_source);
  }
}

void AutofillDriverRouter::RendererShouldFillFieldWithValue(
    AutofillDriver* source,
    const FieldGlobalId& field,
    const std::u16string& value,
    void (*callback)(AutofillDriver* target,
                     const FieldRendererId& field,
                     const std::u16string& value)) {
  if (auto* target = DriverOfFrame(field.frame_token)) {
    callback(target, field.renderer_id, value);
  }
}

void AutofillDriverRouter::RendererShouldPreviewFieldWithValue(
    AutofillDriver* source,
    const FieldGlobalId& field,
    const std::u16string& value,
    void (*callback)(AutofillDriver* target,
                     const FieldRendererId& field,
                     const std::u16string& value)) {
  if (auto* target = DriverOfFrame(field.frame_token)) {
    callback(target, field.renderer_id, value);
  }
}

void AutofillDriverRouter::RendererShouldSetSuggestionAvailability(
    AutofillDriver* source,
    const FieldGlobalId& field,
    const mojom::AutofillState state,
    void (*callback)(AutofillDriver* target,
                     const FieldRendererId& field,
                     const mojom::AutofillState state)) {
  if (auto* target = DriverOfFrame(field.frame_token)) {
    callback(target, field.renderer_id, state);
  }
}

std::vector<FormData> AutofillDriverRouter::GetRendererForms(
    const FormData& browser_form) const {
  return form_forest_
      .GetRendererFormsOfBrowserForm(
          browser_form,
          internal::FormForest::SecurityOptions::TrustAllOrigins())
      .renderer_forms;
}

}  // namespace autofill
