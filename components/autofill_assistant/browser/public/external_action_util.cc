// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/external_action_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"

namespace autofill_assistant {

external::ProfileProto CreateProfileProto(
    const autofill::AutofillProfile& autofill_profile) {
  external::ProfileProto profile_proto;

  autofill::ServerFieldTypeSet types;
  autofill_profile.GetNonEmptyRawTypes(&types);

  for (autofill::ServerFieldType type : types) {
    (*profile_proto.mutable_data()->mutable_values())[type] =
        base::UTF16ToUTF8(autofill_profile.GetRawInfo(type));
    (*profile_proto.mutable_data()->mutable_verification_statuses())[type] =
        autofill_profile.GetVerificationStatusInt(type);
  }

  profile_proto.mutable_data()->set_guid(autofill_profile.guid());
  profile_proto.mutable_data()->set_origin(autofill_profile.origin());

  return profile_proto;
}

external::CreditCardProto CreateCreditCardProto(
    const autofill::CreditCard& credit_card) {
  external::CreditCardProto card_proto;

  autofill::ServerFieldTypeSet types;
  credit_card.GetNonEmptyRawTypes(&types);

  for (autofill::ServerFieldType type : types) {
    (*card_proto.mutable_data()->mutable_values())[type] =
        base::UTF16ToUTF8(credit_card.GetRawInfo(type));
    (*card_proto.mutable_data()->mutable_verification_statuses())[type] =
        credit_card.GetVerificationStatusInt(type);
  }

  card_proto.mutable_data()->set_guid(credit_card.guid());
  card_proto.mutable_data()->set_origin(credit_card.origin());
  card_proto.set_record_type(credit_card.record_type());
  card_proto.set_instrument_id(credit_card.instrument_id());

  if (!credit_card.network().empty()) {
    card_proto.set_network(credit_card.network());
  }

  if (!credit_card.server_id().empty()) {
    card_proto.set_server_id(credit_card.server_id());
  }

  return card_proto;
}

bool IsCompleteAddressProfile(const autofill::AutofillProfile* profile,
                              const std::string& app_locale) {
  std::string country_code =
      base::UTF16ToASCII(profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  if (country_code.empty()) {
    return false;
  }

  autofill::AutofillCountry country(country_code, app_locale);
  return !profile->GetInfo(autofill::NAME_FULL, app_locale).empty() &&
         !profile->GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS).empty() &&
         (!country.requires_zip() ||
          profile->HasRawInfo(autofill::ADDRESS_HOME_ZIP)) &&
         !profile->GetRawInfo(autofill::EMAIL_ADDRESS).empty() &&
         !profile->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER).empty();
}

}  // namespace autofill_assistant
