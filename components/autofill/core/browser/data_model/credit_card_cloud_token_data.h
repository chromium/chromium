// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_CLOUD_TOKEN_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_CLOUD_TOKEN_DATA_H_

#include <string>


namespace autofill {

// Represents all the cloud tokenization data related to the server credit card.
struct CreditCardCloudTokenData {
 public:
  CreditCardCloudTokenData();
  CreditCardCloudTokenData(const CreditCardCloudTokenData& cloud_token_data);
  ~CreditCardCloudTokenData();

  bool operator==(const CreditCardCloudTokenData&) const;

  std::u16string ExpirationMonthAsString() const;
  std::u16string Expiration2DigitYearAsString() const;
  std::u16string Expiration4DigitYearAsString() const;
  void SetExpirationMonthFromString(const std::u16string& month);
  void SetExpirationYearFromString(const std::u16string& year);

  // Used by Autofill Wallet sync bridge to compute the difference between two
  // CreditCardCloudTokenData.
  int Compare(const CreditCardCloudTokenData& cloud_token_data) const;

  // The id assigned by the server to uniquely identify this card.
  std::string masked_card_id;

  // The last 4-5 digits of the Cloud Primary Account Number (CPAN).
  std::u16string suffix;

  // The expiration month of the CPAN.
  int exp_month = 0;

  // The 4-digit expiration year of the CPAN.
  int exp_year = 0;

  // The URL of the card art to be displayed.
  std::string card_art_url;

  // The opaque identifier for the cloud token associated with the card.
  std::string instrument_token;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_CLOUD_TOKEN_DATA_H_
