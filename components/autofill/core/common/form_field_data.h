// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_FIELD_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_FIELD_DATA_H_

#include <stddef.h>

#include <compare>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/origin.h"

namespace base {
class Pickle;
class PickleIterator;
}  // namespace base

namespace autofill {

class LogBuffer;

// The flags describing form field properties.
enum FieldPropertiesFlags : uint32_t {
  kNoFlags = 0u,
  kUserTyped = 1u << 0,
  // kAutofilled means that at least one character of the field value comes from
  // being autofilled. This is different from
  // WebFormControlElement::IsAutofilled(). It is meant to be used for password
  // fields, to determine whether viewing the value needs user reauthentication.
  kAutofilledOnUserTrigger = 1u << 1,
  // The field received focus at any moment.
  kHadFocus = 1u << 2,
  // Use this flag, if some error occurred in flags processing.
  kErrorOccurred = 1u << 3,
  // On submission, the value of the field was recognised as a value which is
  // already stored.
  kKnownValue = 1u << 4,
  // A value was autofilled on pageload. This means that at least one character
  // of the field value comes from being autofilled.
  kAutofilledOnPageLoad = 1u << 5,
  // A value was autofilled on any of the triggers.
  kAutofilled = kAutofilledOnUserTrigger | kAutofilledOnPageLoad,
};

// Autofill supports assigning <label for=x> tags to inputs if x is id/name,
// or the id/name of a shadow host element containing the input.
// This enum is used to track how often each case occurs in practice.
enum class AssignedLabelSource {
  kId = 0,
  kName = 1,
  kShadowHostId = 2,
  kShadowHostName = 3,
  kMaxValue = kShadowHostName,
};

// FieldPropertiesMask is used to contain combinations of FieldPropertiesFlags
// values.
using FieldPropertiesMask = std::underlying_type_t<FieldPropertiesFlags>;

// HTML                                      | value  | text
// ------------------------------------------+--------+------
// <option value=Foo label=Bar>Baz</option>  | "Foo"  | "Bar"
// <option value=Foo>Bar</option>            | "Foo"  | "Bar"
// <option label=Bar>Foo</option>            | "Foo"  | "Bar"
// <option>Foo</option>                      | "Foo"  | "Foo"
// <option value=Foo></option>               | "Foo"  | ""
// <option label=Bar></option>               | ""     | "Bar"
struct SelectOption {
  friend bool operator==(const SelectOption& lhs,
                         const SelectOption& rhs) = default;

  // The option's "value" attribute, or, if not present, its text content.
  std::u16string value;
  // The option's "label" attribute, or, if not present, its text content.
  std::u16string text;
};

// Stores information about the section of the field.
class Section {
 public:
  struct Autocomplete {
    friend auto operator<=>(const Autocomplete& lhs,
                            const Autocomplete& rhs) = default;
    friend bool operator==(const Autocomplete& lhs,
                           const Autocomplete& rhs) = default;

    std::string section;
    HtmlFieldMode mode = HtmlFieldMode::kNone;
  };

  using Default = absl::monostate;

  struct FieldIdentifier {
    FieldIdentifier() = default;
    FieldIdentifier(std::string field_name,
                    size_t local_frame_id,
                    FieldRendererId field_renderer_id)
        : field_name(std::move(field_name)),
          local_frame_id(local_frame_id),
          field_renderer_id(field_renderer_id) {}

    friend auto operator<=>(const FieldIdentifier& lhs,
                            const FieldIdentifier& rhs) = default;
    friend bool operator==(const FieldIdentifier& lhs,
                           const FieldIdentifier& rhs) = default;

    std::string field_name;
    size_t local_frame_id;
    FieldRendererId field_renderer_id;
  };

  static Section FromAutocomplete(Autocomplete autocomplete);
  static Section FromFieldIdentifier(
      const FormFieldData& field,
      base::flat_map<LocalFrameToken, size_t>& frame_token_ids);

  Section();
  Section(const Section& section);
  ~Section();

