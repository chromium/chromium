// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_H_

#include <iosfwd>
#include <string>
#include <string_view>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "url/gurl.h"

namespace autofill {

// Unicode characters used in card number obfuscation:
//  - \u2022 - Bullet.
//  - \u2006 - SIX-PER-EM SPACE (small space between bullets).
//  - \u2060 - WORD-JOINER (makes obfuscated string indivisible).
inline constexpr char16_t kMidlineEllipsisDot[] = u"\u2022\u2060\u2006\u2060";
inline constexpr char16_t kMidlineEllipsisPlainDot = u'\u2022';

struct PaymentsMetadata;

namespace internal {

// Returns an obfuscated representation of a credit card number given its last
// digits. To ensure that the obfuscation is placed at the left of the last four
// digits, even for RTL languages, inserts a Left-To-Right Embedding mark at the
// beginning and a Pop Directional Formatting mark at the end.
// `obfuscation_length` determines the number of dots to placed before the
// digits. Exposed for testing.
std::u16string GetObfuscatedStringForCardDigits(const std::u16string& digits,
                                                int obfuscation_length);

}  // namespace internal

// A form group that stores card information.
class CreditCard : public AutofillDataModel {
 public:
  enum class RecordType {
    // A card with a complete number managed by Chrome (and not representing
    // something on the server).
    kLocalCard,

    // A card from Wallet with masked information. Such cards only have the last
    // 4 digits of the card number, and require an extra download to fetch the
    // full number.
    kMaskedServerCard,

    // A cached form of kMaskedServerCard, with a full number. Historically
    // these could be persisted in Chrome, however that is no longer possible.
    // They exist only in an in-memory cache used for card filling, to avoid
    // another authentication when re-filling a card on the same page.
    //
    // TODO(crbug.com/40939195): Consolidate kMaskedServerCard and
    // kFullServerCard to a single RecordType, and have the cached/full-card
    // status for a given CreditCard be tracked independently.
    kFullServerCard,

    // A card generated from a server card by the card issuer. This card is not
    // persisted in Chrome.
    kVirtualCard,
  };

  // The Issuer for the card. This must stay in sync with the proto enum in
  // autofill_specifics.proto.
  enum class Issuer {
    kIssuerUnknown = 0,
    kGoogle = 1,
    kExternalIssuer = 2,
  };

  // Whether the card has been enrolled in the virtual card feature. This must
  // stay in sync with the proto enum in autofill_specifics.proto. A java
  // IntDef@ is generated from this.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
  enum class VirtualCardEnrollmentState {
    // State unspecified. This is the default value of this enum. Should not be
    // ever used with cards.
    kUnspecified = 0,
    // Deprecated. Card is not enrolled and does not have related virtual card.
    kUnenrolled = 1,
    // Card is enrolled and has related virtual cards.
    kEnrolled = 2,
    // Card is not enrolled and is not eligible for enrollment.
    kUnenrolledAndNotEligible = 3,
    // Card is not enrolled but is eligible for enrollment.
    kUnenrolledAndEligible = 4,
  };

  // The enrollment type of the virtual card attached to this card, if one is
  // present. This must stay in sync with the proto enum in
  // autofill_specifics.proto.
  enum class VirtualCardEnrollmentType {
    // Type unspecified. This is the default value of this enum. Should not be
    // used with cards that have a virtual card enrolled.
    kTypeUnspecified = 0,
    // Issuer-level enrollment.
    kIssuer = 1,
    // Network-level enrollment.
    kNetwork = 2,
  };

  // Creates a copy of the passed in credit card, and sets its `record_type` to
  // `CreditCard::RecordType::kVirtualCard`. This is used to differentiate
  // virtual cards from their real counterpart on the UI layer.
  static CreditCard CreateVirtualCard(const CreditCard& card);

