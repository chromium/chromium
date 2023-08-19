// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/signatures.h"

#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "url/gurl.h"

using base::UTF16ToUTF8;

namespace autofill {

namespace {

// Returns a copy of |input| without >= 5 consecutive digits.
std::string StripDigitsIfRequired(base::StringPiece input) {
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
    auto* end_it = base::ranges::find_if_not(input.substr(i), IsDigit);
    base::StringPiece digits = base::MakeStringPiece(input.begin() + i, end_it);
    DCHECK(base::ranges::all_of(digits, IsDigit));
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
  const GURL& target_url = form_data.action;
  const GURL& source_url = form_data.url;
  base::StringPiece scheme = target_url.scheme_piece();
  base::StringPiece host = target_url.host_piece();

  // If target host or scheme is empty, set scheme and host of source url.
  // This is done to match the Toolbar's behavior.
  if (scheme.empty() || host.empty()) {
    scheme = source_url.scheme_piece();
    host = source_url.host_piece();
  }

  std::string form_signature_field_names;

  for (const FormFieldData& field : form_data.fields) {
    if (!IsCheckable(field.check_status)) {
      // Add all supported form fields (including with empty names) to the
      // signature.  This is a requirement for Autofill servers.
      base::StrAppend(&form_signature_field_names,
                      {"&", StripDigitsIfRequired(UTF16ToUTF8(field.name))});
    }
  }

  std::string form_name =
      StripDigitsIfRequired(GetDOMFormName(UTF16ToUTF8(form_data.name)));
  std::string form_string = base::StrCat(
      {scheme, "://", host, "&", form_name, form_signature_field_names});
  return FormSignature(StrToHash64Bit(form_string));
}

FieldSignature CalculateFieldSignatureByNameAndType(
    base::StringPiece16 field_name,
    base::StringPiece field_type) {
  return FieldSignature(
      StrToHash32Bit(base::StrCat({UTF16ToUTF8(field_name), "&", field_type})));
}

FieldSignature CalculateFieldSignatureForField(
    const FormFieldData& field_data) {
  return CalculateFieldSignatureByNameAndType(field_data.name,
                                              field_data.form_control_type);
}

uint64_t StrToHash64Bit(base::StringPiece str) {
  auto bytes = base::as_bytes(base::make_span(str));
  const base::SHA1Digest digest = base::SHA1HashSpan(bytes);
  return PackBytes(base::make_span(digest).subspan<0, 8>());
}

uint32_t StrToHash32Bit(base::StringPiece str) {
  auto bytes = base::as_bytes(base::make_span(str));
  const base::SHA1Digest digest = base::SHA1HashSpan(bytes);
  return PackBytes(base::make_span(digest).subspan<0, 4>());
}

int64_t HashFormSignature(FormSignature form_signature) {
  return static_cast<uint64_t>(form_signature.value()) % 1021;
}

int64_t HashFieldSignature(FieldSignature field_signature) {
  return static_cast<uint64_t>(field_signature.value()) % 1021;
}

}  // namespace autofill
