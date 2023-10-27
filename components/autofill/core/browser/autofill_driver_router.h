// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_ROUTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_ROUTER_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_forest.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

// AutofillDriverRouter routes events between AutofillDriver objects in order to
// handle frame-transcending forms.
//
// A *frame-transcending* form is a form whose fields live in different frames.
// For example, credit card forms often have the credit card number field in an
// iframe hosted by a payment service provider.
//
// A frame-transcending form therefore consists of multiple *renderer forms*.
// AutofillDriverRouter *flattens* these forms into a single *browser form*,
// and maps all events concerning the renderer forms to that browser form, and
// vice versa.
//
// That way, the collection of renderer forms appears as one ordinary form to
// the browser.
//
// For example, consider the following pseudo HTML code:
//   <html>
//   <form id="Form-1">
//     <input id="Field-1">
//     <iframe id="Frame-1">
//       <input id="Field-2">
//     </iframe>
//     <iframe id="Frame-2">
//       <iframe id="Frame-3">
//         <form id="Form-2">
//           <input id="Field-3">
//         </form>
//         <form id="Form-3">
//           <input id="Field-4">
//         </form>
//       </iframe>
//     </iframe>
//     <input id="Field-5">
//   </form>
//
// Forms can be actual <form> elements or synthetic forms: <input>, <select>,
// and <iframe> elements that are not in the scope of any <form> belong to the
// enclosing frame's synthetic form.
//
// The five renderer forms are therefore, in pseudo C++ code:
//   FormData{
//     .host_frame = "Frame-0",  // The main frame.
//     .name = "Form-1",
//     .fields = { "Field-1", "Field-5" },
//     .child_frames = { "Frame-1", "Frame-2" }
//   }
//   FormData{
//     .host_frame = "Frame-1",
//     .name = "synthetic",
//     .fields = { "Field-2" },
//     .child_frames = { }
//   }
//   FormData{
//     .host_frame = "Frame-2",
//     .name = "synthetic",
//     .fields = { },
//     .child_frames = { "Frame-3" }
//   }
//   FormData{
//     .host_frame = "Frame-3",
//     .name = "Form-2",
//     .fields = { "Field-3" },
//     .child_frames = { }
//   }
//   FormData{
//     .host_frame = "Frame-3",
//     .name = "Form-3",
//     .fields = { "Field-4" },
//     .child_frames = { }
//   }
//
// The browser form of these renderer forms is obtained by flattening the fields
// into the root form:
//   FormData{
//     .name = "Form-1",
//     .fields = { "Field-1", "Field-2", "Field-3", "Field-4", "Field-5" }
//   }
//
// Let AutofillAgent-N, AutofillDriver-N, and AutofillManager-N correspond to
// the Frame-N. AutofillDriverRouter would route an event concerning any of the
// forms in Frame-3 from AutofillDriver-3 to AutofillDriver-0:
//
//   +---Tab---+            +---Tab----+            +----Tab----+
//   | Agent-0 |      +---> | Driver-0 | ---------> | Manager-0 |
//   |         |      |     |          |            |           |
//   | Agent-1 |      |     | Driver-1 |            | Manager-1 |
//   |         |      |     |          |            |           |
//   | Agent-2 |      |     | Driver-2 |            | Manager-2 |
//   |         |      |     |          |            |           |
//   | Agent-3 | -----|---> | Driver-3 | -----+     | Manager-3 |
//   +---------+      |     +----------+      |     +-----------+
//                    |                       |
//                    |      +--Tab---+       |
//                    +----- | Router | <-----+
//                           +--------+
//
// If the event name is `f`, the control flow is as follows:
//   Driver-3's AutofillDriver::f(args1...) calls
//   Router's   AutofillDriverRouter::f(this, args1..., callback) calls
//   Driver-0's AutofillDriver::callback(args2...),
// where `args2` are obtained by mapping renderer forms to browser forms and
// vice versa.
//
// Every function in AutofillDriverRouter takes a |source| parameter, which
// points to the AutofillDriver that triggered the event. In events triggered by
// the renderer, the source driver is the driver of the frame whose DOM contains
// the renderer form.
//
// See ContentAutofillDriver for details on the naming pattern and an example.
//
// See FormForest for details on (un)flattening.
class AutofillDriverRouter {
 public:
  AutofillDriverRouter();
  AutofillDriverRouter(const AutofillDriverRouter&) = delete;
  AutofillDriverRouter& operator=(const AutofillDriverRouter&) = delete;
  ~AutofillDriverRouter();

