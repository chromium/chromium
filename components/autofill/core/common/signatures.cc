// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/signatures.h"

#include <cctype>

#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
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
std::string StripDigitsIfRequired(const std::u16string& input) {
  std::string input_utf8 = UTF16ToUTF8(input);
  std::string result;
  result.reserve(input_utf8.length());

  for (size_t i = 0; i < input_utf8.length();) {
    if (std::isdigit(input_utf8[i])) {
      size_t count = 0;
      while (i < input_utf8.length() && std::isdigit(input_utf8[i])) {
        i++;
        count++;
      }
      if (count < 5)
        result.append(input_utf8, i - count, count);
    } else {
      result.push_back(input_utf8[i]);
      i++;
    }
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
#if defined(OS_IOS)
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
  std::string scheme(target_url.scheme());
  std::string host(target_url.host());

  // If target host or scheme is empty, set scheme and host of source url.
  // This is done to match the Toolbar's behavior.
  if (scheme.empty() || host.empty()) {
    scheme = source_url.scheme();
    host = source_url.host();
  }

  std::string form_signature_field_names;

  auto ShouldSkipField = [](const FormFieldData& field) {
    return IsCheckable(field.check_status);
  };

  for (const FormFieldData& field : form_data.fields) {
    if (!ShouldSkipField(field)) {
      // Add all supported form fields (including with empty names) to the
      // signature.  This is a requirement for Autofill servers.
      form_signature_field_names.append("&");
      form_signature_field_names.append(StripDigitsIfRequired(field.name));
    }
  }

  std::string form_name = GetDOMFormName(UTF16ToUTF8(form_data.name));
  std::string form_string =
      scheme + "://" + host + "&" + form_name + form_signature_field_names;

  return FormSignature(StrToHash64Bit(form_string));
}

FieldSignature CalculateFieldSignatureByNameAndType(
    const std::u16string& field_name,
    const std::string& field_type) {
  std::string name = UTF16ToUTF8(field_name);
  std::string field_string = name + "&" + field_type;
  return FieldSignature(StrToHash32Bit(field_string));
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

int64_t HashFormSignature(autofill::FormSignature form_signature) {
  return static_cast<uint64_t>(form_signature.value()) % 1021;
}

int64_t HashFieldSignature(autofill::FieldSignature field_signature) {
  return static_cast<uint64_t>(field_signature.value()) % 1021;
}

}  // namespace autofill
