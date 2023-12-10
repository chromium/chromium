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

// A form group that stores IBAN information.
class Iban : public AutofillDataModel {
 public:
  using Guid = base::StrongAlias<class GuidTag, std::string>;
  using InstrumentId = base::StrongAlias<class InstrumentIdTag, int64_t>;

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

  // Returns true if `country_code` is in the IBAN-supported country list.
  static bool IsIbanApplicableInCountry(const std::string& country_code);

  // AutofillDataModel:
  AutofillMetadata GetMetadata() const override;
  bool SetMetadata(const AutofillMetadata& metadata) override;

  std::u16string GetRawInfo(ServerFieldType type) const override;
  void SetRawInfoWithVerificationStatus(ServerFieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;

  // Returns true if there are no values (field types) set.
  bool IsEmpty(const std::string& app_locale) const;

  // Comparison for Sync. Returns 0 if |iban| is the same as this, or < 0,
  // or > 0 if it is different. The implied ordering can be used for culling
  // duplicates. The ordering is based on the collation order of the textual
  // contents of the fields.
  int Compare(const Iban& iban) const;

  // Equality operators compare GUIDs, origins, |value_| and |nickname_|.
  bool operator==(const Iban& iban) const;
  bool operator!=(const Iban& iban) const;

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
  int length() const { return length_; }
  void set_length(int length);

  // For local IBANs, checks on `IsValid(value_)`. Always returns true for
  // server-based IBANs because server-based IBANs don't store the full `value`.
  bool IsValid();

  // Construct an IBAN identifier from `prefix_`, `suffix_`, `length_` (and
  // `value_` if it's a local-based IBAN) by the following rules:
  // 1. Always reveal the first and the last four characters.
  // 2. Mask the remaining digits if `is_value_masked` is true, otherwise,
  //    display the digits. `is_value_masked` is true only for local IBANs
  //    because revealing a server IBAN needs an additional retrieval step from
  //    the GPay server.
  // 3. The identifier string will be arranged in groups of four with a space
  //    between each group.
  //
  // Note: The condition "is_value_masked" being false will not function
  //       properly for IBANs with the "kServerIban" record type.
  // Here are some examples:
  // BE71 0961 2345 6769 will be shown as: BE71 **** **** 6769.
  // CH56 0483 5012 3456 7800 9 will be shown as: CH56 **** **** **** *800 9.
  // DE91 1000 0000 0123 4567 89 will be shown as: DE91 **** **** **** **67 89.
  std::u16string GetIdentifierStringForAutofillDisplay(
      bool is_value_masked = true) const;

  // Returns a version of |value_| which does not have any separator characters
  // (e.g., '-' and ' ').
  // TODO(crbug.com/1422672): Cleanup and use value().
  std::u16string GetStrippedValue() const;

  // Returns true if the `prefix_`, `suffix_` and `length_` of the given `iban`
  // matches this IBAN.
  bool MatchesPrefixSuffixAndLength(const Iban& iban) const;

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
  // IBAN's value, respectively. `length_` is the length of the complete IBAN
  // value, ignoring spaces. These three fields are used when showing the IBAN
  // to the user in a masked format, where the prefix and suffix are shown
  // but all characters between them stay masked.
  std::u16string prefix_;
  std::u16string suffix_;
  int length_ = 0;
};

std::ostream& operator<<(std::ostream& os, const Iban& iban);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IBAN_H_
