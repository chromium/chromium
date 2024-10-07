// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_

#include <limits>
#include <string>
#include <string_view>
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
namespace internal {
class FormForest;
}

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
// TODO(crbug.com/40195555): Currently, Autofill ignores unowned fields in
// shadow DOMs.
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
class FormData {
 public:
  struct FillData;
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
  // - some fields of FormFieldData (see FormFieldData::Equal()).
  static bool DeepEqual(const FormData& a, const FormData& b);

  FormData();
  FormData(const FormData&);
  FormData& operator=(const FormData&);
  FormData(FormData&&);
  FormData& operator=(FormData&&);
  ~FormData();

  // Uniquely identifies the DOM element that this form represents.
  //
  // It does *not* uniquely identify this FormData object (there is no such kind
  // of identifier because FormData is a value type).
  //
  // Must not be leaked to renderer process. See FormGlobalId for details.
  FormGlobalId global_id() const { return {host_frame(), renderer_id()}; }

  // TODO(crbug.com/40183094): This function is deprecated. Use
  // FormData::DeepEqual() instead. Returns true if two forms are the same, not
  // counting the values of the form elements.
  bool SameFormAs(const FormData& other) const;

  // Returns a pointer to the field if found, otherwise returns nullptr.
  // Note that FormFieldData::global_id() is not guaranteed to be unique among
  // FormData::fields.
  const FormFieldData* FindFieldByGlobalId(
      const FieldGlobalId& global_id) const;

  // Finds a field in the FormData by its name or id.
  // Returns a pointer to the field if found, otherwise returns nullptr.
  // TODO(crbug.com/40100455): Move to FormDataTestApi.
  FormFieldData* FindFieldByNameForTest(std::u16string_view name_or_id);

  // The id attribute of the form.
  const std::u16string& id_attribute() const { return id_attribute_; }
  void set_id_attribute(std::u16string id_attribute) {
    id_attribute_ = std::move(id_attribute);
  }

  // The name attribute of the form.
  const std::u16string& name_attribute() const { return name_attribute_; }
  void set_name_attribute(std::u16string name_attribute) {
    name_attribute_ = std::move(name_attribute);
  }

  // NOTE: Update `SameFormAs()` and `FormDataAndroid::SimilarFormAs()` if
  // needed when adding new a member.

  // The name by which autofill knows this form. This is generally either the
  // name attribute or the id_attribute value, which-ever is non-empty with
  // priority given to the name_attribute. This value is used when computing
  // form signatures.
  // TODO(crbug.com/40598703): remove this and use attributes/unique_id instead.
  const std::u16string& name() const { return name_; }
  void set_name(std::u16string name) { name_ = std::move(name); }

  // Titles of form's buttons.
  // Only populated in Password Manager.
  const ButtonTitleList& button_titles() const { return button_titles_; }
  void set_button_titles(ButtonTitleList button_titles) {
    button_titles_ = std::move(button_titles);
  }

  // The URL (minus query parameters and fragment) containing the form.
  // This value should not be sent via mojo.
  const GURL& url() const { return url_; }
  void set_url(GURL url) { url_ = std::move(url); }

  // The full URL, including query parameters and fragment.
  // This value should be set only for password forms.
  // This value should not be sent via mojo.
  const GURL& full_url() const { return full_url_; }
  void set_full_url(GURL full_url) { full_url_ = std::move(full_url); }

  // The action target of the form.
  const GURL& action() const { return action_; }
  void set_action(GURL action) { action_ = std::move(action); }

  // If the form in the DOM has an empty action attribute, the |action| field in
  // the FormData is set to the frame URL of the embedding document. This field
  // indicates whether the action attribute is empty in the form in the DOM.
  bool is_action_empty() const { return is_action_empty_; }
  void set_is_action_empty(bool is_action_empty) {
    is_action_empty_ = std::move(is_action_empty);
  }

  // The URL of main frame containing this form.
  // This value should not be sent via mojo.
  // |main_frame_origin| represents the main frame (not necessarily primary
  // main frame) of the form's frame tree as described by MPArch nested frame
  // trees. For details, see RenderFrameHost::GetMainFrame().
  const url::Origin& main_frame_origin() const { return main_frame_origin_; }
  void set_main_frame_origin(url::Origin main_frame_origin) {
    main_frame_origin_ = std::move(main_frame_origin);
  }

  // A unique identifier of the containing frame. This value is not serialized
  // because LocalFrameTokens must not be leaked to other renderer processes.
  // See LocalFrameToken for details.
  const LocalFrameToken& host_frame() const { return host_frame_; }
  void set_host_frame(LocalFrameToken host_frame) {
    host_frame_ = std::move(host_frame);
  }

  // Uniquely identifies the DOM element that this form represents among the
  // form DOM elements in the same frame.
  // In the browser process, use global_id() instead.
  // See global_id() for details.
  const FormRendererId& renderer_id() const { return renderer_id_; }
  void set_renderer_id(FormRendererId renderer_id) {
    renderer_id_ = std::move(renderer_id);
  }

  // A monotonically increasing counter that indicates the generation of the
  // form: if `f.version < g.version`, then `f` has been received from the
  // renderer before `g`. On non-iOS, the converse direction holds as well.
  //
  // This is intended only for AutofillManager's form cache as a workaround for
  // the cache-downdating problem.
  // TODO(crbug.com/40144964): Remove once FormData objects aren't stored
  // globally anymore.
  const FormVersion& version() const { return version_; }
  void set_version(FormVersion version) { version_ = std::move(version); }

