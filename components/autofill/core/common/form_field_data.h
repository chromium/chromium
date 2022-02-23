// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_FIELD_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_FIELD_DATA_H_

#include <stddef.h>

#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include "base/i18n/rtl.h"
#include "build/build_config.h"
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

// FieldPropertiesMask is used to contain combinations of FieldPropertiesFlags
// values.
using FieldPropertiesMask = std::underlying_type_t<FieldPropertiesFlags>;

// For the HTML snippet |<option value="US">United States</option>|, the
// value is "US" and the contents is "United States".
struct SelectOption {
  std::u16string value;
  std::u16string content;
};

// Stores information about a field in a form. Read more about forms and fields
// at FormData.
struct FormFieldData {
  using CheckStatus = mojom::FormFieldData_CheckStatus;
  using RoleAttribute = mojom::FormFieldData_RoleAttribute;
  using LabelSource = mojom::FormFieldData_LabelSource;

  // TODO(crbug/1211834): This comparator is deprecated.
  // Less-than relation for STL containers. Compares only members needed to
  // uniquely identify a field.
  struct IdentityComparator {
    bool operator()(const FormFieldData& a, const FormFieldData& b) const;
  };

  // Returns true if all members of fields |a| and |b| are identical.
  static bool DeepEqual(const FormFieldData& a, const FormFieldData& b);

  FormFieldData();
  FormFieldData(const FormFieldData&);
  FormFieldData& operator=(const FormFieldData&);
  FormFieldData(FormFieldData&&);
  FormFieldData& operator=(FormFieldData&&);
  ~FormFieldData();

  // An identifier that is unique across all fields in all frames.
  // Must not be leaked to renderer process. See FieldGlobalId for details.
  FieldGlobalId global_id() const { return {host_frame, unique_renderer_id}; }

  // An identifier of the renderer form that contained this field.
  // This may be from the browser form that contains this field in the case of a
  // frame-transcending form. See ContentAutofillRouter and internal::FormForest
  // for details on the distinction between renderer and browser forms.
  FormGlobalId renderer_form_id() const { return {host_frame, host_form_id}; }

  // TODO(crbug/1211834): This function is deprecated. Use
  // FormFieldData::DeepEqual() instead.
  // Returns true if both fields are identical, ignoring value- and
  // parsing related members.
  // See also SimilarFieldAs(), DynamicallySameFieldAs().
  bool SameFieldAs(const FormFieldData& field) const;

  // TODO(crbug/1211834): This function is deprecated.
  // Returns true if both fields are identical, ignoring members that
  // are typically changed dynamically.
  // Strictly weaker than SameFieldAs().
  bool SimilarFieldAs(const FormFieldData& field) const;

  // TODO(crbug/1211834): This function is deprecated.
  // Returns true if both forms are equivalent from the POV of dynamic refills.
  // Strictly weaker than SameFieldAs(): replaces equality of |is_focusable| and
  // |role| with equality of IsVisible().
  bool DynamicallySameFieldAs(const FormFieldData& field) const;

  // Returns true for all of textfield-looking types: text, password,
  // search, email, url, and number. It must work the same way as Blink function
  // WebInputElement::IsTextField(), and it returns false if |*this| represents
  // a textarea.
  bool IsTextInputElement() const;

  bool IsPasswordInputElement() const;

  // Returns true if the field is visible to the user.
  bool IsVisible() const {
    return is_focusable && role != RoleAttribute::kPresentation;
  }

  // These functions do not work for Autofill code.
  // TODO(https://crbug.com/1006745): Fix this.
  bool DidUserType() const;
  bool HadFocus() const;
  bool WasAutofilled() const;

#if BUILDFLAG(IS_IOS)
  // The identifier which uniquely addresses this field in the DOM. This is an
  // ephemeral value which is not guaranteed to be stable across page loads. It
  // serves to allow a given field to be found during the current navigation.
  //
  // TODO(crbug.com/896689): Expand the logic/application of this to other
  // platforms and/or merge this concept with |unique_renderer_id|.
  std::u16string unique_id;
#define EXPECT_EQ_UNIQUE_ID(expected, actual) \
  EXPECT_EQ((expected).unique_id, (actual).unique_id)
#else
#define EXPECT_EQ_UNIQUE_ID(expected, actual)
#endif

  // NOTE: update IdentityComparator                 when adding new a member.
  // NOTE: update SameFieldAs()            if needed when adding new a member.
  // NOTE: update SimilarFieldAs()         if needed when adding new a member.
  // NOTE: update DynamicallySameFieldAs() if needed when adding new a member.

  // The name by which autofill knows this field. This is generally either the
  // name attribute or the id_attribute value, which-ever is non-empty with
  // priority given to the name_attribute. This value is used when computing
  // form signatures.
  // TODO(crbug/896689): remove this and use attributes/unique_id instead.
  std::u16string name;

