// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_util.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/common/password_manager_constants.h"

namespace password_manager::util {

// The minimum length of the input name that allows considering it as potential
// single username field.
const size_t kMinInputNameLengthForSingleUsername = 2;

bool IsRendererRecognizedCredentialForm(const autofill::FormData& form) {
  // TODO(crbug.com/40276126): Consolidate with the parsing logic in
  // form_autofill_util.cc.
  return base::ranges::any_of(
      form.fields(), [](const autofill::FormFieldData& field) {
        return field.IsPasswordInputElement() ||
               field.autocomplete_attribute().find(
                   password_manager::constants::kAutocompleteUsername) !=
                   std::string::npos ||
               field.autocomplete_attribute().find(
                   password_manager::constants::kAutocompleteWebAuthn) !=
                   std::string::npos;
      });
}

bool CanFieldBeConsideredAsSingleUsername(const std::u16string& name,
                                          const std::u16string& id,
                                          const std::u16string& label) {
  // Do not consider fields with very short names/ids to avoid aggregating
  // multiple unrelated fields on the server. (crbug.com/1209143)
  if (name.length() < kMinInputNameLengthForSingleUsername &&
      id.length() < kMinInputNameLengthForSingleUsername) {
    return false;
  }
  // Do not consider fields if their HTML attributes indicate they
  // are search fields.
  return (name.find(password_manager::constants::kSearch) ==
          std::u16string::npos) &&
         (id.find(password_manager::constants::kSearch) ==
          std::u16string::npos) &&
         (label.find(password_manager::constants::kSearch) ==
          std::u16string::npos);
}

bool CanValueBeConsideredAsSingleUsername(const std::u16string& value) {
  // Do not consider 1-symbol values, as they are unlikely to be usernames and
  // likely to be characters/digits of OTPs. Exclude too large values, as they
  // are usually not usernames. Exclude empty values as they don't hold any
  // information for the username.
  return value.size() > 1 && value.size() <= 100;
}

bool IsLikelyOtp(std::u16string_view name,
                 std::u16string_view id,
                 std::string_view autocomplete) {
  return base::Contains(
             autocomplete,
             password_manager::constants::kAutocompleteOneTimePassword) ||
         autofill::MatchesRegex<password_manager::constants::kOneTimePwdRe>(
             name) ||
         autofill::MatchesRegex<password_manager::constants::kOneTimePwdRe>(id);
}

}  // namespace password_manager::util
