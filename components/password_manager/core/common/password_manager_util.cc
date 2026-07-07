// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_util.h"

#include <algorithm>

#include "base/i18n/char_iterator.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "third_party/icu/source/common/unicode/uchar.h"

namespace password_manager::util {

namespace {

// The minimum number of alphabetic characters in the input name or id that
// allows considering it as a potential single username field.
const size_t kMinAlphabeticCharsForSingleUsername = 2;

// Returns true if the field attributes indicate a password field.
bool IsLikelyPasswordField(std::u16string_view name, std::u16string_view id) {
  return autofill::MatchesRegex<constants::kPasswordRe>(name) ||
         autofill::MatchesRegex<constants::kPasswordRe>(id);
}

// Returns true if `s` is a meaningful name or id for a single username field.
// It must contain at least `kMinAlphabeticCharsForSingleUsername` alphabetic
// characters (which also guarantees the length is at least
// `kMinAlphabeticCharsForSingleUsername`).
bool IsMeaningfulNameOrId(std::u16string_view s) {
  size_t meaningful_char_count = 0;
  for (base::i18n::UTF16CharIterator iter(s); !iter.end(); iter.Advance()) {
    if (u_isalpha(iter.get()) &&
        ++meaningful_char_count >= kMinAlphabeticCharsForSingleUsername) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool FormContainsWebauthnAutocomplete(const autofill::FormData& form) {
  return std::ranges::any_of(
      form.fields(), [](const autofill::FormFieldData& field) {
        std::vector<std::string_view> tokens = base::SplitStringPiece(
            field.autocomplete_attribute(), base::kWhitespaceASCII,
            base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

        return !tokens.empty() &&
               base::EqualsCaseInsensitiveASCII(
                   tokens.back(), constants::kAutocompleteWebAuthn);
      });
}

bool IsRendererRecognizedCredentialForm(const autofill::FormData& form) {
  // TODO(crbug.com/40276126): Consolidate with the parsing logic in
  // form_autofill_util.cc.
  return std::ranges::any_of(
      form.fields(), [](const autofill::FormFieldData& field) {
        return field.IsPasswordInputElement() ||
               field.autocomplete_attribute().find(
                   password_manager::constants::kAutocompleteUsername) !=
                   std::string::npos ||
               field.autocomplete_attribute().find(
                   password_manager::constants::kAutocompleteWebAuthn) !=
                   std::string::npos ||
               IsLikelyPasswordField(field.name_attribute(),
                                     field.id_attribute());
      });
}

bool CanFieldBeConsideredAsSingleUsername(
    const std::u16string& name,
    const std::u16string& id,
    const std::u16string& label,
    std::optional<autofill::FormControlType> type) {
  // Do not consider fields with very short or meaningless names/ids to avoid
  // aggregating multiple unrelated fields on the server. (crbug.com/1209143)
  if (!IsMeaningfulNameOrId(name) && !IsMeaningfulNameOrId(id)) {
    return false;
  }
  // Do not consider fields if their HTML attributes indicate they
  // are search fields.
  return base::ToLowerASCII(name).find(password_manager::constants::kSearch) ==
             std::u16string::npos &&
         base::ToLowerASCII(id).find(password_manager::constants::kSearch) ==
             std::u16string::npos &&
         base::ToLowerASCII(label).find(password_manager::constants::kSearch) ==
             std::u16string::npos &&
         type.has_value() &&  // Only autofillable fields have a `type` value.
         type.value() != autofill::FormControlType::kInputSearch;
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
  return autocomplete.contains(
             password_manager::constants::kAutocompleteOneTimePassword) ||
         autofill::MatchesRegex<password_manager::constants::kOneTimePwdRe>(
             name) ||
         autofill::MatchesRegex<password_manager::constants::kOneTimePwdRe>(id);
}

}  // namespace password_manager::util