  // Creates a copy of the passed in credit card, and sets its `record_type` to
  // `CreditCard::RecordType::kVirtualCard`. This is used to differentiate
  // virtual cards from their real counterpart on the UI layer. In addition, a
  // suffix is added to the guid which also helps differentiate the virtual card
  // from their real counterpart.
  static std::unique_ptr<CreditCard> CreateVirtualCardWithGuidSuffix(
      const CreditCard& card);

  // Generates a string of `obfuscation_length` bullets and appends `digits` to
  // it.
  static std::u16string GetObfuscatedStringForCardDigits(
      int obfuscation_length,
      const std::u16string& digits);

  CreditCard(const std::string& guid, const std::string& origin);

  // Creates a server card. The type must be RecordType::kMaskedServerCard or
  // RecordType::kFullServerCard.
  CreditCard(RecordType type, const std::string& server_id);

  // Creates a server card with non-legacy instrument id. The type must be
  // RecordType::kMaskedServerCard or RecordType::kFullServerCard.
  CreditCard(RecordType type, int64_t instrument_id);

  CreditCard();
  CreditCard(const CreditCard& credit_card);
  CreditCard(CreditCard&& credit_card);
  CreditCard& operator=(const CreditCard& credit_card);
  CreditCard& operator=(CreditCard&& credit_card);
  ~CreditCard() override;

  std::string guid() const { return guid_; }
  void set_guid(std::string_view guid) { guid_ = guid; }

  std::string origin() const { return origin_; }
  void set_origin(const std::string& origin) { origin_ = origin; }

  // The user-visible issuer network of the card, e.g. 'Mastercard'.
  static std::u16string NetworkForDisplay(const std::string& network);

  // The ResourceBundle ID for the appropriate card issuer icon.
  static int IconResourceId(Suggestion::Icon icon);

  // Converts icon_str to Suggestion::Icon and calls the method above.
  static int IconResourceId(std::string_view icon_str);

  // Returns whether the nickname is valid. Note that empty nicknames are valid
  // because they are not required.
  static bool IsNicknameValid(const std::u16string& nickname);

  // The first function returns dots that are each padded by whitespace while
  // the latter returns just a sequence of dots.
  static std::u16string GetMidlineEllipsisDots(size_t num_dots);
  static std::u16string GetMidlineEllipsisPlainDots(size_t num_dots);

  // Returns whether the card is a local card.
  static bool IsLocalCard(const CreditCard* card);

  // Network issuer strings are defined at the bottom of this file, e.g.
  // kVisaCard.
  void SetNetworkForMaskedCard(std::string_view network);

  PaymentsMetadata GetMetadata() const;
  bool SetMetadata(const PaymentsMetadata& metadata);

  // Returns whether the card is deletable: if it is expired and has not been
  // used for longer than `kDisusedDataModelDeletionTimeDelta`.
  bool IsDeletable() const;

  // FormGroup:
  void GetMatchingTypesWithProfileSources(
      const std::u16string& text,
      const std::string& app_locale,
      FieldTypeSet* matching_types,
      PossibleProfileValueSources* profile_value_sources) const override;
  std::u16string GetRawInfo(FieldType type) const override;
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;

  // Special method to set value for HTML5 month input type.
  void SetInfoForMonthInputType(const std::u16string& value);

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

  const std::u16string& nickname() const { return nickname_; }

  int64_t instrument_id() const { return instrument_id_; }
  void set_instrument_id(int64_t instrument_id) {
    instrument_id_ = instrument_id;
  }

  // Set the nickname with the processed input (replace all tabs and newlines
  // with whitespaces, and trim leading/trailing whitespaces).
  void SetNickname(const std::u16string& nickname);

  Issuer card_issuer() const { return card_issuer_; }
  void set_card_issuer(Issuer card_issuer) { card_issuer_ = card_issuer; }
  const std::string& issuer_id() const { return issuer_id_; }
  void set_issuer_id(std::string_view issuer_id) {
    issuer_id_ = std::string(issuer_id);
  }

