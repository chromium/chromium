// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_H_

#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_model.h"

namespace autofill {

struct AutofillMetadata;

// A midline horizontal ellipsis (U+22EF).
extern const base::char16 kMidlineEllipsis[];

namespace internal {

// Returns an obfuscated representation of a credit card number given its last
// digits. To ensure that the obfuscation is placed at the left of the last four
// digits, even for RTL languages, inserts a Left-To-Right Embedding mark at the
// beginning and a Pop Directional Formatting mark at the end.
// Exposed for testing.
base::string16 GetObfuscatedStringForCardDigits(const base::string16& digits);

}  // namespace internal

// A form group that stores card information.
class CreditCard : public AutofillDataModel {
 public:
  enum RecordType {
    // A card with a complete number managed by Chrome (and not representing
    // something on the server).
    LOCAL_CARD,

    // A card from Wallet with masked information. Such cards will only have
    // the last 4 digits of the card number, and require an extra download to
    // convert to a FULL_SERVER_CARD.
    MASKED_SERVER_CARD,

    // A card from the Wallet server with full information. This card is not
    // locally editable.
    FULL_SERVER_CARD,
  };

  // The status of this card. Only used for server cards.
  enum ServerStatus {
    EXPIRED,
    OK,
  };

  // The type of the card. Local cards are all CARD_TYPE_UNKNOWN. Server cards
  // may have a more specific type.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.autofill
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: CardType
  enum CardType : int {
    CARD_TYPE_UNKNOWN,
    CARD_TYPE_CREDIT,
    CARD_TYPE_DEBIT,
    CARD_TYPE_PREPAID,
  };

  CreditCard(const std::string& guid, const std::string& origin);

  // Creates a server card.  The type must be MASKED_SERVER_CARD or
  // FULL_SERVER_CARD.
  CreditCard(RecordType type, const std::string& server_id);

  // For use in STL containers.
  CreditCard();
  CreditCard(const CreditCard& credit_card);
  ~CreditCard() override;

  // Returns a version of |number| that has any separator characters removed.
  static const base::string16 StripSeparators(const base::string16& number);

  // The user-visible issuer network of the card, e.g. 'Mastercard'.
  static base::string16 NetworkForDisplay(const std::string& network);

  // The ResourceBundle ID for the appropriate card issuer network image.
  static int IconResourceId(const std::string& network);

  // Returns the internal representation of card issuer network corresponding to
  // the given |number|.  The card issuer network is determined purely according
  // to the Issuer Identification Number (IIN), a.k.a. the "Bank Identification
  // Number (BIN)", which is parsed from the relevant prefix of the |number|.
  // This function performs no additional validation checks on the |number|.
  // Hence, the returned issuer network for both the valid card
  // "4111-1111-1111-1111" and the invalid card "4garbage" will be Visa, which
  // has an IIN of 4.
  static const char* GetCardNetwork(const base::string16& number);

  // Network issuer strings are defined at the bottom of this file, e.g.
  // kVisaCard.
  void SetNetworkForMaskedCard(base::StringPiece network);

  // Sets/gets the status of a server card.
  void SetServerStatus(ServerStatus status);
  ServerStatus GetServerStatus() const;

  // AutofillDataModel:
  AutofillMetadata GetMetadata() const override;
  bool SetMetadata(const AutofillMetadata metadata) override;

  // FormGroup:
  void GetMatchingTypes(const base::string16& text,
                        const std::string& app_locale,
                        ServerFieldTypeSet* matching_types) const override;
  base::string16 GetRawInfo(ServerFieldType type) const override;
  void SetRawInfo(ServerFieldType type, const base::string16& value) override;

  // Special method to set value for HTML5 month input type.
  void SetInfoForMonthInputType(const base::string16& value);

  const std::string& network() const { return network_; }

  const std::string& bank_name() const { return bank_name_; }
  void set_bank_name(const std::string& bank_name) { bank_name_ = bank_name; }

  int expiration_month() const { return expiration_month_; }
  int expiration_year() const { return expiration_year_; }

  // These setters verify that the month and year are within appropriate
  // ranges, or 0. They take integers as an alternative to setting the inputs
  // from strings via SetInfo().
  void SetExpirationMonth(int expiration_month);
  void SetExpirationYear(int expiration_year);

  const std::string& server_id() const { return server_id_; }
  void set_server_id(const std::string& server_id) { server_id_ = server_id; }