  // `absl::variant` does not implement `operator<=>` - therefore the ordering
  // needs to be specified manually. Once `absl::variant` is `std::variant`,
  // this return type can become `auto`.
  friend std::strong_ordering operator<=>(const Section& lhs,
                                          const Section& rhs) = default;
  friend bool operator==(const Section& lhs, const Section& rhs) = default;
  explicit operator bool() const;

  bool is_from_autocomplete() const;
  bool is_from_fieldidentifier() const;
  bool is_default() const;

  // Reconstructs `this` to a string. The string representation of the section
  // is used in the renderer.
  // TODO(crbug.com/40200532): Remove when fixed.
  std::string ToString() const;

 private:
  // Represents the section's origin:
  //  - `Default` is the empty, initial value before running any sectioning
  //     algorithm,
  //  - `Autocomplete` represents a section derived from the autocomplete
  //     attribute,
  //  - `FieldIdentifier` represents a section generated based on the first
  //     field in the section.
  using SectionValue = absl::variant<Default, Autocomplete, FieldIdentifier>;

  friend struct mojo::StructTraits<autofill::mojom::SectionDataView,
                                   autofill::Section>;
  friend struct mojo::UnionTraits<autofill::mojom::SectionValueDataView,
                                  autofill::Section::SectionValue>;

  SectionValue value_;
};

LogBuffer& operator<<(LogBuffer& buffer, const Section& section);
std::ostream& operator<<(std::ostream& os, const Section& section);

using FormControlType = mojom::FormControlType;

LogBuffer& operator<<(LogBuffer& buffer, FormControlType type);

// Stores information about a field in a form. Read more about forms and fields
// at FormData.
class FormFieldData {
 public:
  using CheckStatus = mojom::FormFieldData_CheckStatus;
  using RoleAttribute = mojom::FormFieldData_RoleAttribute;
  using LabelSource = mojom::FormFieldData_LabelSource;

  struct FillData;

  // Returns true if many members of fields |a| and |b| are identical.
  //
  // "Many" is intended to be "all", but currently the following members are not
  // being compared:
  //
  // - FormFieldData::value,
  // - FormFieldData::aria_label,
  // - FormFieldData::aria_description,
  // - FormFieldData::host_frame,
  // - FormFieldData::host_form_id,
  // - FormFieldData::host_form_signature,
  // - FormFieldData::origin,
  // - FormFieldData::force_override,
  // - FormFieldData::form_control_ax_id,
  // - FormFieldData::section,
  // - FormFieldData::is_autofilled,
  // - FormFieldData::is_user_edited,
  // - FormFieldData::properties_mask,
  // - FormFieldData::is_enabled,
  // - FormFieldData::is_readonly,
  // - FormFieldData::user_input,
  // - FormFieldData::options,
  // - FormFieldData::label_source,
  // - FormFieldData::bounds,
  // - FormFieldData::datalist_options.
  static bool DeepEqual(const FormFieldData& a, const FormFieldData& b);

  FormFieldData();
  FormFieldData(const FormFieldData&);
  FormFieldData& operator=(const FormFieldData&);
  FormFieldData(FormFieldData&&);
  FormFieldData& operator=(FormFieldData&&);
  ~FormFieldData();

  // Uniquely identifies the DOM element that this field represents.
  //
  // It does *not* uniquely identify this FormFieldData object (there is no such
  // kind of identifier because FormFieldData is a value type). In particular,
  // they're not guaranteed to be unique FormData::fields; see FormData::fields
  // for details.
  //
  // Must not be leaked to renderer process. See FieldGlobalId for details.
  FieldGlobalId global_id() const { return {host_frame(), renderer_id()}; }

  // An identifier of the renderer form that contained this field.
  // This may be different from the browser form that contains this field in the
  // case of a frame-transcending form. See AutofillDriverRouter and
  // internal::FormForest for details on the distinction between renderer and
  // browser forms.
  FormGlobalId renderer_form_id() const {
    return {host_frame(), host_form_id()};
  }

