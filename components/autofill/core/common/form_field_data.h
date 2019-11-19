// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_FIELD_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_FIELD_DATA_H_

#include <stddef.h>

#include <limits>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace base {
class Pickle;
class PickleIterator;
}  // namespace base

namespace autofill {

class LogBuffer;

// The flags describing form field properties.
enum FieldPropertiesFlags {
  NO_FLAGS = 0u,
  USER_TYPED = 1u << 0,
  // AUTOFILLED means that at least one character of the field value comes from
  // being autofilled. This is different from
  // WebFormControlElement::IsAutofilled(). It is meant to be used for password
  // fields, to determine whether viewing the value needs user reauthentication.
  AUTOFILLED_ON_USER_TRIGGER = 1u << 1,
  // The field received focus at any moment.
  HAD_FOCUS = 1u << 2,
  // Use this flag, if some error occurred in flags processing.
  ERROR_OCCURRED = 1u << 3,
  // On submission, the value of the field was recognised as a value which is
  // already stored.
  KNOWN_VALUE = 1u << 4,
  // A value was autofilled on pageload. This means that at least one character
  // of the field value comes from being autofilled.
  AUTOFILLED_ON_PAGELOAD = 1u << 5,
  // A value was autofilled on any of the triggers.
  AUTOFILLED = AUTOFILLED_ON_USER_TRIGGER | AUTOFILLED_ON_PAGELOAD,
};

// FieldPropertiesMask is used to contain combinations of FieldPropertiesFlags
// values.
typedef uint32_t FieldPropertiesMask;

// Stores information about a field in a form.
struct FormFieldData {
  using CheckStatus = mojom::FormFieldData_CheckStatus;
  using RoleAttribute = mojom::FormFieldData_RoleAttribute;
  using LabelSource = mojom::FormFieldData_LabelSource;

  // Less-than relation for STL containers. Compares only members needed to
  // uniquely identify a field.
  struct IdentityComparator {
    bool operator()(const FormFieldData& a, const FormFieldData& b) const;
  };

  static constexpr uint32_t kNotSetFormControlRendererId =
      std::numeric_limits<uint32_t>::max();

  FormFieldData();
  FormFieldData(const FormFieldData&);
  FormFieldData& operator=(const FormFieldData&);
  FormFieldData(FormFieldData&&);
  FormFieldData& operator=(FormFieldData&&);
  ~FormFieldData();

  // Returns true if both fields are identical, ignoring value- and
  // parsing related members.
  // See also SimilarFieldAs(), DynamicallySameFieldAs().
  bool SameFieldAs(const FormFieldData& field) const;

  // Returns true if both fields are identical, ignoring members that
  // are typically changed dynamically.
  // Strictly weaker than SameFieldAs().
  bool SimilarFieldAs(const FormFieldData& field) const;

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

#if defined(OS_IOS)
  // The identifier which uniquely addresses this field in the DOM. This is an
  // ephemeral value which is not guaranteed to be stable across page loads. It
  // serves to allow a given field to be found during the current navigation.
  //
  // TODO(crbug.com/896689): Expand the logic/application of this to other
  // platforms and/or merge this concept with |unique_renderer_id|.
  base::string16 unique_id;
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
  base::string16 name;

  base::string16 id_attribute;
  base::string16 name_attribute;
  base::string16 label;
  base::string16 value;
  std::string form_control_type;
  std::string autocomplete_attribute;
  base::string16 placeholder;
  base::string16 css_classes;
  base::string16 aria_label;
  base::string16 aria_description;

  // Unique renderer id returned by WebFormElement::UniqueRendererFormId(). It
  // is not persistent between page loads, so it is not saved and not used in
  // comparison in SameFieldAs().
  uint32_t unique_renderer_id = kNotSetFormControlRendererId;

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
  base::string16 typed_value;

  // For the HTML snippet |<option value="US">United States</option>|, the
  // value is "US" and the contents are "United States".
  std::vector<base::string16> option_values;
  std::vector<base::string16> option_contents;

  // Password Manager doesn't use labels nor client side nor server side, so
  // label_source isn't in serialize methods.
  LabelSource label_source = LabelSource::kUnknown;
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