  std::u16string id_attribute;
  std::u16string name_attribute;
  std::u16string label;
  std::u16string value;
  std::string form_control_type;
  std::string autocomplete_attribute;
  std::u16string placeholder;
  std::u16string css_classes;
  std::u16string aria_label;
  std::u16string aria_description;

  // A unique identifier of the containing frame. This value is not serialized
  // because LocalFrameTokens must not be leaked to other renderer processes.
  // It is not persistent between page loads and therefore not used in
  // comparison in SameFieldAs().
  LocalFrameToken host_frame;

  // An identifier of the field that is unique among the field from the same
  // frame. In the browser process, it should only be used in conjunction with
  // |host_frame| to identify a field; see global_id(). It is not persistent
  // between page loads and therefore not used in comparison in SameFieldAs().
  FieldRendererId unique_renderer_id;

  // Unique renderer ID of the enclosing form in the same frame.
  FormRendererId host_form_id;

  // The signature of the field's renderer form, that is, the signature of the
  // FormData that contained this field when it was received by the
  // AutofillDriver (see ContentAutofillRouter and internal::FormForest
  // for details on the distinction between renderer and browser forms). The
  // value is only set in ContentAutofillDriver and null on iOS.
  // This value is written and read only in the browser for voting of
  // cross-frame forms purposes. It is therefore not sent via mojo.
  FormSignature host_form_signature;

  // The origin of the frame that hosts the field.
  url::Origin origin;

  // The ax node id of the form control in the accessibility tree.
  int32_t form_control_ax_id = 0;

  // The unique identifier of the section (e.g. billing vs. shipping address)
  // of this field.
  std::string section;

  // Note: we use uint64_t instead of size_t because this struct is sent over
  // IPC which could span 32 & 64 bit processes. We chose uint64_t instead of
  // uint32_t to maintain compatibility with old code which used size_t
  // (base::Pickle used to serialize that as 64 bit).
  uint64_t max_length = 0;
  bool is_autofilled = false;
  CheckStatus check_status = CheckStatus::kNotCheckable;
  bool is_focusable = true;
  bool should_autocomplete = true;
  RoleAttribute role = RoleAttribute::kOther;
  base::i18n::TextDirection text_direction = base::i18n::UNKNOWN_DIRECTION;
  FieldPropertiesMask properties_mask = 0;

  // Data members from the next block are used for parsing only, they are not
  // serialised for storage.
  bool is_enabled = false;
  bool is_readonly = false;
  // Contains value that was either manually typed or autofilled on user
  // trigger.
  std::u16string user_input;

  // The options of a select box.
  std::vector<SelectOption> options;

  // Password Manager doesn't use labels nor client side nor server side, so
  // label_source isn't in serialize methods.
  LabelSource label_source = LabelSource::kUnknown;

  // The bounds of this field in current frame coordinates at the parse time. It
  // is valid if not empty, will not be synced to the server side or be used for
  // field comparison and isn't in serialize methods.
  gfx::RectF bounds;

  // The datalist is associated with this field, if any. The following two
  // vectors valid if not empty, will not be synced to the server side or be
  // used for field comparison and aren't in serialize methods.
  // The datalist option is intentionally separated from |options| because they
  // are handled very differently in Autofill.
  std::vector<std::u16string> datalist_values;
  std::vector<std::u16string> datalist_labels;
};

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
#define EXPECT_FORM_FIELD_DATA_EQUALS(expected, actual)                        \
  do {                                                                         \
    EXPECT_EQ_UNIQUE_ID(expected, actual);                                     \
    EXPECT_EQ(expected.label, actual.label);                                   \
    EXPECT_EQ(expected.name, actual.name);                                     \
    EXPECT_EQ(expected.value, actual.value);                                   \
    EXPECT_EQ(expected.form_control_type, actual.form_control_type);           \
    EXPECT_EQ(expected.autocomplete_attribute, actual.autocomplete_attribute); \
    EXPECT_EQ(expected.placeholder, actual.placeholder);                       \
    EXPECT_EQ(expected.max_length, actual.max_length);                         \
    EXPECT_EQ(expected.css_classes, actual.css_classes);                       \
    EXPECT_EQ(expected.is_autofilled, actual.is_autofilled);                   \
    EXPECT_EQ(expected.section, actual.section);                               \
    EXPECT_EQ(expected.check_status, actual.check_status);                     \
    EXPECT_EQ(expected.properties_mask, actual.properties_mask);               \
    EXPECT_EQ(expected.id_attribute, actual.id_attribute);                     \
    EXPECT_EQ(expected.name_attribute, actual.name_attribute);                 \
  } while (0)

// Produces a <table> element with information about the form.
LogBuffer& operator<<(LogBuffer& buffer, const FormFieldData& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_FIELD_DATA_H_
