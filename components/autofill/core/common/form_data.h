// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

class LogBuffer;

// Pair of a button title (e.g. "Register") and its type (e.g.
// INPUT_ELEMENT_SUBMIT_TYPE).
using ButtonTitleInfo = std::pair<std::u16string, mojom::ButtonTitleType>;

// List of button titles of a given form.
using ButtonTitleList = std::vector<ButtonTitleInfo>;

using FormVersion = ::base::IdTypeU64<class FormVersionMarker>;

// Element of FormData::child_frames.
struct FrameTokenWithPredecessor {
  FrameTokenWithPredecessor();
  FrameTokenWithPredecessor(const FrameTokenWithPredecessor&);
  FrameTokenWithPredecessor(FrameTokenWithPredecessor&&);
  FrameTokenWithPredecessor& operator=(const FrameTokenWithPredecessor&);
  FrameTokenWithPredecessor& operator=(FrameTokenWithPredecessor&&);
  ~FrameTokenWithPredecessor();

  // An identifier of the child frame.
  FrameToken token;
  // This index represents which field, if any, precedes the frame in DOM order.
  // It shall be the maximum integer |i| such that the |i|th field precedes the
  // frame |token|. If there is no such field, it shall be -1.
  int predecessor = -1;

  friend bool operator==(const FrameTokenWithPredecessor& a,
                         const FrameTokenWithPredecessor& b) = default;
};

// Autofill represents forms and fields as FormData and FormFieldData objects.
//
// On the renderer side, there are roughly one-to-one correspondences
//  - between FormData and blink::WebFormElement, and
//  - between FormFieldData and blink::WebFormControlElement,
// where the Blink classes directly correspond to DOM elements.
//
// On the browser side, there are one-to-one correspondences
//  - between FormData and FormStructure, and
//  - between FormFieldData and AutofillField,
// where AutofillField and FormStructure hold additional information, such as
// Autofill type predictions and sectioning.
//
// A FormData is essentially a collection of FormFieldDatas with additional
// metadata.
//
// FormDatas and FormFieldDatas are used in the renderer-browser communication:
//  - The renderer passes a FormData and/or FormFieldData to the browser when it
//    has found a new form in the DOM, a form was submitted, etc. (see
//    mojom::AutofillDriver).
//  - The browser passes a FormData and/or FormFieldData to the renderer for
//    preview, filling, etc. (see mojom::AutofillAgent). In the preview and
//    filling cases, the browser sets the field values to the values to be
//    previewed or filled.
//
// There are a few exceptions to the aforementioned one-to-one correspondences
// between Autofill's data types and Blink's:
// - Autofill only supports certain types of WebFormControlElements: select,
//   textarea, and input elements whose type [4] is one of the following:
//   checkbox, email, month, number, password, radio, search, tel, text, url
//   (where values not listed in [4] default to "text", and "checkbox" and
//   "radio" inputs are currently not filled). In particular, form-associated
//   custom elements [3] are not supported.
// - Autofill has the concept of an unowned form, which does not correspond to
//   an existing blink::WebFormElement.
// - Autofill may move FormFieldDatas to other FormDatas across shadow/main
//   DOMs and across frames.
//
// In Blink, a field can, but does not have to be associated with a form. A
// field is *associated* with a form iff either:
// - it is a descendant of a <form> element, or
// - it has its "form" attribute set to the ID of a <form> element.
// Note that this association does not transcend DOMs. See [1] for details.
//
// In Autofill, we lift Blink's form association across DOMs. We say a field is
// *owned* by a form iff:
// - the field is associated with that form, or
// - the field is unassociated and the form is its nearest shadow-including
//   ancestor [2].
// So the difference between the two terms is that a field in a shadow DOM may
// be unassociated but owned (by a <form> in an ancestor DOM).
//
// Example:
// <body>
//   <form>
//     <input id=A>
//   </form>
//   <input id=B>
//   <form>
//     #shadow-root
//       <input id=C>
//   </form>
// </body>
// The input A is an associated and owned field.
// The input B is an unassociated and unowned field.
// The input C is an unassociated but an owned field.
//
// TODO(crbug.com/1243730): Currently, Autofill ignores unowned fields in shadow
// DOMs.
//
// The unowned fields of the frame constitute that frame's *unowned form*.
//
// Forms from different frames of the same WebContents may furthermore be
// merged. For details, see AutofillDriverRouter.
//
// clang-format off
// [1] https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#reset-the-form-owner
// [2] https://dom.spec.whatwg.org/#concept-shadow-including-descendant
// [3] https://html.spec.whatwg.org/multipage/custom-elements.html#custom-elements-face-example
// [4] https://html.spec.whatwg.org/multipage/input.html#attr-input-type
// clang-format on
struct FormData {
  // Returns true if many members of forms |a| and |b| are identical.
  //
  // "Many" is intended to be "all", but currently the following members are not
  // being compared:
  //
  // - FormData::button_titles,
  // - FormData::full_url,
  // - FormData::is_action_empty,
  // - FormData::main_frame_origin,
  // - FormData::host_frame,
  // - FormData::version,
  // - FormData::submission_event,
  // - FormData::username_predictions,
  // - FormData::is_gaia_with_skip_save_password_form,
  // - FormData::frame_id,
  // - some fields of FormFieldData (see FormFieldData::Equal()).
  static bool DeepEqual(const FormData& a, const FormData& b);

