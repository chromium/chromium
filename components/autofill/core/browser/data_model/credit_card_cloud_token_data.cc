// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"

namespace autofill {

CreditCardCloudTokenData::CreditCardCloudTokenData() = default;

CreditCardCloudTokenData::CreditCardCloudTokenData(
    const CreditCardCloudTokenData& cloud_token_data) = default;

CreditCardCloudTokenData::~CreditCardCloudTokenData() = default;

bool CreditCardCloudTokenData::operator==(
    const CreditCardCloudTokenData& other_data) const {
  return Compare(other_data) == 0;
}

std::u16string CreditCardCloudTokenData::ExpirationMonthAsString() const {
  return data_util::Expiration2DigitMonthAsString(exp_month);
}

std::u16string CreditCardCloudTokenData::Expiration2DigitYearAsString() const {
  return data_util::Expiration2DigitYearAsString(exp_year);
}

std::u16string CreditCardCloudTokenData::Expiration4DigitYearAsString() const {
  return data_util::Expiration4DigitYearAsString(exp_year);
}

void CreditCardCloudTokenData::SetExpirationMonthFromString(
    const std::u16string& month) {
  data_util::ParseExpirationMonth(month, /*app_locale=*/std::string(),
                                  &exp_month);
}

void CreditCardCloudTokenData::SetExpirationYearFromString(
    const std::u16string& year) {
  data_util::ParseExpirationYear(year, &exp_year);
}

int CreditCardCloudTokenData::Compare(
    const CreditCardCloudTokenData& other_data) const {
  int comparison = masked_card_id.compare(other_data.masked_card_id);
  if (comparison != 0)
    return comparison;

  comparison = suffix.compare(other_data.suffix);
  if (comparison != 0)
    return comparison;

  comparison = Expiration2DigitYearAsString().compare(
      other_data.Expiration2DigitYearAsString());
  if (comparison != 0)
    return comparison;

  comparison =
      ExpirationMonthAsString().compare(other_data.ExpirationMonthAsString());
  if (comparison != 0)
    return comparison;

  comparison = card_art_url.compare(other_data.card_art_url);
  if (comparison != 0)
    return comparison;

  comparison = instrument_token.compare(other_data.instrument_token);
  return comparison;
}

}  // namespace autofill