  // TODO(crbug.com/40183094): This function is deprecated. Use
  // FormFieldData::DeepEqual() instead.
  bool SameFieldAs(const FormFieldData& field) const;

  // Returns true for all of textfield-looking types: text, password,
  // search, email, url, and number. It must work the same way as Blink function
  // WebInputElement::IsTextField(), and it returns false if |*this| represents
  // a textarea.
  bool IsTextInputElement() const;

  bool IsPasswordInputElement() const;

  // <select> gets special handling when it comes to unfocusable fields. The
  // motivation for this exception is that synthetic select fields often come
  // with an unfocusable <select> element.
  //
  // A synthetic select field is a combination of JavaScript-controlled DOM
  // elements that provide a list of options. They're frequently associated with
  // hidden (i.e., unfocusable) <select> element. JavaScript keeps the selected
  // option in sync with the visible DOM elements of the select field. To
  // support synthetic select fields, Autofill intentionally fills unfocusable
  // <select> elements.
  bool IsSelectElement() const;

  // Returns true if the field is focusable to the user.
  // This is an approximation of visibility with false positives.
  bool IsFocusable() const {
    return is_focusable() && role() != RoleAttribute::kPresentation;
  }

  bool DidUserType() const;
  bool HadFocus() const;
  bool WasPasswordAutofilled() const;

  // NOTE: Update `SameFieldAs()` and `FormFieldDataAndroid::SimilarFieldAs()`
  // if needed when adding new a member.

  // The name by which autofill knows this field. This is generally either the
  // name attribute or the id_attribute value, which-ever is non-empty with
  // priority given to the name_attribute. This value is used when computing
  // form signatures.
  // TODO(crbug.com/40598703): remove this and use attributes/unique_id instead.
  const std::u16string& name() const { return name_; }
  void set_name(std::u16string name) { name_ = std::move(name); }

  const std::u16string& id_attribute() const { return id_attribute_; }
  void set_id_attribute(std::u16string id_attribute) {
    id_attribute_ = std::move(id_attribute);
  }
  const std::u16string& name_attribute() const { return name_attribute_; }
  void set_name_attribute(std::u16string name_attribute) {
    name_attribute_ = std::move(name_attribute);
  }
  const std::u16string& label() const { return label_; }
  void set_label(std::u16string label) { label_ = std::move(label); }

  // The form control element's value (i.e., the value of their IDL attribute
  // "value") or the contenteditable's text content, depending on the
  // FormFieldData::form_control_type().
  //
  // To get a field's initial value or the value for submission, see
  // AutofillField::value() and AutofillField::value_for_import().
  //
  // A note on FormFieldData objects of type FormControlType::kSelect*, i.e.,
  // <select> elements:
  //
  //   <select> elements have an associated list <option> elements, each of
  //   which has a value and a text. The idea is that the value serves technical
  //   purposes, while the text is visible to the user.
  //
  //   FormFieldData::value() is the value of the selected <option>, if any, or
  //   the empty string. See SelectOption for details on how the value of an
  //   <option> is determined.
  //
  //   FormFieldData::value() may not be the ideal human-readable representation
  //   of a <select> element. The selected option's text is usually the better
  //   string to display to the user (e.g., during form import). For further
  //   details, see SelectOption and FormFieldData::selected_option().
  //
  // Truncated at `kMaxStringLength`.
  // TODO(crbug.com/40941640): Extract the value of contenteditables on iOS.
  const std::u16string& value() const { return value_; }
  void set_value(std::u16string value) { value_ = std::move(value); }

  // Returns the (first) selected option. Returns std::nullopt if none is found.
  // The only field types that come with options are FormControlType::kSelect*
  // and FormControlType::kInput* with a datalist. But even their `value()` may
  // mismatch all `options()`, e.g., when JavaScript set the value to a
  // different value or when the number or string length of the options exceeded
  // limits during extraction.
  base::optional_ref<const SelectOption> selected_option() const;

