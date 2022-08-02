// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/iban.h"

#include <string>

#include "base/guid.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"

namespace autofill {

IBAN::IBAN(const std::string& guid)
    : AutofillDataModel(guid, /*origin=*/std::string()),
      record_type_(LOCAL_IBAN) {}

IBAN::IBAN() : IBAN(base::GenerateGUID()) {}

IBAN::IBAN(const IBAN& iban) : IBAN() {
  operator=(iban);
}

IBAN::~IBAN() = default;

void IBAN::operator=(const IBAN& iban) {
  set_use_count(iban.use_count());
  set_use_date(iban.use_date());

  // Just overwrite use_count and use_date fields as those fields will
  // not be compared for == operator.
  if (this == &iban) {
    return;
  }

  set_guid(iban.guid());

  server_id_ = iban.server_id_;
  record_type_ = iban.record_type_;
  value_ = iban.value_;
  nickname_ = iban.nickname_;
}

AutofillMetadata IBAN::GetMetadata() const {
  AutofillMetadata metadata = AutofillDataModel::GetMetadata();
  metadata.id = (record_type_ == LOCAL_IBAN ? guid() : server_id_);
  return metadata;
}

bool IBAN::SetMetadata(const AutofillMetadata metadata) {
  // Make sure the ids match.
  return ((metadata.id !=
           (record_type_ == LOCAL_IBAN ? guid() : server_id_))) &&
         AutofillDataModel::SetMetadata(metadata);
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

void IBAN::SetRawInfoWithVerificationStatus(
    ServerFieldType type,
    const std::u16string& value,
    structured_address::VerificationStatus status) {
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
  return guid() == iban.guid() && record_type() == iban.record_type() &&
         Compare(iban) == 0;
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

}  // namespace autofill