  // If the card numbers for |this| and |imported_card| match, and merging the
  // two wouldn't result in unverified data overwriting verified data,
  // overwrites |this| card's data with the data in |imported_card|. Returns
  // true if the card numbers match, false otherwise.
  [[nodiscard]] bool UpdateFromImportedCard(const CreditCard& imported_card,
                                            const std::string& app_locale);

  // Comparison for Sync.  Returns 0 if the card is the same as |this|, or < 0,
  // or > 0 if it is different.  The implied ordering can be used for culling
  // duplicates.  The ordering is based on collation order of the textual
  // contents of the fields.
  // GUIDs, origins, labels, and unique IDs are not compared, only the values of
  // the cards themselves. A full card is equivalent to its corresponding masked
  // card.
  [[nodiscard]] int Compare(const CreditCard& credit_card) const;

  // Determines if `this` and `other` are likely duplicates of each other (name,
  // expiration date, cc number, billing address match each other if they are
  // defined) but one card is a local card and the other is a server card.
  [[nodiscard]] bool IsLocalOrServerDuplicateOf(const CreditCard& other) const;

  // Determines if `this` is the matching card as `other` (same card number and
  // expiration date). If either is a masked server card, compares their last
  // four digits and expiration dates.
  [[nodiscard]] bool MatchingCardDetails(const CreditCard& other) const;

  // Returns true based on the following criteria:
  // 1) If `this` or `other` is a masked server card, this function returns true
  //    if `other` has the same last four digits as `this`.
  // 2) Otherwise, this function returns true if `other` has the same full card
  //    number as `this`.
  [[nodiscard]] bool HasSameNumberAs(const CreditCard& other) const;

  // Returns true if expiration date for `this` card is the same as `other`.
  [[nodiscard]] bool HasSameExpirationDateAs(const CreditCard& other) const;

  // Calculates the ranking score used for ranking the card suggestion. If
  // `use_frecency` is true we use the new ranking algorithm.
  double GetRankingScore(base::Time current_time,
                         bool use_frecency = false) const;

  // Compares two credit cards and returns if the current card has a greater
  // ranking score than `other`.
  bool HasGreaterRankingThan(const CreditCard& other,
                             base::Time comparison_time,
                             bool use_frecency = false) const;

  // Equality operators compare GUIDs, origins, and the contents.
  // Usage metadata (use count, use date, modification date) are NOT compared.
  bool operator==(const CreditCard& credit_card) const;

  // Returns true if the data in this model was entered directly by the user,
  // rather than automatically aggregated.
  bool IsVerified() const;

  // How this card is stored.
  RecordType record_type() const { return record_type_; }
  void set_record_type(RecordType rt) { record_type_ = rt; }

  // Returns true if there are no values (field types) set.
  bool IsEmpty(const std::string& app_locale) const;

  // Returns true if credit card number is valid.
  // MASKED_SERVER_CARDs will never be valid because the number is
  // not complete.
  bool HasValidCardNumber() const;

  // Returns true if credit card has valid expiration year.
  bool HasValidExpirationYear() const;

  // Returns true if credit card has valid expiration date.
  bool HasValidExpirationDate() const;

  // Returns true if IsValidCardNumber && IsValidExpirationDate.
  bool IsValid() const;

  // Returns the card number.
  const std::u16string& number() const { return number_; }
  // Sets |number_| to |number| and computes the appropriate card issuer
  // |network_|.
  void SetNumber(const std::u16string& number);

  // Logs the number of days since the card was last used and records its use.
  void RecordAndLogUse();

  // Returns whether the card is expired based on |current_time|.
  bool IsExpired(base::Time current_time) const;

  // Returns whether the card is a masked card. Such cards will only have
  // the last 4 digits of the card number.
  bool masked() const;

  // Whether the card expiration date should be updated.
  bool ShouldUpdateExpiration() const;

  // Complete = contains number, expiration date and name on card.
  // Valid = unexpired with valid number format.
  bool IsCompleteValidCard() const;

