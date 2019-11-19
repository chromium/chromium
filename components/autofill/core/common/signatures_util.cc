// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/signatures_util.h"

#include <cctype>

#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "url/gurl.h"

namespace autofill {

namespace {

// Returns a copy of |input| without >= 5 consecutive digits.
std::string StripDigitsIfRequired(const base::string16& input) {
  std::string input_utf8 = base::UTF16ToUTF8(input);
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

}  // namespace

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

  for (const FormFieldData& field : form_data.fields) {
    if (!ShouldSkipField(field)) {
      // Add all supported form fields (including with empty names) to the
      // signature.  This is a requirement for Autofill servers.
      form_signature_field_names.append("&");
      form_signature_field_names.append(StripDigitsIfRequired(field.name));
    }
  }

  std::string form_string = scheme + "://" + host + "&" +
                            base::UTF16ToUTF8(form_data.name) +
                            form_signature_field_names;

  return StrToHash64Bit(form_string);
}

FieldSignature CalculateFieldSignatureByNameAndType(
    const base::string16& field_name,
    const std::string& field_type) {
  std::string name = base::UTF16ToUTF8(field_name);
  std::string field_string = name + "&" + field_type;
  return StrToHash32Bit(field_string);
}

FieldSignature CalculateFieldSignatureForField(
    const FormFieldData& field_data) {
  return CalculateFieldSignatureByNameAndType(field_data.name,
                                              field_data.form_control_type);
}

uint64_t StrToHash64Bit(const std::string& str) {
  std::string hash_bin = base::SHA1HashString(str);
  DCHECK_EQ(base::kSHA1Length, hash_bin.length());

  uint64_t hash64 = (((static_cast<uint64_t>(hash_bin[0])) & 0xFF) << 56) |
                    (((static_cast<uint64_t>(hash_bin[1])) & 0xFF) << 48) |
                    (((static_cast<uint64_t>(hash_bin[2])) & 0xFF) << 40) |
                    (((static_cast<uint64_t>(hash_bin[3])) & 0xFF) << 32) |
                    (((static_cast<uint64_t>(hash_bin[4])) & 0xFF) << 24) |
                    (((static_cast<uint64_t>(hash_bin[5])) & 0xFF) << 16) |
                    (((static_cast<uint64_t>(hash_bin[6])) & 0xFF) << 8) |
                    ((static_cast<uint64_t>(hash_bin[7])) & 0xFF);

  return hash64;
}

uint32_t StrToHash32Bit(const std::string& str) {
  std::string hash_bin = base::SHA1HashString(str);
  DCHECK_EQ(base::kSHA1Length, hash_bin.length());

  uint32_t hash32 = ((hash_bin[0] & 0xFF) << 24) |
                    ((hash_bin[1] & 0xFF) << 16) | ((hash_bin[2] & 0xFF) << 8) |
                    (hash_bin[3] & 0xFF);

  return hash32;
}

}  // namespace autofill