  // A vector of all frames in the form, where currently 'frames' only refers
  // to iframes and not fenced frames. It can only be iframes because those are
  // the only frames with cross frame form filling.
  const std::vector<FrameTokenWithPredecessor>& child_frames() const {
    return child_frames_;
  }
  void set_child_frames(std::vector<FrameTokenWithPredecessor> child_frames) {
    child_frames_ = std::move(child_frames);
  }

  // The type of the event that was taken as an indication that this form is
  // being or has already been submitted. This field is filled only in Password
  // Manager for submitted password forms.
  mojom::SubmissionIndicatorEvent submission_event() const {
    return submission_event_;
  }
  void set_submission_event(mojom::SubmissionIndicatorEvent submission_event) {
    submission_event_ = std::move(submission_event);
  }

  // A vector of all the input fields in the form.
  //
  // WARNING: `fields` may contain duplicates:
  //
  // Usually, FormFieldData::global_id() (in the browser process) and
  // FormFieldData::renderer_id (in the renderer process) uniquely identify
  // objects in `fields`. This is reliable enough for practical purposes.
  //
  // Collisions are possible in rare cases. Two known scenarios are:
  // - The renderer is compromised and sends duplicates.
  // - In the DOM, a field F moves from a form G to a form H and the browser
  //   still knows an outdated version of G at the time it learns about the
  //   updated version of H. Then the browser has FormData representations of G
  //   and H which both include a FormFieldData representation of F. If G and H
  //   come from subframes, they're flattened into the same FormData, which then
  //   contains two representations of F; that is, FormData::fields contains two
  //   fields with the same FormFieldData::global_id().
  const std::vector<FormFieldData>& fields() const { return fields_; }
  void set_fields(std::vector<FormFieldData> new_fields) {
    fields_ = std::move(new_fields);
  }
  [[nodiscard]] std::vector<FormFieldData> ExtractFields() {
    return std::exchange(fields_, std::vector<FormFieldData>());
  }
  class MutableFieldsPassKey {
    constexpr MutableFieldsPassKey() = default;
    friend class FormDataAndroid;
    friend class internal::FormForest;
  };
  // Use `ExtractFields()` and `set_fields()` instead if possible.
  std::vector<FormFieldData>& mutable_fields(MutableFieldsPassKey pass_key) {
    return fields_;
  }

  // Contains unique renderer IDs of text elements which are predicted to be
  // usernames. The order matters: elements are sorted in descending likelihood
  // of being a username (the first one is the most likely username). Can
  // contain IDs of elements which are not in |fields|. This is only used during
  // parsing into PasswordForm, and hence not serialized for storage.
  const std::vector<FieldRendererId>& username_predictions() const {
    return username_predictions_;
  }
  void set_username_predictions(
      std::vector<FieldRendererId> username_predictions) {
    username_predictions_ = std::move(username_predictions);
  }

  // True if this is a Gaia form which should be skipped on saving.
  bool is_gaia_with_skip_save_password_form() const {
    return is_gaia_with_skip_save_password_form_;
  }
  void set_is_gaia_with_skip_save_password_form(
      bool is_gaia_with_skip_save_password_form) {
    is_gaia_with_skip_save_password_form_ =
        is_gaia_with_skip_save_password_form;
  }

  // Currently likely_contains_captcha_ is initialized only on Android platform.
  // For all other platforms its value is always `false`.
  bool likely_contains_captcha() const { return likely_contains_captcha_; }
  void set_likely_contains_captcha(bool likely_contains_captcha) {
    likely_contains_captcha_ = likely_contains_captcha;
  }

 private:
  friend class FormDataTestApi;

  std::u16string id_attribute_;
  std::u16string name_attribute_;
  std::u16string name_;
  ButtonTitleList button_titles_;
  GURL url_;
  GURL full_url_;
  GURL action_;
  bool is_action_empty_ = false;
  url::Origin main_frame_origin_;
  LocalFrameToken host_frame_;
  FormRendererId renderer_id_;
  FormVersion version_;
  std::vector<FrameTokenWithPredecessor> child_frames_;
  mojom::SubmissionIndicatorEvent submission_event_ =
      mojom::SubmissionIndicatorEvent::NONE;
  std::vector<FormFieldData> fields_;
  std::vector<FieldRendererId> username_predictions_;
  bool is_gaia_with_skip_save_password_form_ = false;
  bool likely_contains_captcha_ = false;
};

// Whether any of the fields in |form| is a non-empty password field.
bool FormHasNonEmptyPasswordField(const FormData& form);

// For testing.
std::ostream& operator<<(std::ostream& os, const FormData& form);

#if defined(UNIT_TEST)
inline bool operator==(const FormData& lhs, const FormData& rhs) {
  return FormData::DeepEqual(lhs, rhs);
}
#endif  // defined(UNIT_TEST)

// Serialize FormData. Used by the PasswordManager to persist FormData
// pertaining to password forms. Serialized data is appended to |pickle|.
void SerializeFormData(const FormData& form_data, base::Pickle* pickle);
// Deserialize FormData. This assumes that |iter| is currently pointing to
// the part of a pickle created by SerializeFormData. Returns true on success.
bool DeserializeFormData(base::PickleIterator* iter, FormData* form_data);

LogBuffer& operator<<(LogBuffer& buffer, const FormData& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_
