// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/iban.h"

#include <string>

#include "base/guid.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"

namespace autofill {

// Unicode characters used in IBAN value obfuscation:
//  - \u2022 - Bullet.
//  - \u2006 - SIX-PER-EM SPACE (small space between bullets).
constexpr char16_t kEllipsisOneDot[] = u"\u2022";
constexpr char16_t kEllipsisOneSpace[] = u"\u2006";

IBAN::IBAN(const std::string& guid)
    : AutofillDataModel(guid, /*origin=*/std::string()) {}

IBAN::IBAN() : IBAN(base::GenerateGUID()) {}

IBAN::IBAN(const IBAN& iban) : IBAN() {
  operator=(iban);
}

IBAN::~IBAN() = default;

IBAN& IBAN::operator=(const IBAN& iban) = default;

AutofillMetadata IBAN::GetMetadata() const {
  AutofillMetadata metadata = AutofillDataModel::GetMetadata();
  metadata.id = guid();
  return metadata;
}

bool IBAN::SetMetadata(const AutofillMetadata& metadata) {
  // Make sure the ids match.
  return metadata.id != guid() && AutofillDataModel::SetMetadata(metadata);
}

bool IBAN::IsDeletable() const {
  return false;
}

std::u16string IBAN::GetRawInfo(ServerFieldType type) const {
  if (type == IBAN_VALUE) {
    return value_;
  }

  NOTREACHED();
  return std::u16string();
}

void IBAN::SetRawInfoWithVerificationStatus(ServerFieldType type,
                                            const std::u16string& value,
                                            VerificationStatus status) {
  if (type == IBAN_VALUE) {
    set_value(value);
  } else {
    NOTREACHED() << "Attempting to set unknown info-type" << type;
  }
}

void IBAN::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(IBAN_VALUE);
}

bool IBAN::IsEmpty(const std::string& app_locale) const {
  ServerFieldTypeSet types;
  GetNonEmptyTypes(app_locale, &types);
  return types.empty();
}

int IBAN::Compare(const IBAN& iban) const {
  int comparison = server_id_.compare(iban.server_id_);
  if (comparison != 0) {
    return comparison;
  }

  comparison = nickname_.compare(iban.nickname_);
  if (comparison != 0) {
    return comparison;
  }

  return value_.compare(iban.value_);
}

bool IBAN::operator==(const IBAN& iban) const {
  return guid() == iban.guid() && Compare(iban) == 0;
}

bool IBAN::operator!=(const IBAN& iban) const {
  return !operator==(iban);
}

void IBAN::set_nickname(const std::u16string& nickname) {
  // First replace all tabs and newlines with whitespaces and store it as
  // |nickname_|.
  base::ReplaceChars(nickname, u"\t\r\n", u" ", &nickname_);
  // An additional step to collapse whitespaces, this step does:
  // 1. Trim leading and trailing whitespaces.
  // 2. All other whitespace sequences are converted to a single space.
  nickname_ =
      base::CollapseWhitespace(nickname_,
                               /*trim_sequences_with_line_breaks=*/true);
}

std::u16string IBAN::GetIdentifierStringForAutofillDisplay() const {
  const std::u16string stripped_value = GetStrippedValue();
  size_t value_length = stripped_value.size();
  // Directly return an empty string if the length of IBAN value is invalid.
  if (value_length < 9 || value_length > 34)
    return std::u16string();

  auto ShouldObfuscate = [&](size_t i) {
    // The first 2-letter country code and 2 IBAN check digits will stay
    // unmasked, the last four digits will be shown as-is too.
    return 4 <= i && i < value_length - 4;
  };

  std::u16string output;
  output.reserve(stripped_value.size() + (stripped_value.size() - 1) / 4);
  for (size_t i = 0; i < stripped_value.size(); ++i) {
    if (i % 4 == 0 && i > 0)
      output.append(kEllipsisOneSpace);
    output.append(ShouldObfuscate(i) ? kEllipsisOneDot
                                     : stripped_value.substr(i, 1));
  }
  return output;
}

std::u16string IBAN::GetStrippedValue() const {
  std::u16string stripped_value;
  base::RemoveChars(value_, u"- ", &stripped_value);
  return stripped_value;
}

}  // namespace autofill