  // The selected text, or the empty string if no text is selected.
  // Truncated at `50 * kMaxStringLength`.
  // This is not necessarily a substring of `value` because both strings are
  // truncated, and because for rich-text contenteditables the selection and
  // text content differ in whitespace.
  // TODO(crbug.com/40941640): Extract on iOS.
  const std::u16string& selected_text() const { return selected_text_; }
  void set_selected_text(std::u16string selected_text) {
    selected_text_ = std::move(selected_text);
  }

  FormControlType form_control_type() const { return form_control_type_; }
  void set_form_control_type(FormControlType form_control_type) {
    form_control_type_ = form_control_type;
  }
  const std::string& autocomplete_attribute() const {
    return autocomplete_attribute_;
  }
  void set_autocomplete_attribute(std::string autocomplete_attribute) {
    autocomplete_attribute_ = std::move(autocomplete_attribute);
  }
  const std::optional<AutocompleteParsingResult>& parsed_autocomplete() const {
    return parsed_autocomplete_;
  }
  void set_parsed_autocomplete(
      std::optional<AutocompleteParsingResult> parsed_autocomplete) {
    parsed_autocomplete_ = std::move(parsed_autocomplete);
  }
  const std::u16string& placeholder() const { return placeholder_; }
  void set_placeholder(std::u16string placeholder) {
    placeholder_ = std::move(placeholder);
  }
  const std::u16string& css_classes() const { return css_classes_; }
  void set_css_classes(std::u16string css_classes) {
    css_classes_ = std::move(css_classes);
  }
  const std::u16string& aria_label() const { return aria_label_; }
  void set_aria_label(std::u16string aria_label) {
    aria_label_ = std::move(aria_label);
  }
  const std::u16string& aria_description() const { return aria_description_; }
  void set_aria_description(std::u16string aria_description) {
    aria_description_ = std::move(aria_description);
  }

  // A unique identifier of the containing frame. This value is not serialized
  // because LocalFrameTokens must not be leaked to other renderer processes.
  // It is not persistent between page loads and therefore not used in
  // comparison in SameFieldAs().
  const LocalFrameToken& host_frame() const { return host_frame_; }
  void set_host_frame(LocalFrameToken host_frame) {
    host_frame_ = std::move(host_frame);
  }

  // Uniquely identifies the DOM element that this field represents among the
  // field DOM elements in the same frame.
  // In the browser process, use global_id() instead.
  // See global_id() for details on the properties and pitfalls.
  FieldRendererId renderer_id() const { return renderer_id_; }
  void set_renderer_id(FieldRendererId renderer_id) {
    renderer_id_ = renderer_id;
  }

  // Renderer ID of the owning form in the same frame.
  FormRendererId host_form_id() const { return host_form_id_; }
  void set_host_form_id(FormRendererId host_form_id) {
    host_form_id_ = host_form_id;
  }

  // The signature of the field's renderer form, that is, the signature of the
  // FormData that contained this field when it was received by the
  // AutofillDriver (see AutofillDriverRouter and internal::FormForest
  // for details on the distinction between renderer and browser forms).
  // Currently, the value is only set in ContentAutofillDriver; it's null on iOS
  // and in the Password Manager.
  // This value is written and read only in the browser for voting of
  // cross-frame forms purposes. It is therefore not sent via mojo.
  FormSignature host_form_signature() const { return host_form_signature_; }
  void set_host_form_signature(FormSignature host_form_signature) {
    host_form_signature_ = host_form_signature;
  }

  // The origin of the frame that hosts the field.
  const url::Origin& origin() const { return origin_; }
  void set_origin(url::Origin origin) { origin_ = std::move(origin); }

  // The ax node id of the form control in the accessibility tree.
  int32_t form_control_ax_id() const { return form_control_ax_id_; }
  void set_form_control_ax_id(int32_t form_control_ax_id) {
    form_control_ax_id_ = form_control_ax_id;
  }

