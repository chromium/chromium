// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_ROUTER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_ROUTER_H_

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/content/browser/form_forest.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

class AutofillableData;
class ContentAutofillDriver;

// ContentAutofillRouter routes events between ContentAutofillDriver objects in
// order to handle frame-transcending forms.
//
// A *frame-transcending* form is a form whose fields live in different frames.
// For example, credit card forms often have the credit card number field in an
// iframe hosted by a payment service provider.
//
// A frame-transcending form therefore consists of multiple *renderer forms*.
// ContentAutofillRouter *flattens* these forms into a single *browser form*,
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
// Let AutofillAgent-N, ContentAutofillRouter-N, and AutofillManager-N
// correspond to the Frame-N. ContentAutofillRouter would route an event
// concerning any of the forms in Frame-3 from ContentAutofillDriver-3 to
// ContentAutofillDriver-0:
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
//   Driver-3's ContentAutofillDriver::f(args...) calls
//   Router's   ContentAutofillRouter::f(this, args..., callback) calls
//   Driver-0's ContentAutofillDriver::callback(args...).
//
// Every function in ContentAutofillRouter takes a |source| parameter, which
// points to the ContentAutofillDriver that triggered the event. In events
// triggered by the renderer, the source driver is the driver the associated
// renderer form originates from.
//
// See ContentAutofillDriver for details on the naming pattern and an example.
//
// See FormForest for details on (un)flattening.
class ContentAutofillRouter {
 public:
  ContentAutofillRouter();
  ContentAutofillRouter(const ContentAutofillRouter&) = delete;
  ContentAutofillRouter& operator=(const ContentAutofillRouter&) = delete;
  ~ContentAutofillRouter();

  // Deletes all forms and fields related to |driver| (and this driver only).
  // Must be called whenever |driver| is destroyed.
  // As a simple performance optimization, if |driver| is a main frame, the
  // whole router is reset to the initial state.
  void UnregisterDriver(ContentAutofillDriver* driver);

  // Returns the ContentAutofillDriver* for which AskForValuesToFill() was
  // called last.
  // TODO(crbug.com/1224846) QueryFormFieldAutofill() was renamed to
  // AskForValuesToFill(), so we should rename last_queried_source() and
  // last_queried_source_ as well.
  ContentAutofillDriver* last_queried_source() const {
    return last_queried_source_;
  }

  // Registers the key-press handler with the driver that last called
  // AskForValuesToFill(), that is, |last_queried_source_|.
  void SetKeyPressHandler(
      ContentAutofillDriver* source,
      const content::RenderWidgetHost::KeyPressEventCallback& handler,
      void (*callback)(
          ContentAutofillDriver* target,
          const content::RenderWidgetHost::KeyPressEventCallback& handler));
  void UnsetKeyPressHandler(ContentAutofillDriver* source,
                            void (*callback)(ContentAutofillDriver* target));

  // Sets the suppress state in the driver that last called
  // AskForValuesToFill(), that is, |last_queried_source_|.
  void SetShouldSuppressKeyboard(ContentAutofillDriver* source,
                                 bool suppress,
                                 void (*callback)(ContentAutofillDriver* target,
                                                  bool suppress));

  // Routing of events called by the renderer:
  void SetFormToBeProbablySubmitted(
      ContentAutofillDriver* source,
      const absl::optional<FormData>& form,
      void (*callback)(ContentAutofillDriver* target,
                       const absl::optional<FormData>& form));
  void FormsSeen(
      ContentAutofillDriver* source,
      const std::vector<FormData>& updated_forms,
      const std::vector<FormGlobalId>& removed_forms,
      void (*callback)(ContentAutofillDriver* target,
                       const std::vector<FormData>& updated_forms,
                       const std::vector<FormGlobalId>& removed_forms));
  void FormSubmitted(
      ContentAutofillDriver* source,
      const FormData& form,
      bool known_success,
      mojom::SubmissionSource submission_source,
      void (*callback)(ContentAutofillDriver* target,
                       const FormData& form,
                       bool known_success,
                       mojom::SubmissionSource submission_source));
  void TextFieldDidChange(ContentAutofillDriver* source,
                          const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          base::TimeTicks timestamp,
                          void (*callback)(ContentAutofillDriver* target,
                                           const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box,
                                           base::TimeTicks timestamp));
  void TextFieldDidScroll(ContentAutofillDriver* source,
                          const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          void (*callback)(ContentAutofillDriver* target,
                                           const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box));
  void SelectControlDidChange(ContentAutofillDriver* source,
                              const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box,
                              void (*callback)(ContentAutofillDriver* target,
                                               const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box));
  void AskForValuesToFill(
      ContentAutofillDriver* source,
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutoselectFirstSuggestion autoselect_first_suggestion,
      FormElementWasClicked form_element_was_clicked,
      void (*callback)(ContentAutofillDriver* target,
                       const FormData& form,
                       const FormFieldData& field,
                       const gfx::RectF& bounding_box,
                       AutoselectFirstSuggestion autoselect_first_suggestion,
                       FormElementWasClicked form_element_was_clicked));
  void HidePopup(ContentAutofillDriver* source,
                 void (*callback)(ContentAutofillDriver* target));
  void FocusNoLongerOnForm(ContentAutofillDriver* source,
                           bool had_interacted_form,
                           void (*callback)(ContentAutofillDriver* target,
                                            bool had_interacted_form));
  void FocusOnFormField(ContentAutofillDriver* source,
                        const FormData& form,
                        const FormFieldData& field,
                        const gfx::RectF& bounding_box,
                        void (*callback)(ContentAutofillDriver* target,
                                         const FormData& form,
                                         const FormFieldData& field,
                                         const gfx::RectF& bounding_box));
  void DidFillAutofillFormData(ContentAutofillDriver* source,
                               const FormData& form,
                               base::TimeTicks timestamp,
                               void (*callback)(ContentAutofillDriver* target,
                                                const FormData& form,
                                                base::TimeTicks timestamp));
  void DidPreviewAutofillFormData(
      ContentAutofillDriver* source,
      void (*callback)(ContentAutofillDriver* target));
  void DidEndTextFieldEditing(ContentAutofillDriver* source,
                              void (*callback)(ContentAutofillDriver* target));
  void SelectFieldOptionsDidChange(
      ContentAutofillDriver* source,
      const FormData& form,
      void (*callback)(ContentAutofillDriver* target, const FormData& form));
  void JavaScriptChangedAutofilledValue(
      ContentAutofillDriver* source,
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value,
      void (*callback)(ContentAutofillDriver* target,
                       const FormData& form,
                       const FormFieldData& field,
                       const std::u16string& old_value));

