// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_AUTOFILL_COUNTRY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_AUTOFILL_COUNTRY_H_

#include <string>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/common/autofill_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"

namespace autofill {

class LogBuffer;

// Stores data associated with a country. Strings are localized to the app
// locale.
class AutofillCountry {
 public:
  // Returns country data corresponding to the two-letter ISO code
  // `country_code`.
  // `locale` is used translate the `name()` appropriately and can be ignored
  // if the name is not queried.
  explicit AutofillCountry(
      const std::string& country_code,
      const absl::optional<std::string>& locale = absl::nullopt);

  AutofillCountry(const AutofillCountry&) = delete;
  AutofillCountry& operator=(const AutofillCountry&) = delete;

  ~AutofillCountry();

  // Autofill relies on libaddressinput for its address format.
  // AddressFormatExtensions are used to extend this format on a country-by-
  // country basis. This is needed because while some field types are not
  // strictly required for a valid address, we nonetheless see them in practice
  // and want to offer filling support.
  // This struct defines that a certain `type` is considered part of the address
  // format in Autofill, specifies its `label` and placement after the existing
  // type `placed_after` in the settings-UI.
  // `large_sized` indicates if the field stretches the entire line (true) or
  // half the line (false).
  struct AddressFormatExtension {
    ServerFieldType type;
    int label_id;
    ServerFieldType placed_after;
    // Usually " " or "\n". Should not be empty.
    base::StringPiece separator_before_label;
    bool large_sized;
  };

  // Gets all the `AddressFormatExtension`s available for `country_code()`.
  base::span<const AddressFormatExtension> address_format_extensions() const;

  // Returns true if the given `field_type` is part of Autofill's address
  // format for `country_code()`.
  bool IsAddressFieldSettingAccessible(ServerFieldType field_type) const;

  // Returns true if the given `field_type` is considered required.
  // Not to be confused with libaddressinput's requirements, it has its
  // own set of required fields.
  bool IsAddressFieldRequired(ServerFieldType field_type) const;

  // Returns the likely country code for |locale|, or "US" as a fallback if no
  // mapping from the locale is available.
  static const std::string CountryCodeForLocale(const std::string& locale);

  // The `country_code` provided to the constructor, with aliases like "GB"
  // replaced by their canonical version ("UK", in this case).
  const std::string& country_code() const { return country_code_; }

  // Returns the name of the country translated into the `locale` provided to
  // the constructor. If no `locale` was provided, an empty string is returned.
  const std::u16string& name() const { return name_; }

  // Full name is expected in a complete address for this country.
  bool requires_full_name() const {
    return base::FeatureList::IsEnabled(
        features::kAutofillRequireNameForProfileImport);
  }

  // City is expected in a complete address for this country.
  bool requires_city() const {
    return (required_fields_for_address_import_ & ADDRESS_REQUIRES_CITY) != 0;
  }

  // State is expected in a complete address for this country.
  bool requires_state() const {
    return (required_fields_for_address_import_ & ADDRESS_REQUIRES_STATE) != 0;
  }

  // Zip is expected in a complete address for this country.
  bool requires_zip() const {
    return (required_fields_for_address_import_ & ADDRESS_REQUIRES_ZIP) != 0;
  }

  // An address line1 is expected in a complete address for this country.
  bool requires_line1() const {
    return (required_fields_for_address_import_ & ADDRESS_REQUIRES_LINE1) != 0;
  }

  // True if a complete address is expected to either contain a state or a ZIP
  // code. Not true if the address explicitly needs both.
  bool requires_zip_or_state() const {
    return (required_fields_for_address_import_ &
            ADDRESS_REQUIRES_ZIP_OR_STATE) != 0;
  }

  bool requires_line1_or_house_number() const {
    return (required_fields_for_address_import_ &
            ADDRESS_REQUIRES_LINE1_OR_HOUSE_NUMBER);
  }

 private:
  AutofillCountry(const std::string& country_code,
                  const std::u16string& name,
                  const std::u16string& postal_code_label,
                  const std::u16string& state_label);

  // The two-letter ISO-3166 country code.
  std::string country_code_;

  // The country's name, localized to the app locale.
  std::u16string name_;

  // Required fields for an address import for the country.
  RequiredFieldsForAddressImport required_fields_for_address_import_;
};

LogBuffer& operator<<(LogBuffer& buffer, const AutofillCountry& country);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_AUTOFILL_COUNTRY_H_