  // The unique identifier of the section (e.g. billing vs. shipping address)
  // of this field.
  const Section& section() const { return section_; }
  void set_section(Section section) { section_ = std::move(section); }

  // The default value for text fields that have no maxlength attribute
  // specified. We choose the maximum 32 bit, rather than 64 bit, number because
  // so we don't need to worry about integer overflows when doing arithmetic
  // with FormFieldData::max_length.
  static constexpr size_t kDefaultMaxLength =
      std::numeric_limits<uint32_t>::max();

  // The maximum length of the FormFieldData::value as specified in the DOM. For
  // fields that do not support free text input (e.g., <select> and <input
  // type=month>), this is 0. For other fields (e.g., <input type=text>), this
  // is `kDefaultMaxLength`, which means we don't need to worry about integer
  // overflows when doing arithmetic with FormFieldData::max_length.
  //
  // Changes to the default value also must be reflected in
  // form_autofill_util.cc's GetMaxLength() and
  // FormFieldData::has_no_max_length().
  //
  // We use uint64_t instead of size_t because this struct is sent over IPC
  // which could span 32 & 64 bit processes. We chose uint64_t instead of
  // uint32_t to maintain compatibility with old code which used size_t
  // (base::Pickle used to serialize that as 64 bit).
  uint64_t max_length() const { return max_length_; }
  void set_max_length(uint64_t max_length) { max_length_ = max_length; }

  bool is_autofilled() const { return is_autofilled_; }
  void set_is_autofilled(bool is_autofilled) { is_autofilled_ = is_autofilled; }

  // Whether the user has edited this field since page load or resetting the
  // field.
  //
  // Examples that count as edits:
  // - Typing into a text control.
  // - Pasting into a text control.
  // - Clicking and selecting an option of a <select> counts.
  // - Unfocusing a <select> using TAB (because of the keydown event).
  //
  // Examples that do not count as edits:
  // - Autofill.
  // - Typing into a contenteditable.
  // - Setting the field's value directly in JavaScript.
  // - Untrusted events (see JavaScript's Event.isTrusted).
  //
  // The property is sticky: a user-edited field becomes non-user-edited only
  // when the form is reset (JavaScript's HTMLFormElement.reset()).
  // TODO(crbug.com/40941928): On iOS, also non-trusted events reset the
  // property.
  bool is_user_edited() const { return is_user_edited_; }
  void set_is_user_edited(bool is_user_edited) {
    is_user_edited_ = is_user_edited;
  }

  CheckStatus check_status() const { return check_status_; }
  void set_check_status(CheckStatus check_status) {
    check_status_ = check_status;
  }
  bool is_focusable() const { return is_focusable_; }
  void set_is_focusable(bool is_focusable) { is_focusable_ = is_focusable; }
  bool is_visible() const { return is_visible_; }
  void set_is_visible(bool is_visible) { is_visible_ = is_visible; }
  bool should_autocomplete() const { return should_autocomplete_; }
  void set_should_autocomplete(bool should_autocomplete) {
    should_autocomplete_ = should_autocomplete;
  }
  RoleAttribute role() const { return role_; }
  void set_role(RoleAttribute role) { role_ = role; }
  base::i18n::TextDirection text_direction() const { return text_direction_; }
  void set_text_direction(base::i18n::TextDirection text_direction) {
    text_direction_ = text_direction;
  }
  FieldPropertiesMask properties_mask() const { return properties_mask_; }
  void set_properties_mask(FieldPropertiesMask properties_mask) {
    properties_mask_ = properties_mask;
  }

  // Data members from the next block are used for parsing only, they are not
  // serialised for storage.
  bool is_enabled() const { return is_enabled_; }
  void set_is_enabled(bool is_enabled) { is_enabled_ = is_enabled; }
  bool is_readonly() const { return is_readonly_; }
  void set_is_readonly(bool is_readonly) { is_readonly_ = is_readonly; }
  // Contains password, username or credit card number value that was either
  // manually typed or autofilled on user trigger into a text-mode input field.
  const std::u16string& user_input() const { return user_input_; }
  void set_user_input(std::u16string user_input) {
    user_input_ = std::move(user_input);
  }