  // Deletes all forms and fields related to |driver| (and this driver only).
  // Must be called whenever |driver| is destroyed.
  //
  // |driver_is_dying| indicates if the |driver| is being destructed or about to
  // be destructed. Typically, the driver dies on cross-origin navigations but
  // survives same-origin navigations (but more precisely this depends on the
  // lifecycle of the content::RenderFrameHost). If the driver survives, the
  // router may keep the meta data is collected about the frame (in particular,
  // the parent frame).
  void UnregisterDriver(AutofillDriver* driver, bool driver_is_dying);

  // Events called by the renderer, passed to the browser:
  // Keep in alphabetic order.
  void FormsSeen(
      AutofillDriver* source,
      std::vector<FormData> updated_forms,
      const std::vector<FormGlobalId>& removed_forms,
      void (*callback)(AutofillDriver* target,
                       const std::vector<FormData>& updated_forms,
                       const std::vector<FormGlobalId>& removed_forms));
  void FormSubmitted(
      AutofillDriver* source,
      FormData form,
      bool known_success,
      mojom::SubmissionSource submission_source,
      void (*callback)(AutofillDriver* target,
                       const FormData& form,
                       bool known_success,
                       mojom::SubmissionSource submission_source));
  void TextFieldDidChange(AutofillDriver* source,
                          FormData form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          base::TimeTicks timestamp,
                          void (*callback)(AutofillDriver* target,
                                           const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box,
                                           base::TimeTicks timestamp));
  void TextFieldDidScroll(AutofillDriver* source,
                          FormData form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          void (*callback)(AutofillDriver* target,
                                           const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box));
  void SelectControlDidChange(AutofillDriver* source,
                              FormData form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box,
                              void (*callback)(AutofillDriver* target,
                                               const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box));
  void AskForValuesToFill(
      AutofillDriver* source,
      FormData form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutofillSuggestionTriggerSource trigger_source,
      void (*callback)(AutofillDriver* target,
                       const FormData& form,
                       const FormFieldData& field,
                       const gfx::RectF& bounding_box,
                       AutofillSuggestionTriggerSource trigger_source));
  // This event is broadcast to all drivers.
  void DidEndTextFieldEditing(AutofillDriver* source,
                              void (*callback)(AutofillDriver* target));
  void DidFillAutofillFormData(AutofillDriver* source,
                               FormData form,
                               base::TimeTicks timestamp,
                               void (*callback)(AutofillDriver* target,
                                                const FormData& form,
                                                base::TimeTicks timestamp));
  void FocusOnFormField(
      AutofillDriver* source,
      FormData form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      void (*callback)(AutofillDriver* target,
                       const FormData& form,
                       const FormFieldData& field,
                       const gfx::RectF& bounding_box),
      void (*focus_no_longer_on_form)(AutofillDriver* target));
  // This event is broadcast to all drivers.
  void FocusNoLongerOnForm(AutofillDriver* source,
                           bool had_interacted_form,
                           void (*callback)(AutofillDriver* target,
                                            bool had_interacted_form));
  // This event is broadcast to all drivers.
  void HidePopup(AutofillDriver* source,
                 void (*callback)(AutofillDriver* target));
  void JavaScriptChangedAutofilledValue(
      AutofillDriver* source,
      FormData form,
      const FormFieldData& field,
      const std::u16string& old_value,
      void (*callback)(AutofillDriver* target,
                       const FormData& form,
                       const FormFieldData& field,
                       const std::u16string& old_value));
  void SelectOrSelectListFieldOptionsDidChange(
      AutofillDriver* source,
      FormData form,
      void (*callback)(AutofillDriver* target, const FormData& form));
  // If `!form`, this event is just returned to `source`.
  void SetFormToBeProbablySubmitted(
      AutofillDriver* source,
      std::optional<FormData> form,
      void (*callback)(AutofillDriver* target, const FormData* optional_form));

