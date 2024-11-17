// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/address_suggestion_strike_database.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_save_strike_database.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

// static
std::string AddressSuggestionStrikeDatabase::GetId(
    FormSignature form_signature,
    FieldSignature field_signature,
    const GURL& url) {
  return base::NumberToString(form_signature.value()) +
         base::NumberToString(field_signature.value()) + "-" + url.host();
}

}  // namespace autofill