  // The computed writingsuggestions value. See
  // https://html.spec.whatwg.org/multipage/interaction.html#writing-suggestions
  // for spec.
  // TODO(crbug.com/338590542): Extract on iOS.
  bool allows_writing_suggestions() const {
    return allows_writing_suggestions_;
  }
  void set_allows_writing_suggestions(bool allows_writing_suggestions) {
    allows_writing_suggestions_ = allows_writing_suggestions;
  }

  // The options of a select box.
  const std::vector<SelectOption>& options() const { return options_; }
  void set_options(std::vector<SelectOption> options) {
    options_ = std::move(options);
  }

  // Password Manager doesn't use labels nor client side nor server side, so
  // label_source isn't in serialize methods.
  LabelSource label_source() const { return label_source_; }
  void set_label_source(LabelSource label_source) {
    label_source_ = label_source;
  }

  // The bounds of this field in current frame coordinates at the
  // form-extraction time. It is valid if not empty, will not be synced to the
  // server side or be used for field comparison and isn't in serialize methods.
  const gfx::RectF& bounds() const { return bounds_; }
  void set_bounds(gfx::RectF bounds) { bounds_ = std::move(bounds); }

  // The datalist is associated with this field, if any. Will not be synced to
  // the server side or be used for field comparison and aren't in serialize
  // methods.
  const std::vector<SelectOption>& datalist_options() const {
    return datalist_options_;
  }
  void set_datalist_options(std::vector<SelectOption> datalist_options) {
    datalist_options_ = std::move(datalist_options);
  }

  // When sent from browser to renderer, this bit indicates whether a field
  // should be filled even though it is already considered autofilled OR
  // user modified.
  bool force_override() const { return force_override_; }
  void set_force_override(bool force_override) {
    force_override_ = force_override;
  }

 private:
  std::u16string name_;
  std::u16string id_attribute_;
  std::u16string name_attribute_;
  std::u16string label_;
  std::u16string value_;
  std::u16string selected_text_;
  FormControlType form_control_type_ = FormControlType::kInputText;
  std::string autocomplete_attribute_;
  std::optional<AutocompleteParsingResult> parsed_autocomplete_;
  std::u16string placeholder_;
  std::u16string css_classes_;
  std::u16string aria_label_;
  std::u16string aria_description_;
  LocalFrameToken host_frame_;
  FieldRendererId renderer_id_;
  FormRendererId host_form_id_;
  FormSignature host_form_signature_;
  url::Origin origin_;
  int32_t form_control_ax_id_ = 0;
  uint64_t max_length_ = std::numeric_limits<uint32_t>::max();
  Section section_;
  bool is_autofilled_ = false;
  bool is_user_edited_ = false;
  CheckStatus check_status_ = CheckStatus::kNotCheckable;
  bool is_focusable_ = true;
  bool is_visible_ = true;  // See `features::kAutofillDetectFieldVisibility`.
  bool should_autocomplete_ = true;
  RoleAttribute role_ = RoleAttribute::kOther;
  base::i18n::TextDirection text_direction_ = base::i18n::UNKNOWN_DIRECTION;
  FieldPropertiesMask properties_mask_ = 0;
  bool is_enabled_ = false;
  bool is_readonly_ = false;
  std::u16string user_input_;
  bool allows_writing_suggestions_ = true;
  std::vector<SelectOption> options_;
  LabelSource label_source_ = LabelSource::kUnknown;
  gfx::RectF bounds_;
  std::vector<SelectOption> datalist_options_;
  bool force_override_ = false;
};

// Structure containing necessary information to be sent from the browser to the
// renderer in order to fill a field.
// See documentation of FormFieldData for more info.
struct FormFieldData::FillData {
  FillData();
  explicit FillData(const FormFieldData& field);
  FillData(const FillData&);
  FillData& operator=(const FillData&);