  // Events called by the browser, passed to the renderer:
  // Keep in alphabetic order.
  std::vector<FieldGlobalId> ApplyFormAction(
      AutofillDriver* source,
      mojom::ActionType action_type,
      mojom::ActionPersistence action_persistence,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map,
      void (*callback)(AutofillDriver* target,
                       mojom::ActionType action_type,
                       mojom::ActionPersistence action_persistence,
                       const FormData& form));
  void ApplyFieldAction(
      AutofillDriver* source,
      mojom::ActionPersistence action_persistence,
      mojom::TextReplacement text_replacement,
      const FieldGlobalId& field,
      const std::u16string& value,
      void (*callback)(AutofillDriver* target,
                       mojom::ActionPersistence action_persistence,
                       mojom::TextReplacement text_replacement,
                       const FieldRendererId& field,
                       const std::u16string& value));
  using BrowserFormHandler = AutofillDriver::BrowserFormHandler;
  using RendererFormHandler =
      base::OnceCallback<void(const absl::optional<::autofill::FormData>&)>;
  // Routes both the request *and* the response: it calls `callback` with
  // - the `target` driver that shall extract the form,
  // - the `form_id` to be extracted, and
  // - the `renderer_form_handler` that processes the extracted form.
  //
  // Processing the extracted form means to call the callback
  // `browser_form_handler`. For that to happen, the extracted renderer form
  // must be mapped to a browser form. The `renderer_form_handler` performs this
  // conversion and then passes the result to `browser_form_handler` together
  // with the driver that manages the browser form (i.e., the driver whose
  // AutofillDriver::GetFrameToken() is equal to the browser form's
  // FormData::host_frame`).
  //
  // It is the responsibility of `callback` to call `renderer_form_handler`
  // with the extracted renderer form so that the mapping and callback can
  // happen.
  //
  // So intuitively, `callback` should do something like
  // `target->autofill_agent()->ExtractForm(form_id, renderer_form_handler)`.
  //
  // If routing the request fails, ExtractForm() calls `browser_form_handler`
  // right away with nullptr and std::nullopt.
  void ExtractForm(AutofillDriver* source,
                   FormGlobalId form_id,
                   BrowserFormHandler browser_form_handler,
                   void (*callback)(AutofillDriver* target,
                                    const FormRendererId& form_id,
                                    RendererFormHandler renderer_form_handler));
  void RendererShouldAcceptDataListSuggestion(
      AutofillDriver* source,
      const FieldGlobalId& field,
      const std::u16string& value,
      void (*callback)(AutofillDriver* target,
                       const FieldRendererId& field,
                       const std::u16string& value));
  void RendererShouldClearFilledSection(
      AutofillDriver* source,
      void (*callback)(AutofillDriver* target));
  void RendererShouldClearPreviewedForm(
      AutofillDriver* source,
      void (*callback)(AutofillDriver* target));
  void RendererShouldSetSuggestionAvailability(
      AutofillDriver* source,
      const FieldGlobalId& field,
      const mojom::AutofillState state,
      void (*callback)(AutofillDriver* target,
                       const FieldRendererId& field,
                       const mojom::AutofillState state));
  void RendererShouldTriggerSuggestions(
      AutofillDriver* source,
      const FieldGlobalId& field,
      AutofillSuggestionTriggerSource trigger_source,
      void (*callback)(AutofillDriver* target,
                       const FieldRendererId& field,
                       AutofillSuggestionTriggerSource trigger_source));
  void SendAutofillTypePredictionsToRenderer(
      AutofillDriver* source,
      const std::vector<FormDataPredictions>& type_predictions,
      void (*callback)(AutofillDriver* target,
                       const std::vector<FormDataPredictions>& predictions));
  void SendFieldsEligibleForManualFillingToRenderer(
      AutofillDriver* source,
      const std::vector<FieldGlobalId>& fields,
      void (*callback)(AutofillDriver* target,
                       const std::vector<FieldRendererId>& fields));

  // Event called by the browser, passed to the browser:
  void OnContextMenuShownInField(
      AutofillDriver* source,
      const FormGlobalId& form_global_id,
      const FieldGlobalId& field_global_id,
      void (*callback)(AutofillDriver* target,
                       const FormGlobalId& form_global_id,
                       const FieldGlobalId& field_global_id));

  // Returns the underlying renderer forms of `browser_form`.
  // Note that this function is intended for use outside of the `autofill`
  // component to ensure compatibility with callers whose concept of a form
  // does not include frame-transcending forms. It returns the constituent
  // renderer forms regardless of their frames' origins and the field types.
  std::vector<FormData> GetRendererForms(const FormData& browser_form) const;

 private:
  // Returns the driver of |frame| stored in |form_forest_|.
  // Does not invalidate any forms in the FormForest.
  AutofillDriver* DriverOfFrame(LocalFrameToken frame);

  // Calls AutofillDriver::TriggerFormExtractionInDriverFrame() for all
  // drivers in |form_forest_| except for |exception|.
  void TriggerFormExtractionExcept(AutofillDriver* exception);

  // The forest of forms. See its documentation for the usage protocol.
  internal::FormForest form_forest_;

  // When the focus moves to a different frame, the order of the events
  // FocusNoLongerOnForm() and FocusOnFormField() may be reversed due to race
  // conditions. We use these members to correct the order of the events.
  LocalFrameToken focused_frame_;
  bool focus_no_longer_on_form_has_fired_ = true;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_ROUTER_H_
