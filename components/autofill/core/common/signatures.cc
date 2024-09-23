// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/signatures.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "url/gurl.h"

namespace autofill {

namespace {

// Returns a copy of |input| without >= 5 consecutive digits.
std::string StripDigitsIfRequired(std::string_view input) {
  static constexpr auto IsDigit = base::IsAsciiDigit<char>;
  std::string result;
  result.reserve(input.size());

  for (size_t i = 0; i < input.size();) {
    // If `input[i]` is not a digit, append it to `result` and move to the next
    // character.
    if (!IsDigit(input[i])) {
      result.push_back(input[i]);
      ++i;
      continue;
    }

    // If `input[i]` is a digit, find the range of consecutive digits starting
    // at `i`. If this range is shorter than 5 characters append it to `result`.
    auto end_it = base::ranges::find_if_not(input.substr(i), IsDigit);
    std::string_view digits = base::MakeStringPiece(input.begin() + i, end_it);
    DCHECK(std::ranges::all_of(digits, IsDigit));
    if (digits.size() < 5)
      base::StrAppend(&result, {digits});
    i += digits.size();
  }

  return result;
}

template <size_t N>
uint64_t PackBytes(base::span<const uint8_t, N> bytes) {
  static_assert(N <= 8u,
                "Error: Can't pack more than 8 bytes into a uint64_t.");
  uint64_t result = 0;
  for (auto byte : bytes)
    result = (result << 8) | byte;
  return result;
}

}  // namespace

// If a form name was set by Chrome, we should ignore it when calculating
// the form signature.
std::string GetDOMFormName(const std::string& form_name) {
#if BUILDFLAG(IS_IOS)
  // In case of an empty form name, the synthetic name is created. Ignore it.
  return (StartsWith(form_name, "gChrome~form~", base::CompareCase::SENSITIVE)
              ? std::string()
              : form_name);
#else
  return form_name;
#endif
}

FormSignature CalculateFormSignature(const FormData& form_data) {
  const GURL& target_url = form_data.action();
  const GURL& source_url = form_data.url();
  std::string_view scheme = target_url.scheme_piece();
  std::string_view host = target_url.host_piece();

  // If target host or scheme is empty, set scheme and host of source url.
  // This is done to match the Toolbar's behavior.
  if (scheme.empty() || host.empty()) {
    scheme = source_url.scheme_piece();
    host = source_url.host_piece();
  }

  std::string form_signature_field_names;

  for (const FormFieldData& field : form_data.fields()) {
    if (!IsCheckable(field.check_status())) {
      // Add all supported form fields (including with empty names) to the
      // signature.  This is a requirement for Autofill servers.
      base::StrAppend(
          &form_signature_field_names,
          {"&", StripDigitsIfRequired(base::UTF16ToUTF8(field.name()))});
    }
  }

  std::string form_name = StripDigitsIfRequired(
      GetDOMFormName(base::UTF16ToUTF8(form_data.name())));
  std::string form_string = base::StrCat(
      {scheme, "://", host, "&", form_name, form_signature_field_names});
  return FormSignature(StrToHash64Bit(form_string));
}

FormSignature CalculateAlternativeFormSignature(const FormData& form_data) {
  std::string_view scheme = form_data.action().scheme_piece();
  std::string_view host = form_data.action().host_piece();

  // If target host or scheme is empty, set scheme and host of source url.
  // This is done to match the Toolbar's behavior.
  if (scheme.empty() || host.empty()) {
    scheme = form_data.url().scheme_piece();
    host = form_data.url().host_piece();
  }

  std::string form_signature_field_types;
  for (const FormFieldData& field : form_data.fields()) {
    if (!IsCheckable(field.check_status())) {
      // Add all supported form fields' form control types to the signature.
      // We use the string representation of the FormControlType because
      // changing the signature algorithm is non-trivial. If and when the
      // sectioning algorithm, we could use the raw FormControlType enum
      // instead.
      base::StrAppend(
          &form_signature_field_types,
          {"&", FormControlTypeToString(field.form_control_type())});
    }
  }

  std::string form_string =
      base::StrCat({scheme, "://", host, form_signature_field_types});

  // Add more non-empty elements (one of path, reference, or query ordered by
  // preference) for small forms with 1-2 fields in order to prevent signature
  // collisions.
  if (form_data.fields().size() <= 2) {
    // Path piece includes the slash "/", so a non-empty path must have length
    // longer than 1.
    if (form_data.url().path_piece().length() > 1) {
      base::StrAppend(&form_string, {form_data.url().path_piece()});
    } else if (form_data.url().has_ref()) {
      base::StrAppend(&form_string, {"#", form_data.url().ref_piece()});
    } else if (form_data.url().has_query()) {
      base::StrAppend(&form_string, {"?", form_data.url().query_piece()});
    }
  }

  return FormSignature(StrToHash64Bit(form_string));
}

FieldSignature CalculateFieldSignatureByNameAndType(
    std::u16string_view field_name,
    FormControlType field_type) {
  return FieldSignature(
      StrToHash32Bit(base::StrCat({base::UTF16ToUTF8(field_name), "&",
                                   FormControlTypeToString(field_type)})));
}

FieldSignature CalculateFieldSignatureForField(
    const FormFieldData& field_data) {
  return CalculateFieldSignatureByNameAndType(field_data.name(),
                                              field_data.form_control_type());
}

uint64_t StrToHash64Bit(std::string_view str) {
  auto bytes = base::as_bytes(base::make_span(str));
  const base::SHA1Digest digest = base::SHA1Hash(bytes);
  return PackBytes(base::make_span(digest).subspan<0, 8>());
}

uint32_t StrToHash32Bit(std::string_view str) {
  auto bytes = base::as_bytes(base::make_span(str));
  const base::SHA1Digest digest = base::SHA1Hash(bytes);
  return PackBytes(base::make_span(digest).subspan<0, 4>());
}

int64_t HashFormSignature(FormSignature form_signature) {
  return static_cast<uint64_t>(form_signature.value()) % 1021;
}

int64_t HashFieldSignature(FieldSignature field_signature) {
  return static_cast<uint64_t>(field_signature.value()) % 1021;
}

}  // namespace autofill
