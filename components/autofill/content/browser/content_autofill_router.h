// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_ROUTER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_ROUTER_H_

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
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

class ContentAutofillDriver;

// ContentAutofillRouter routes events between ContentAutofillDriver objects in
// order to handle frame-transcending forms. The precise definition follows, but
// essentially a frame-transcending form is a <form> which contain <iframe>s
// with form controls that logically belong to the <form>. For example in credit
// card forms, it is common that the credit card number is entered into an
// <iframe> that is hosted on a different server than the one that hosts the
// main page.
//
//
// Each form control element belongs to one (synthetic or real) form. The *host
// frame* of a form is the frame whose DOM contains the form.
// A form is a *child form* of its host frame.
//
// A form can contain nested <iframe>s. The *host form* of a frame is the
// (synthetic or real) form that contains the <iframe> that embeds the frame: if
// the <iframe> is a descendant of a <form>, that form is the host form;
// otherwise, the synthetic form is the host form.
// A frame is a *child frame* of its host form.
//
// The described host-to-child relationship induces a forest of trees whose
// nodes are forms and frames. Every tree with forms from different frames (<=>
// with at least two forms) is called a *frame-transcending form*.
//
// *Flattening* refers to the process of collapsing the non-root forms of such a
// tree into the root form. That is, flattening maps a hierarchy of forms to a
// single form. This is done by inserting the non-root forms' fields into the
// root form according to the cross-frame DOM order.
//
// The intention is that the browser shall only work with flattened forms. We
// refer to the forms of the form tree as the *renderer forms*, and to the
// flattened forms as *browser forms*. The inverse of flattening,
// *unflattening*, is the process of determining from a browser form the
// renderer forms that constitute that flattened form.
//
// For example, for the following pseudo HTML code,
//   <html>
//   <form id="form1">
//     <input id="field1">
//     <iframe id="frame1">
//       <input id="field2">
//     </iframe>
//     <iframe id="frame2">
//       <iframe id="frame3">
//         <form id="form2">
//           <input id="field3">
//         </form>
//         <form id="form3">
//           <input id="field4">
//         </form>
//       </iframe>
//     </iframe>
//     <input id="field5">
//   </form>
// the renderer forms will be in pseudo C++ code
//   FormData{  // form1 in main frame.
//     .name = "form1",
//     .fields = { "field1", "field5" },
//     .child_frames = { "frame1", "frame2" }
//   }
//   FormData{  // Synthetic form in frame1.
//     .fields = { "field2" },
//     .child_frames = { }
//   }
//   FormData{  // Synthetic form in frame2.
//     .fields = { },
//     .child_frames = { "frame3" }
//   }
//   FormData{  // form2 in frame3.
//     .fields = { "field3" },
//     .child_frames = { }
//   }
//   FormData{  // form3 in frame3.
//     .fields = { "field4" },
//     .child_frames = { }
//   }
// which by flattening are turned into one form
//   FormData{
//     .name = "form1",
//     .fields = { "field1", "field2", "field3", "field4", "field5" }
//   }
// Unflattening the flattened form produces the above renderer forms.
//
//
// ContentAutofillRouter's job is to
// 1. flatten renderer forms and/or unflatten flattened forms, and
// 2. route the communication between the renderer forms on the one hand and the
//    flattened form on the other hand.
//
// The routing is necessary because after flattening,
// 1. events coming from an AutofillAgent concerning a renderer form need to be
//    routed to the flattened form's AutofillManager, and
// 2. events coming from an AutofillManager concerning a flattened form need to
//    be routed to the AutofillAgents whose forms constitute the flattened form.
//
// ContentAutofillRouter carries out this routing at a ContentAutofillDriver
// level: each event in ContentAutofillDriver calls the identically-named
// function of ContentAutofillRouter, which then routes the call back to one or
// multiple ContentAutofillDrivers.
//
// For example, an event coming from AutofillAgent 1 might be routed from
// ContentAutofillDriver 1 to ContentAutofillDriver 2 and then be handled by
// AutofillManager 2:
//
//   +---Tab---+            +---Tab----+            +----Tab----+
//   | Agent 1 | ---------> | Driver 1 | -----+     | Manager 1 |
//   |         |            |          |      |     |           |
//   | Agent 2 |      +---> | Driver 2 | -----|---> | Manager 2 |
//   +---------+      |     +----------+      |     +-----------+
//                    |                       |
//                    |      +--Tab---+       |
//                    +----- | Router | <-----+
//                           +--------+
//
// See ContentAutofillDriver for details on the naming pattern and an example.
// See FormForest for details on (un)flattening.
class ContentAutofillRouter {
 public:
  ContentAutofillRouter();
  ContentAutofillRouter(const ContentAutofillRouter&) = delete;
  ContentAutofillRouter& operator=(const ContentAutofillRouter&) = delete;
  ~ContentAutofillRouter();

