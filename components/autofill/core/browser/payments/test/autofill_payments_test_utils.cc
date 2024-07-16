// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test/autofill_payments_test_utils.h"

#include <string_view>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill::payments {

namespace {

AutofillProfile BuildProfile(std::string_view first_name,
                             std::string_view last_name,
                             std::string_view address_line,
                             std::string_view city,
                             std::string_view state,
                             std::string_view zip,
                             std::string_view phone_number) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);

  profile.SetInfo(NAME_FIRST, base::ASCIIToUTF16(first_name), "en-US");
  profile.SetInfo(NAME_LAST, base::ASCIIToUTF16(last_name), "en-US");
  profile.SetInfo(ADDRESS_HOME_LINE1, base::ASCIIToUTF16(address_line),
                  "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, base::ASCIIToUTF16(city), "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, base::ASCIIToUTF16(state), "en-US");
  profile.SetInfo(ADDRESS_HOME_ZIP, base::ASCIIToUTF16(zip), "en-US");
  profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, base::ASCIIToUTF16(phone_number),
                  "en-US");
  profile.FinalizeAfterImport();
  return profile;
}

}  // namespace

std::vector<AutofillProfile> BuildTestProfiles() {
  std::vector<AutofillProfile> profiles;
  profiles.push_back(BuildProfile("John", "Smith", "1234 Main St.", "Miami",
                                  "FL", "32006", "212-555-0162"));
  profiles.push_back(BuildProfile("Pat", "Jones", "432 Oak Lane", "Lincoln",
                                  "OH", "43005", "(834)555-0090"));
  return profiles;
}

}  // namespace autofill::payments
