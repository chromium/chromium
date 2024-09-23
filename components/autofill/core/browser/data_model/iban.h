// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IBAN_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IBAN_H_

#include <string>
#include <string_view>

#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {

struct PaymentsMetadata;

// A form group that stores IBAN information.
class Iban : public AutofillDataModel {
 public:
  using Guid = base::StrongAlias<class GuidTag, std::string>;
  using InstrumentId = base::StrongAlias<class InstrumentIdTag, int64_t>;

  // A java IntDef@ is generated from this.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: IbanRecordType
  enum RecordType {
    // An IBAN extracted from a submitted form, whose record type is currently
    // unknown or irrelevant.
    kUnknown,

    // An IBAN with a complete value managed by Chrome (not representing
    // something stored in Google Payments). The local IBAN will only be stored
    // on this device.
    kLocalIban,

    // An IBAN from Google Payments with masked information. Such IBANs will
    // only have the first and last four characters of the IBAN, along with
    // formatted dots and spaces, and will require an extra retrieval step from
    // the GPay server to view. The server IBAN will be synced across all
    // devices.
    kServerIban,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class IbanSupportedCountry {
    kUnsupported = 0,
    kAD = 1,   // Andorra
    kAE = 2,   // United Arab Emirates
    kAL = 3,   // Albania
    kAT = 4,   // Austria
    kAZ = 5,   // Azerbaijan
    kBA = 6,   // Bosnia and Herzegovina
    kBE = 7,   // Belgium
    kBG = 8,   // Bulgaria
    kBH = 9,   // Bahrain
    kBR = 10,  // Brazil
    kBY = 11,  // Belarus
    kCH = 12,  // Switzerland
    kCR = 13,  // Costa Rica
    kCY = 14,  // Cyprus
    kCZ = 15,  // Czech Republic
    kDE = 16,  // Germany
    kDK = 17,  // Denmark
    kDO = 18,  // Dominican Republic
    kEE = 19,  // Estonia
    kEG = 20,  // Egypt
    kES = 21,  // Spain
    kFI = 22,  // Finland
    kFO = 23,  // Faroe Islands
    kFR = 24,  // France
    kGB = 25,  // United Kingdom
    kGE = 26,  // Georgia
    kGI = 27,  // Gibraltar
    kGL = 28,  // Greenland
    kGR = 29,  // Greece
    kGT = 30,  // Guatemala
    kHR = 31,  // Croatia
    kHU = 32,  // Hungary
    kIL = 33,  // Israel
    kIQ = 34,  // Iraq
    kIS = 35,  // Iceland
    kIT = 36,  // Italy
    kJO = 37,  // Jordan
    kKW = 38,  // Kuwait
    kKZ = 39,  // Kazakhstan
    kLB = 40,  // Lebanon
    kLC = 41,  // Saint Lucia
    kLI = 42,  // Liechtenstein
    kLT = 43,  // Lithuania
    kLU = 44,  // Luxembourg
    kLV = 45,  // Latvia
    kLY = 46,  // Libya
    kMC = 47,  // Monaco
    kMD = 48,  // Moldova
    kME = 49,  // Montenegro
    kMK = 50,  // North Macedonia
    kMR = 51,  // Mauritania
    kMT = 52,  // Malta
    kMU = 53,  // Mauritius
    kNL = 54,  // Netherlands
    kPK = 55,  // Pakistan
    kPL = 56,  // Poland
    kPS = 57,  // Palestinian territories
    kPT = 58,  // Portugal
    kQA = 59,  // Qatar
    kRO = 60,  // Romania
    kRS = 61,  // Serbia
    kRU = 62,  // Russia
    kSA = 63,  // Saudi Arabia
    kSC = 64,  // Seychelles
    kSD = 65,  // Sudan
    kSE = 66,  // Sweden
    kSI = 67,  // Slovenia
    kSK = 68,  // Slovakia
    kSM = 69,  // San Marino
    kST = 70,  // São Tomé and Príncipe
    kSV = 71,  // El Salvador
    kTL = 72,  // East Timor
    kTN = 73,  // Tunisia
    kTR = 74,  // Turkey
    kUA = 75,  // Ukraine
    kVA = 76,  // Vatican City
    kVG = 77,  // Virgin Islands, British
    kXK = 78,  // Kosovo
    kMaxValue = kXK,
  };

  // Creates an IBAN with `kUnknown` record type.
  Iban();

  // Creates a local IBAN with the given `guid`.
  explicit Iban(const Guid& guid);

  // Creates a server IBAN with the given `instrument_id`.
  explicit Iban(const InstrumentId& instrument_id);

  Iban(const Iban&);
  ~Iban() override;

  Iban& operator=(const Iban& iban);