  // For use in STL containers.
  void operator=(const CreditCard& credit_card);

  // If the card numbers for |this| and |imported_card| match, and merging the
  // two wouldn't result in unverified data overwriting verified data,
  // overwrites |this| card's data with the data in |imported_card|. Returns
  // true if the card numbers match, false otherwise.
  bool UpdateFromImportedCard(const CreditCard& imported_card,
                              const std::string& app_locale) WARN_UNUSED_RESULT;

  // Comparison for Sync.  Returns 0 if the card is the same as |this|, or < 0,
  // or > 0 if it is different.  The implied ordering can be used for culling
  // duplicates.  The ordering is based on collation order of the textual
  // contents of the fields.
  // GUIDs, origins, labels, and unique IDs are not compared, only the values of
  // the cards themselves.
  int Compare(const CreditCard& credit_card) const;

  // Determines if |this| is a local version of the server card |other|.
  bool IsLocalDuplicateOfServerCard(const CreditCard& other) const;

  // Determines if |this| has the same number as |other|. If either is a masked
  // server card, compares the last four digits only.
  bool HasSameNumberAs(const CreditCard& other) const;

  // Equality operators compare GUIDs, origins, and the contents.
  // Usage metadata (use count, use date, modification date) are NOT compared.
  bool operator==(const CreditCard& credit_card) const;
  bool operator!=(const CreditCard& credit_card) const;

  // How this card is stored.
  RecordType record_type() const { return record_type_; }
  void set_record_type(RecordType rt) { record_type_ = rt; }

  // Whether this is a credit, debit, or prepaid card. Known only for server
  // cards. All local cards are CARD_TYPE_UNKNOWN.
  CardType card_type() const { return card_type_; }
  void set_card_type(CardType card_type) { card_type_ = card_type; }

  // Returns true if there are no values (field types) set.
  bool IsEmpty(const std::string& app_locale) const;

  // Returns true if credit card number is valid.
  // MASKED_SERVER_CARDs will never be valid because the number is
  // not complete.
  bool HasValidCardNumber() const;

  // Returns true if credit card has valid expiration date.
  bool HasValidExpirationDate() const;

  // Returns true if IsValidCardNumber && IsValidExpirationDate.
  bool IsValid() const;

  // Returns the card number.
  const base::string16& number() const { return number_; }
  // Sets |number_| to |number| and computes the appropriate card issuer
  // |network_|.
  void SetNumber(const base::string16& number);

  // Logs the number of days since the card was last used and records its use.
  void RecordAndLogUse();

  // Converts a string representation of a month (such as "February" or "feb."
  // or "2") into a numeric value in [1, 12]. Returns true on successful
  // conversion or false if a month was not recognized.
  static bool ConvertMonth(const base::string16& month,
                           const std::string& app_locale,
                           int* num);

  // Returns whether the card is expired based on |current_time|.
  bool IsExpired(const base::Time& current_time) const;

  // Whether the card expiration date should be updated.
  bool ShouldUpdateExpiration(const base::Time& current_time) const;

  const std::string& billing_address_id() const { return billing_address_id_; }
  void set_billing_address_id(const std::string& id) {
    billing_address_id_ = id;
  }

  // Sets |expiration_month_| to the integer conversion of |text| and returns
  // whether the operation was successful.
  bool SetExpirationMonthFromString(const base::string16& text,
                                    const std::string& app_locale);

  // Sets |expiration_year_| to the integer conversion of |text|. Will handle
  // 4-digit year or 2-digit year (eventually converted to 4-digit year).
  void SetExpirationYearFromString(const base::string16& text);

  // Sets |expiration_year_| and |expiration_month_| to the integer conversion
  // of |text|. Will handle mmyy, mmyyyy, mm-yyyy and mm-yy as well as single
  // digit months, with various separators.
  void SetExpirationDateFromString(const base::string16& text);

  // Various display functions.