  FormData();
  FormData(const FormData&);
  FormData& operator=(const FormData&);
  FormData(FormData&&);
  FormData& operator=(FormData&&);
  ~FormData();

  // An identifier that is unique across all forms in all frames.
  // Must not be leaked to renderer process. See FieldGlobalId for details.
  FormGlobalId global_id() const { return {host_frame, unique_renderer_id}; }

  // TODO(crbug/1211834): This function is deprecated. Use FormData::DeepEqual()
  // instead.
  // Returns true if two forms are the same, not counting the values of the form
  // elements.
  bool SameFormAs(const FormData& other) const;

  // Returns a pointer to the field if found, otherwise returns nullptr.
  const FormFieldData* FindFieldByGlobalId(
      const FieldGlobalId& global_id) const;

  // Finds a field in the FormData by its name or id.
  // Returns a pointer to the field if found, otherwise returns nullptr.
  FormFieldData* FindFieldByName(const base::StringPiece16 name_or_id);

  // The id attribute of the form.
  std::u16string id_attribute;

  // The name attribute of the form.
  std::u16string name_attribute;

  // NOTE: Update `SameFormAs()` and `FormDataAndroid::SimilarFormAs()` if
  // needed when adding new a member.

  // The name by which autofill knows this form. This is generally either the
  // name attribute or the id_attribute value, which-ever is non-empty with
  // priority given to the name_attribute. This value is used when computing
  // form signatures.
  // TODO(crbug/896689): remove this and use attributes/unique_id instead.
  std::u16string name;

  // Titles of form's buttons.
  // Only populated in Password Manager.
  ButtonTitleList button_titles;

  // The URL (minus query parameters and fragment) containing the form.
  // This value should not be sent via mojo.
  GURL url;

  // The full URL, including query parameters and fragment.
  // This value should be set only for password forms.
  // This value should not be sent via mojo.
  GURL full_url;

  // The action target of the form.
  GURL action;

  // If the form in the DOM has an empty action attribute, the |action| field in
  // the FormData is set to the frame URL of the embedding document. This field
  // indicates whether the action attribute is empty in the form in the DOM.
  bool is_action_empty = false;

  // The URL of main frame containing this form.
  // This value should not be sent via mojo.
  // |main_frame_origin| represents the main frame (not necessarily primary
  // main frame) of the form's frame tree as described by MPArch nested frame
  // trees. For details, see RenderFrameHost::GetMainFrame().
  url::Origin main_frame_origin;

  // True if this form is a form tag.
  bool is_form_tag = true;

  // A unique identifier of the containing frame. This value is not serialized
  // because LocalFrameTokens must not be leaked to other renderer processes.
  LocalFrameToken host_frame;
  // An identifier of the form that is unique among the forms from the same
  // frame. In the browser process, it should only be used in conjunction with
  // |host_frame| to identify a field; see global_id(). It is not persistent
  // between page loads and therefore not used in comparison in SameFieldAs().
  FormRendererId unique_renderer_id;

  // A monotonically increasing counter that indicates the generation of the
  // form: if `f.version < g.version`, then `f` has been received from the
  // renderer before `g`. On non-iOS, the converse direction holds as well.
  //
  // This is intended only for AutofillManager's form cache as a workaround for
  // the cache-downdating problem.
  // TODO(crbug.com/1117028): Remove once FormData objects aren't stored
  // globally anymore.
  FormVersion version;

  // A vector of all frames in the form, where currently 'frames' only refers
  // to iframes and not fenced frames. It can only be iframes because those are
  // the only frames with cross frame form filling.
  std::vector<FrameTokenWithPredecessor> child_frames;

  // The type of the event that was taken as an indication that this form is
  // being or has already been submitted. This field is filled only in Password
  // Manager for submitted password forms.
  mojom::SubmissionIndicatorEvent submission_event =
      mojom::SubmissionIndicatorEvent::NONE;

  // A vector of all the input fields in the form.
  std::vector<FormFieldData> fields;

  // Contains unique renderer IDs of text elements which are predicted to be
  // usernames. The order matters: elements are sorted in descending likelihood
  // of being a username (the first one is the most likely username). Can
  // contain IDs of elements which are not in |fields|. This is only used during
  // parsing into PasswordForm, and hence not serialized for storage.
  std::vector<FieldRendererId> username_predictions;

  // True if this is a Gaia form which should be skipped on saving.
  bool is_gaia_with_skip_save_password_form = false;

#if BUILDFLAG(IS_IOS)
  std::string frame_id;
#endif
};

// Whether any of the fields in |form| is a non-empty password field.
bool FormHasNonEmptyPasswordField(const FormData& form);

// For testing.
std::ostream& operator<<(std::ostream& os, const FormData& form);

// Serialize FormData. Used by the PasswordManager to persist FormData
// pertaining to password forms. Serialized data is appended to |pickle|.
void SerializeFormData(const FormData& form_data, base::Pickle* pickle);
// Deserialize FormData. This assumes that |iter| is currently pointing to
// the part of a pickle created by SerializeFormData. Returns true on success.
bool DeserializeFormData(base::PickleIterator* iter, FormData* form_data);

LogBuffer& operator<<(LogBuffer& buffer, const FormData& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_