  ~FillData();

  // The field value to be set by the renderer.
  std::u16string value;

  // Uniquely identifies the DOM element that this field represents among the
  // field DOM elements in the same document.
  FieldRendererId renderer_id;

  // Uniquely identifies the DOM element of the form containing this field among
  // elements in the same document (or the collection of unowned fields of the
  // DOM in case this ID is null).
  FormRendererId host_form_id;

  // The unique identifier of the section (e.g. billing vs. shipping address)
  // of this field. This is only used on iOS.
  // TODO(crbug.com/40266549): Remove when Undo Autofill launches on iOS.
  Section section;

  // Whether the renderer should mark the field as autofilled or not. In most
  // filling cases this will be true. However for the case of UndoAutofill we
  // might wanna revert a field state into not autofilled, in which case this
  // would be false.
  bool is_autofilled = false;

  // When sent from browser to renderer, this bit indicates whether a field
  // should be filled even though it is already considered autofilled OR
  // user modified.
  // TODO(crbug.com/40943206): Remove.
  bool force_override = false;
};

std::string_view FormControlTypeToString(FormControlType type);

// Consider using the FormControlType enum instead.
//
// The fallback value is returned if `type_string` has no corresponding enum
// value in `FormControlType`. Regular use-cases should not need to pass a
// fallback value because `FormControlType` reflects all autofillable form
// control types.
//
// An exception where a fallback is needed is deserialization code. For legacy
// reasons, form control types are serialized as strings. The fallback value
// handles cases where the serialized data is corrupted or perhaps refers to an
// old form control type that has been removed from the HTML spec or from
// Autofill since.
FormControlType StringToFormControlTypeDiscouraged(
    std::string_view type_string,
    std::optional<FormControlType> fallback = std::nullopt);

// Serialize and deserialize FormFieldData. These are used when FormData objects
// are serialized and deserialized.
void SerializeFormFieldData(const FormFieldData& form_field_data,
                            base::Pickle* serialized);
bool DeserializeFormFieldData(base::PickleIterator* pickle_iterator,
                              FormFieldData* form_field_data);

// So we can compare FormFieldDatas with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const FormFieldData& field);

// Prefer to use this macro in place of |EXPECT_EQ()| for comparing
// |FormFieldData|s in test code.
// TODO(crbug.com/40765988): Replace this with FormData::DeepEqual().
#define EXPECT_FORM_FIELD_DATA_EQUALS(expected, actual)                      \
  do {                                                                       \
    EXPECT_EQ(expected.label(), actual.label());                             \
    EXPECT_EQ(expected.name(), actual.name());                               \
    EXPECT_EQ(expected.value(), actual.value());                             \
    EXPECT_EQ(expected.form_control_type(), actual.form_control_type());     \
    EXPECT_EQ(expected.autocomplete_attribute(),                             \
              actual.autocomplete_attribute());                              \
    EXPECT_EQ(expected.parsed_autocomplete(), actual.parsed_autocomplete()); \
    EXPECT_EQ(expected.placeholder(), actual.placeholder());                 \
    EXPECT_EQ(expected.max_length(), actual.max_length());                   \
    EXPECT_EQ(expected.css_classes(), actual.css_classes());                 \
    EXPECT_EQ(expected.is_autofilled(), actual.is_autofilled());             \
    EXPECT_EQ(expected.is_user_edited(), actual.is_user_edited());           \
    EXPECT_EQ(expected.section(), actual.section());                         \
    EXPECT_EQ(expected.check_status(), actual.check_status());               \
    EXPECT_EQ(expected.properties_mask(), actual.properties_mask());         \
    EXPECT_EQ(expected.id_attribute(), actual.id_attribute());               \
    EXPECT_EQ(expected.name_attribute(), actual.name_attribute());           \
  } while (0)

// Produces a <table> element with information about the form.
LogBuffer& operator<<(LogBuffer& buffer, const FormFieldData& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_FIELD_DATA_H_