  // Card preview summary, for example: "Visa - ****1234", ", 01/2020".
  const std::pair<base::string16, base::string16> LabelPieces() const;
  // Like LabelPieces, but appends the two pieces together.
  const base::string16 Label() const;
  // The last four digits of the card number (or possibly less if there aren't
  // enough characters).
  base::string16 LastFourDigits() const;
  // The user-visible issuer network of the card, e.g. 'Mastercard'.
  base::string16 NetworkForDisplay() const;
  // A label for this card formatted as '****2345'.
  base::string16 ObfuscatedLastFourDigits() const;
  // A label for this card formatted as 'IssuerNetwork - ****2345'.
  base::string16 NetworkAndLastFourDigits() const;
  // A label for this card formatted as 'BankName' - ****2345' if bank name
  // experiment turned on and bank name available; otherwise, formatted as
  // 'IssuerNetwork - ****2345'.
  base::string16 NetworkOrBankNameAndLastFourDigits() const;
  // A label for this card formatted as
  // 'BankName/Netowrk' - ****2345, expires on MM/YY' if bank name
  // experiment turned on and bank name available; otherwise, formatted as
  // 'IssuerNetwork - ****2345, expires on MM/YY'.
  // This label is used as a second line label when the autofill dropdown
  // layout experiment is enabled and the cardholder name is selected.
  base::string16 NetworkOrBankNameLastFourDigitsAndDescriptiveExpiration(
      const std::string& app_locale) const;
  // A label for this card formatted as 'Expires on MM/YY'.
  // This label is used as a second line label when the autofill dropdown
  // uses a two line layout and the credit card number is selected.
  base::string16 DescriptiveExpiration(const std::string& app_locale) const;

  // Localized expiration for this card formatted as 'Exp: 06/17' if with_prefix
  // is true or as '06/17' otherwise.
  base::string16 AbbreviatedExpirationDateForDisplay(bool with_prefix) const;
  // Formatted expiration date (e.g., 05/2020).
  base::string16 ExpirationDateForDisplay() const;
  // Expiration functions.
  base::string16 ExpirationMonthAsString() const;
  base::string16 Expiration4DigitYearAsString() const;

  // Whether the cardholder name was created from separate first name and last
  // name fields.
  bool HasFirstAndLastName() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(CreditCardTest, SetExpirationDateFromString);
  FRIEND_TEST_ALL_PREFIXES(CreditCardTest, SetExpirationYearFromString);
  FRIEND_TEST_ALL_PREFIXES(CreditCardTest, BankNameAndLastFourDigitsStrings);

  base::string16 Expiration2DigitYearAsString() const;

  // FormGroup:
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;
  base::string16 GetInfoImpl(const AutofillType& type,
                             const std::string& app_locale) const override;
  bool SetInfoImpl(const AutofillType& type,
                   const base::string16& value,
                   const std::string& app_locale) override;

  // The issuer network of the card to fill in to the page, e.g. 'Mastercard'.
  base::string16 NetworkForFill() const;

  // The month and year are zero if not present.
  int Expiration4DigitYear() const { return expiration_year_; }
  int Expiration2DigitYear() const { return expiration_year_ % 100; }

  // A label for this card formatted as 'BankName - 2345'.
  base::string16 BankNameAndLastFourDigits() const;

  // Sets the name_on_card_ value based on the saved name parts.
  void SetNameOnCardFromSeparateParts();

  // See enum definition above.
  RecordType record_type_;
  CardType card_type_;

  // The card number. For MASKED_SERVER_CARDs, this number will just contain the
  // last four digits of the card number.
  base::string16 number_;

  // The cardholder's name. May be empty.
  base::string16 name_on_card_;

  // The network issuer of the card. This is one of the k...Card constants
  // below.
  std::string network_;

  // The issuer bank name of the card.
  std::string bank_name_;

  // These members are zero if not present.
  int expiration_month_;
  int expiration_year_;

  // For server cards (both MASKED and UNMASKED) this is the ID assigned by the
  // server to uniquely identify this card.
  std::string server_id_;

  // The status of the card, as reported by the server. Not valid for local
  // cards.
  ServerStatus server_status_;

  // The identifier of the billing address for this card.
  std::string billing_address_id_;

  // The credit card holder's name parts. Used when creating a new card to hold
  // on to the value until the credit card holder's other name part is set,
  // since we only store the full name.
  base::string16 temp_card_first_name_;
  base::string16 temp_card_last_name_;
};

// So we can compare CreditCards with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const CreditCard& credit_card);

// The string identifiers for credit card icon resources.
extern const char kAmericanExpressCard[];
extern const char kDinersCard[];
extern const char kDiscoverCard[];
extern const char kEloCard[];
extern const char kGenericCard[];
extern const char kJCBCard[];
extern const char kMasterCard[];
extern const char kMirCard[];
extern const char kUnionPay[];
extern const char kVisaCard[];

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_H_
