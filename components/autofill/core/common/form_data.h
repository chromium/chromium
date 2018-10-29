// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_

#include <limits>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/common/form_field_data.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

// Holds information about a form to be filled and/or submitted.
struct FormData {
  static constexpr uint32_t kNotSetFormRendererId =
      std::numeric_limits<uint32_t>::max();

  FormData();
  FormData(const FormData& data);
  ~FormData();

  // Returns true if two forms are the same, not counting the values of the
  // form elements.
  bool SameFormAs(const FormData& other) const;

  // Same as SameFormAs() except calling FormFieldData.SimilarFieldAs() to
  // compare fields.
  bool SimilarFormAs(const FormData& other) const;

  // If |form| is the same as this from the POV of dynamic refills.
  bool DynamicallySameFormAs(const FormData& form) const;

  // Note: operator==() performs a full-field-comparison(byte by byte), this is
  // different from SameFormAs(), which ignores comparison for those "values" of
  // all form fields, just like what FormFieldData::SameFieldAs() ignores.
  bool operator==(const FormData& form) const;
  bool operator!=(const FormData& form) const;
  // Allow FormData to be a key in STL containers.
  bool operator<(const FormData& form) const;

  // The name of the form.
  base::string16 name;
  // The form submission button's title.
  base::string16 button_title;
  // The URL (minus query parameters) containing the form.
  GURL origin;
  // The action target of the form.
  GURL action;
  // The URL of main frame containing this form.
  url::Origin main_frame_origin;
  // True if this form is a form tag.
  bool is_form_tag;
  // True if the form is made of unowned fields (i.e., not within a <form> tag)
  // in what appears to be a checkout flow. This attribute is only calculated
  // and used if features::kAutofillRestrictUnownedFieldsToFormlessCheckout is
  // enabled, to prevent heuristics from running on formless non-checkout.
  bool is_formless_checkout;
  //  Unique renderer id which is returned by function
  //  WebFormElement::UniqueRendererFormId(). It is not persistant between page
  //  loads, so it is not saved and not used in comparison in SameFormAs().
  uint32_t unique_renderer_id = kNotSetFormRendererId;
  // A vector of all the input fields in the form.
  std::vector<FormFieldData> fields;
  // Contains unique renderer IDs of text elements which are predicted to be
  // usernames. The order matters: elements are sorted in descending likelihood
  // of being a username (the first one is the most likely username). Can
  // contain IDs of elements which are not in |fields|. This is only used during
  // parsing into PasswordForm, and hence not serialised for storage.
  std::vector<uint32_t> username_predictions;
};

// For testing.
std::ostream& operator<<(std::ostream& os, const FormData& form);

// Serialize FormData. Used by the PasswordManager to persist FormData
// pertaining to password forms. Serialized data is appended to |pickle|.
void SerializeFormData(const FormData& form_data, base::Pickle* pickle);
// Deserialize FormData. This assumes that |iter| is currently pointing to
// the part of a pickle created by SerializeFormData. Returns true on success.
bool DeserializeFormData(base::PickleIterator* iter, FormData* form_data);

// Serialize FormData. Used by the PasswordManager to persist FormData
// pertaining to password forms in base64 string. It is useful since in some
// cases we need to store C strings without embedded '\0' symbols.
void SerializeFormDataToBase64String(const FormData& form_data,
                                     std::string* output);
// Deserialize FormData. Returns true on success.
bool DeserializeFormDataFromBase64String(const base::StringPiece& input,
                                         FormData* form_data);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_DATA_H_