  const std::string& billing_address_id() const { return billing_address_id_; }
  void set_billing_address_id(const std::string& id) {
    billing_address_id_ = id;
  }

  // Sets |expiration_month_| to the integer conversion of |text| and returns
  // whether the operation was successful.
  bool SetExpirationMonthFromString(const std::u16string& text,
                                    const std::string& app_locale);

  // Sets |expiration_year_| to the integer conversion of |text|. Will handle
  // 4-digit year or 2-digit year (eventually converted to 4-digit year).
  // Returns whether the operation was successful.
  bool SetExpirationYearFromString(const std::u16string& text);

  // Sets |expiration_year_| and |expiration_month_| to the integer conversion
  // of |text|. Will handle mmyy, mmyyyy, mm-yyyy and mm-yy as well as single
  // digit months, with various separators.
  void SetExpirationDateFromString(const std::u16string& text);

  // Various display functions.

  // Card preview summary, for example: "Nickname/Network - ****1234 John
  // Smith".
  std::pair<std::u16string, std::u16string> LabelPieces() const;
  // Like LabelPieces, but appends the two pieces together.
  std::u16string Label() const;
  // The last four digits of the card number (or possibly less if there aren't
  // enough characters).
  std::u16string LastFourDigits() const;
  // The well-formatted full digits for display, we will add white space as
  // separator between digits, e.g. "1234 5678 9000 0000".
  std::u16string FullDigitsForDisplay() const;
  // The user-visible issuer network of the card, e.g. 'Mastercard'.
  std::u16string NetworkForDisplay() const;
  // A label for this card formatted as '••••2345' where the number of dots are
  // specified by the `obfuscation_length`.
  std::u16string ObfuscatedNumberWithVisibleLastFourDigits(
      int obfuscation_length = 4) const;
  // A label for this card formatted '••••••••••••2345' where every digit in the
  // the credit card number is obfuscated except for the last four. This method
  // is primarily used for splitting the preview of a credit card number into
  // several fields.
  std::u16string ObfuscatedNumberWithVisibleLastFourDigitsForSplitFields()
      const;

  // The icon to be used for the autofill suggestion. For example, icon for:
  // visa, american express, etc.
  Suggestion::Icon CardIconForAutofillSuggestion() const;

  // A label for this card formatted as 'IssuerNetwork ****2345'. By default,
  // the `obfuscation_length` is set to 4 which would add **** to the last four
  // digits of the card.
  std::u16string NetworkAndLastFourDigits(int obfuscation_length = 4) const;
  // A label for this card formatted as 'CardName ****2345', where the name is
  // that returned by |CardNameForAutofillDisplay|. If the last four digits are
  // unavailable returns just the card name, and vice-versa.
  std::u16string CardNameAndLastFourDigits(
      std::u16string customized_nickname = std::u16string(),
      int obfuscation_length = 4) const;
  // A name to identify this card. It is the nickname if available, or the
  // product description. If neither are available, it falls back to the issuer
  // network.
  std::u16string CardNameForAutofillDisplay(
      const std::u16string& customized_nickname = std::u16string()) const;

#if BUILDFLAG(IS_ANDROID)
  // Label for the card to be displayed in the manual filling view on Android.
  std::u16string CardIdentifierStringForManualFilling() const;
#endif  // BUILDFLAG(IS_ANDROID)

  // A label for this card formatted as 'Nickname - ****2345, expires on MM/YY'
  // if nickname experiment is turned on and nickname is available; otherwise,
  // formatted as 'IssuerNetwork - ****2345, expires on MM/YY'.
  // This label is used as a second line label when the cardholder
  // name/expiration date field is selected.
  std::u16string CardIdentifierStringAndDescriptiveExpiration(
      const std::string& app_locale,
      std::u16string customized_nickname = std::u16string()) const;
  // A label for this card formatted as 'Expires on MM/YY'.
  // This label is used as a second line label when the autofill dropdown
  // uses a two line layout and the credit card number is selected.
  std::u16string DescriptiveExpiration(const std::string& app_locale) const;