  // Deletes all forms and fields related to |driver| (and this driver only).
  // Should be called whenever |driver| is destroyed.
  void UnregisterDriver(ContentAutofillDriver* driver);

  void Reset() {}

  // Returns the ContentAutofillDriver* for which QueryFormFieldAutofill() was
  // called last.
  ContentAutofillDriver* last_queried_source() const { return nullptr; }

  void RegisterKeyPressHandler(
      ContentAutofillDriver* source_driver,
      const content::RenderWidgetHost::KeyPressEventCallback& handler);
  void RemoveKeyPressHandler(ContentAutofillDriver* source_driver);

  // Routing of events called by the renderer:
  void SetFormToBeProbablySubmitted(ContentAutofillDriver* source_driver,
                                    const absl::optional<FormData>& form);
  void FormsSeen(ContentAutofillDriver* source_driver,
                 const std::vector<FormData>& forms);
  void FormSubmitted(ContentAutofillDriver* source_driver,
                     const FormData& form,
                     bool known_success,
                     mojom::SubmissionSource submission_source);
  void TextFieldDidChange(ContentAutofillDriver* source_driver,
                          const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          base::TimeTicks timestamp);
  void TextFieldDidScroll(ContentAutofillDriver* source_driver,
                          const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box);
  void SelectControlDidChange(ContentAutofillDriver* source_driver,
                              const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box);
  void QueryFormFieldAutofill(ContentAutofillDriver* source_driver,
                              int32_t id,
                              const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box,
                              bool autoselect_first_suggestion);
  void HidePopup(ContentAutofillDriver* source_driver);
  void FocusNoLongerOnForm(ContentAutofillDriver* source_driver,
                           bool had_interacted_form);
  void FocusOnFormField(ContentAutofillDriver* source_driver,
                        const FormData& form,
                        const FormFieldData& field,
                        const gfx::RectF& bounding_box);
  void DidFillAutofillFormData(ContentAutofillDriver* source_driver,
                               const FormData& form,
                               base::TimeTicks timestamp);
  void DidPreviewAutofillFormData(ContentAutofillDriver* source_driver);
  void DidEndTextFieldEditing(ContentAutofillDriver* source_driver);
  void SelectFieldOptionsDidChange(ContentAutofillDriver* source_driver,
                                   const FormData& form);

  // Routing of events called by the browser:
  void SendFormDataToRenderer(
      ContentAutofillDriver* source_driver,
      int query_id,
      AutofillDriver::RendererFormDataAction action,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map);
  void SendAutofillTypePredictionsToRenderer(
      ContentAutofillDriver* source_driver,
      const std::vector<FormDataPredictions> type_predictions);
  void SendFieldsEligibleForManualFillingToRenderer(
      ContentAutofillDriver* source_driver,
      const std::vector<FieldGlobalId>& fields);
  void RendererShouldAcceptDataListSuggestion(
      ContentAutofillDriver* source_driver,
      const FieldGlobalId& field,
      const std::u16string& value);
  void RendererShouldClearFilledSection(ContentAutofillDriver* source_driver);
  void RendererShouldClearPreviewedForm(ContentAutofillDriver* source_driver);
  void RendererShouldFillFieldWithValue(ContentAutofillDriver* source_driver,
                                        const FieldGlobalId& field,
                                        const std::u16string& value);
  void RendererShouldPreviewFieldWithValue(ContentAutofillDriver* source_driver,
                                           const FieldGlobalId& field,
                                           const std::u16string& value);
  void RendererShouldSetSuggestionAvailability(
      ContentAutofillDriver* source_driver,
      const FieldGlobalId& field,
      const mojom::AutofillState state);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_ROUTER_H_