  // Event called by Autofill Assistant as if it was called by the renderer.
  void FillFormForAssistant(ContentAutofillDriver* source,
                            const AutofillableData& fill_data,
                            const FormData& form,
                            const FormFieldData& field,
                            void (*callback)(ContentAutofillDriver* target,
                                             const AutofillableData& fill_data,
                                             const FormData& form,
                                             const FormFieldData& fiel));

  // Event called when the context menu is opened on a field.
  void OnContextMenuShownInField(
      ContentAutofillDriver* source,
      const FormGlobalId& form_global_id,
      const FieldGlobalId& field_global_id,
      void (*callback)(ContentAutofillDriver* target,
                       const FormGlobalId& form_global_id,
                       const FieldGlobalId& field_global_id));

  // Routing of events called by the browser:
  std::vector<FieldGlobalId> FillOrPreviewForm(
      ContentAutofillDriver* source,
      mojom::RendererFormDataAction action,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map,
      void (*callback)(ContentAutofillDriver* target,
                       mojom::RendererFormDataAction action,
                       const FormData& form));
  void SendAutofillTypePredictionsToRenderer(
      ContentAutofillDriver* source,
      const std::vector<FormDataPredictions>& type_predictions,
      void (*callback)(ContentAutofillDriver* target,
                       const std::vector<FormDataPredictions>& predictions));
  void SendFieldsEligibleForManualFillingToRenderer(
      ContentAutofillDriver* source,
      const std::vector<FieldGlobalId>& fields,
      void (*callback)(ContentAutofillDriver* target,
                       const std::vector<FieldRendererId>& fields));
  void RendererShouldAcceptDataListSuggestion(
      ContentAutofillDriver* source,
      const FieldGlobalId& field,
      const std::u16string& value,
      void (*callback)(ContentAutofillDriver* target,
                       const FieldRendererId& field,
                       const std::u16string& value));
  void RendererShouldClearFilledSection(
      ContentAutofillDriver* source,
      void (*callback)(ContentAutofillDriver* target));
  void RendererShouldClearPreviewedForm(
      ContentAutofillDriver* source,
      void (*callback)(ContentAutofillDriver* target));
  void RendererShouldFillFieldWithValue(
      ContentAutofillDriver* source,
      const FieldGlobalId& field,
      const std::u16string& value,
      void (*callback)(ContentAutofillDriver* target,
                       const FieldRendererId& field,
                       const std::u16string& value));
  void RendererShouldPreviewFieldWithValue(
      ContentAutofillDriver* source,
      const FieldGlobalId& field,
      const std::u16string& value,
      void (*callback)(ContentAutofillDriver* target,
                       const FieldRendererId& field,
                       const std::u16string& value));
  void RendererShouldSetSuggestionAvailability(
      ContentAutofillDriver* source,
      const FieldGlobalId& field,
      const mojom::AutofillState state,
      void (*callback)(ContentAutofillDriver* target,
                       const FieldRendererId& field,
                       const mojom::AutofillState state));

 private:
  friend class ContentAutofillRouterTestApi;

  // Returns the driver of |frame| stored in |form_forest_|.
  ContentAutofillDriver* DriverOfFrame(LocalFrameToken frame);

  // Calls ContentAutofillDriver::TriggerReparse() for all drivers in
  // |form_forest_| except for |exception|.
  void TriggerReparseExcept(ContentAutofillDriver* exception);

  // Update the last queried and source and do cleanup work.
  void SetLastQueriedSource(ContentAutofillDriver* source);
  void SetLastQueriedTarget(ContentAutofillDriver* target);

  // The URL of a main frame managed by the ContentAutofillRouter.
  // TODO(crbug.com/1240247): Remove.
  std::string MainUrlForDebugging() const;

  // The frame managed by the ContentAutofillRouter that was last passed to
  // an event.
  // TODO(crbug.com/1240247): Remove.
  content::GlobalRenderFrameHostId some_rfh_for_debugging_;

  // The forest of forms. See its documentation for the usage protocol.
  internal::FormForest form_forest_;

  // The driver that triggered the last AskForValuesToFill() call.
  // Update with SetLastQueriedSource().
  raw_ptr<ContentAutofillDriver> last_queried_source_ = nullptr;
  // The driver to which the last AskForValuesToFill() call was routed.
  // Update with SetLastQueriedTarget().
  raw_ptr<ContentAutofillDriver> last_queried_target_ = nullptr;

  // When the focus moves to a different frame, the order of the events
  // FocusNoLongerOnForm() and FocusOnFormField() may be reversed due to race
  // conditions. We use these members to correct the order of the events.
  LocalFrameToken focused_frame_;
  bool focus_no_longer_on_form_has_fired_ = true;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_ROUTER_H_