  // Localized expiration for this card formatted as 'Exp: 06/17' if with_prefix
  // is true or as '06/17' otherwise.
  std::u16string AbbreviatedExpirationDateForDisplay(bool with_prefix) const;
  // Formatted expiration date (e.g., 05/2020).
  std::u16string ExpirationDateForDisplay() const;
  // Expiration functions.
  std::u16string Expiration2DigitMonthAsString() const;
  std::u16string Expiration2DigitYearAsString() const;
  std::u16string Expiration4DigitYearAsString() const;

  // Returns whether the card has a cardholder name.
  bool HasNameOnCard() const;

  // Returns whether the card has a non-empty nickname that also
  // passes |IsNicknameValid| checks.
  bool HasNonEmptyValidNickname() const;

  // Should be used ONLY by tests.
  std::u16string NicknameAndLastFourDigitsForTesting() const;

  VirtualCardEnrollmentState virtual_card_enrollment_state() const {
    return virtual_card_enrollment_state_;
  }
  void set_virtual_card_enrollment_state(
      VirtualCardEnrollmentState virtual_card_enrollment_state) {
    virtual_card_enrollment_state_ = virtual_card_enrollment_state;
  }

  VirtualCardEnrollmentType virtual_card_enrollment_type() const {
    return virtual_card_enrollment_type_;
  }
  void set_virtual_card_enrollment_type(
      VirtualCardEnrollmentType virtual_card_enrollment_type) {
    virtual_card_enrollment_type_ = virtual_card_enrollment_type;
  }

  const GURL& card_art_url() const { return card_art_url_; }
  void set_card_art_url(const GURL& card_art_url) {
    card_art_url_ = card_art_url;
  }

  // Returns true when the card has rich card art, excluding any static card art
  // image.
  bool HasRichCardArtImageFromMetadata() const;

  const std::u16string& product_description() const {
    return product_description_;
  }
  void set_product_description(const std::u16string& product_description) {
    product_description_ = product_description;
  }

  const GURL& product_terms_url() const { return product_terms_url_; }
  void set_product_terms_url(const GURL& product_terms_url) {
    product_terms_url_ = product_terms_url;
  }

  const std::u16string& cvc() const { return cvc_; }
  void clear_cvc() { cvc_.clear(); }
  void set_cvc(const std::u16string& cvc) { cvc_ = cvc; }

  base::Time cvc_modification_date() const { return cvc_modification_date_; }
  void set_cvc_modification_date(base::Time date) {
    cvc_modification_date_ = date;
  }

 private:
  friend class CreditCardTestApi;

  FRIEND_TEST_ALL_PREFIXES(CreditCardTest, SetExpirationDateFromString);
  FRIEND_TEST_ALL_PREFIXES(CreditCardTest, SetExpirationYearFromString);

  // FormGroup:
  void GetSupportedTypes(FieldTypeSet* supported_types) const override;
  std::u16string GetInfoImpl(const AutofillType& type,
                             const std::string& app_locale) const override;
  bool SetInfoWithVerificationStatusImpl(const AutofillType& type,
                                         const std::u16string& value,
                                         const std::string& app_locale,
                                         VerificationStatus status) override;

  // The issuer network of the card to fill in to the page, e.g. 'Mastercard'.
  std::u16string NetworkForFill() const;

  // A label for this card formatted as 'Nickname - ****2345'. Always call
  // HasNonEmptyValidNickname() before calling this. By default,
  // the `obfuscation_length` is set to 4 which would add **** to the last four
  // digits of the card.
  std::u16string NicknameAndLastFourDigits(
      std::u16string customized_nickname = std::u16string(),
      int obfuscation_length = 4) const;

  // Sets the name_on_card_ value based on the saved name parts.
  void SetNameOnCardFromSeparateParts();