  // Returns true if IBAN value is valid. This method is case-insensitive.
  // The validation follows the below steps:
  // 1. The IBAN consists of 16 to 33 alphanumeric characters, the first two
  //    letters are country code.
  // 2. Check that the total IBAN length is correct as per the country.
  // 3. Move the four initial characters to the end of the string and replace
  //    each letter in the rearranged string with two digits, thereby expanding
  //    the string, where 'A' = 10, 'B' = 11, ..., 'Z' = 35.
  // 4. Interpret the string as a decimal integer and compute the remainder of
  //    the number on division by 97, returning true if the remainder is 1.
  //
  // The validation algorithm is from:
  // https://en.wikipedia.org/wiki/International_Bank_Account_Number#Algorithms
  static bool IsValid(const std::u16string& value);

  // Returns the capitalized country code of the given `iban_value`.
  static std::string GetCountryCode(const std::u16string& iban_value);

  // Returns true if `country_code` is in the IBAN-supported country list.
  static bool IsIbanApplicableInCountry(const std::string& country_code);

  static IbanSupportedCountry GetIbanSupportedCountry(
      std::string_view country_code);

  static size_t GetLengthOfIbanCountry(IbanSupportedCountry supported_country);

  PaymentsMetadata GetMetadata() const;
  bool SetMetadata(const PaymentsMetadata& metadata);

  std::u16string GetRawInfo(FieldType type) const override;
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;
  void GetSupportedTypes(FieldTypeSet* supported_types) const override;

  // Returns true if there are no values (field types) set.
  bool IsEmpty(const std::string& app_locale) const;

  // Returns 0 if `iban` is the same as this, or < 0, or > 0 if it is different.
  // It compares `identifier_`, `value_`, `prefix_`, `suffix_`, `nickname_`,
  // `length_` and `record_type_`. The implied ordering can be used for culling
  // duplicates. The ordering is based on the collation order of the textual
  // contents of the fields.
  int Compare(const Iban& iban) const;

  // Equality operators call `Compare()` above.
  bool operator==(const Iban& iban) const;

  void set_identifier(const absl::variant<Guid, InstrumentId>& identifier);

  const std::string& guid() const;
  int64_t instrument_id() const;

  // Returns the value (the actual bank account number) of IBAN.
  const std::u16string& value() const { return value_; }
  void set_value(const std::u16string& value);

  const std::u16string& nickname() const { return nickname_; }
  // Set the |nickname_| with the processed input (replace all tabs and newlines
  // with whitespaces, condense multiple whitespaces into a single one, and
  // trim leading/trailing whitespaces).
  void set_nickname(const std::u16string& nickname);

  // Setters and getters for the type of this IBAN. The different types of IBAN
  // differ in how they are stored, retrieved, and displayed to the user.
  RecordType record_type() const { return record_type_; }
  void set_record_type(RecordType record_type) { record_type_ = record_type; }

  const std::u16string& prefix() const { return prefix_; }
  void set_prefix(std::u16string prefix);
  const std::u16string& suffix() const { return suffix_; }
  void set_suffix(std::u16string suffix);

  // For local IBANs, checks on `IsValid(value_)`. Always returns true for
  // server-based IBANs because server-based IBANs don't store the full `value`.
  bool IsValid();

  // Returns the capitalized country code of this IBAN.
  std::string GetCountryCode() const;

  // Logs the number of days since this IBAN was last used, increments its use
  // count, and updates its last used date to today.
  void RecordAndLogUse();

  // Construct an IBAN identifier from `prefix_` and `suffix_`.
  // If `is_value_masked` is true, the identifier is constructed by
  // first 2 country code (prefix) + space + 2 masking dots + suffix.
  // Here are some examples:
  // BE71 0961 2345 6769 will be shown as: BE **6769.
  // CH56 0483 5012 3456 7800 9 will be shown as: CH **8009.
  // DE91 1000 0000 0123 4567 89 will be shown as: DE **6789.
  // Otherwise, the full unmasked value is shown in groups of four characters.
  std::u16string GetIdentifierStringForAutofillDisplay(
      bool is_value_masked = true) const;

  // Returns true if the `prefix_` and `suffix_` of the given `iban` matches
  // this IBAN.
  bool MatchesPrefixAndSuffix(const Iban& iban) const;

 private:
  // To distinguish between local IBANs, utilize the Guid as the identifier. For
  // server-based IBANs, they are uniquely identified by the InstrumentId, a
  // unique identifier assigned by the server.
  absl::variant<Guid, InstrumentId> identifier_;

  // The IBAN's value. For local IBANs, this value is the actual full IBAN
  // value. For server-based IBANs, this value is empty.
  std::u16string value_;

  // The nickname of the IBAN. May be empty.
  std::u16string nickname_;

  RecordType record_type_;

  // `prefix_` and `suffix_` are the beginning and ending characters of the
  // IBAN's value, respectively. These two fields are used when showing the IBAN
  // to the user in a masked format, where the prefix and suffix are shown
  // but all characters between them stay masked.
  std::u16string prefix_;
  std::u16string suffix_;
};

std::ostream& operator<<(std::ostream& os, const Iban& iban);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IBAN_H_