  // A unique identifier for cards of `record_type()` `kLocalCard`. For them,
  // the `guid_` identifies the card across browser restarts and is used as the
  // primary key in the database.
  // For server cards, see `server_id_` and `instrument_id_`. Unfortunately,
  // some dependencies around `guid_` for server cards exist. See the server_id
  // constructor of `CreditCard()`. Notably, for server cards the `guid_` is
  // not persisted and should not be used.
  // TODO(crbug.com/40146355): Create a variant of the different ids, since
  // only one of them should be populated based on the `record_type()`.
  std::string guid_;

  // The origin of this data.  This should be
  //   (a) a web URL for the domain of the form from which the data was
  //       automatically aggregated, e.g. https://www.example.com/register,
  //   (b) some other non-empty string, which cannot be interpreted as a web
  //       URL, identifying the origin for non-aggregated data, or
  //   (c) an empty string, indicating that the origin for this data is unknown.
  std::string origin_;

  // See enum definition above.
  RecordType record_type_;

  // The card number. For MASKED_SERVER_CARDs, this number will just contain the
  // last four digits of the card number.
  std::u16string number_;

  // The cardholder's name. May be empty.
  std::u16string name_on_card_;

  // The network issuer of the card. This is one of the k...Card constants
  // below.
  std::string network_;

  // bank_name is no longer actively used but remains for legacy reasons.
  // The issuer bank name of the card.
  std::string bank_name_;

  // These members are zero if not present.
  int expiration_month_;
  int expiration_year_;

  // For server cards (both MASKED and UNMASKED) this is the ID assigned by the
  // server to uniquely identify this card.
  std::string server_id_;

  // The identifier of the billing address for this card.
  std::string billing_address_id_;

  // The credit card holder's name parts. Used when creating a new card to hold
  // on to the value until the credit card holder's other name part is set,
  // since we only store the full name.
  std::u16string temp_card_first_name_;
  std::u16string temp_card_last_name_;

  // The nickname of the card. May be empty when nickname is not set.
  std::u16string nickname_;

  // TODO(crbug.com/40248631): Consider removing this field and all its usage
  // after `issuer_id_` is used.
  // The issuer for the card. This is populated from the sync response. It has a
  // default value of CreditCard::Issuer::kIssuerUnknown.
  Issuer card_issuer_;

  // The issuer id of the card. This is set for server cards only (both actual
  // cards and virtual cards).
  std::string issuer_id_;

  // For masked server cards, this is the ID assigned by the server to uniquely
  // identify this card. |server_id_| is the legacy version of this.
  // TODO(crbug.com/40146355): remove server_id_ after full deprecation
  int64_t instrument_id_;

  // The virtual card enrollment state of this card. If it is kEnrolled, then
  // this card has virtual cards linked to it.
  VirtualCardEnrollmentState virtual_card_enrollment_state_ =
      VirtualCardEnrollmentState::kUnspecified;

  // The virtual card enrollment type of this card. This will be used when the
  // enrollment type can make a difference in the functionality we offer for
  // virtual cards. An example of differing functionality is if this virtual
  // card enrollment type is a network-level enrollment, and we are on a URL
  // that is opted out of virtual cards with the network of this card.
  VirtualCardEnrollmentType virtual_card_enrollment_type_ =
      VirtualCardEnrollmentType::kTypeUnspecified;

  // The URL to fetch the rich card art image.
  GURL card_art_url_;

  // The product description for the card to be used in the UI when card is
  // presented.
  std::u16string product_description_;

  // The URL for issuer terms of service to be displayed on the settings
  // page.
  GURL product_terms_url_;

  // The card verification code of the card. May be empty.
  std::u16string cvc_;

  // CVCs can be updated independently of the card and track their modification
  // date independently. The timestamp `is_null()` for cards without CVC.
  base::Time cvc_modification_date_;
};

// So we can compare CreditCards with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const CreditCard& credit_card);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_H_
