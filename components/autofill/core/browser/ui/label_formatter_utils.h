// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_LABEL_FORMATTER_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_LABEL_FORMATTER_UTILS_H_

#include <list>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Adds |part| to the end of |parts| if |part| is not an empty string.
void AddLabelPartIfNotEmpty(const base::string16& part,
                            std::vector<base::string16>* parts);

// Returns the text to show to the user. If there is more than one element in
// |parts|, then a separator, |IDS_AUTOFILL_SUGGESTION_LABEL_SEPARATOR|, is
// inserted between them.
base::string16 ConstructLabelLine(const std::vector<base::string16>& parts);

// Like ConstructLabelLine, but uses |IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR|
// instead.
base::string16 ConstructMobileLabelLine(
    const std::vector<base::string16>& parts);

// Returns true if |type| is associated with an address, but not with a street
// address. For example, if the given type is ADDRESS_HOME_ZIP, then true is
// returned. If ADDRESS_BILLING_LINE1 is given then false is returned.
bool IsNonStreetAddressPart(ServerFieldType type);

// Returns true if |type| is associated with a street address.
bool IsStreetAddressPart(ServerFieldType type);

// Returns true if |types| has a non-street-address-related type.
bool HasNonStreetAddress(const std::vector<ServerFieldType>& types);

// Returns true if |types| has a street-address-related type.
bool HasStreetAddress(const std::vector<ServerFieldType>& types);

// Returns a vector of only street-address-related field types in |types| if
// |extract_street_address_types| is true, e.g. ADDRESS_HOME_LINE1.
//
// Returns a vector of only non-street-address-related field types in |types|
// if |extract_street_address_types| is false, e.g. ADDRESS_BILLING_ZIP.
std::vector<ServerFieldType> ExtractSpecifiedAddressFieldTypes(
    bool extract_street_address_types,
    const std::vector<ServerFieldType>& types);

// Returns a collection of the types in |types| that belong to the
// ADDRESS_HOME or ADDRESS_BILLING FieldTypeGroups.
std::vector<ServerFieldType> ExtractAddressFieldTypes(
    const std::vector<ServerFieldType>& types);

// Returns a collection of the types in |types| without |field_type_to_remove|.
std::vector<ServerFieldType> TypesWithoutFocusedField(
    const std::vector<ServerFieldType>& types,
    ServerFieldType field_type_to_remove);

// Returns a pared down copy of |profile|. The copy has the same guid, origin,
// country and language codes, and |field_types| as |profile|.
AutofillProfile MakeTrimmedProfile(const AutofillProfile& profile,
                                   const std::string& app_locale,
                                   const std::vector<ServerFieldType>& types);

// Returns either street-address data or non-street-address data found in
// |profile|. If |focused_field_type| is a street address field, then returns
// non-street-address data, e.g. Lowell, MA 01852.
//
// If the focused type is not a street address field and if
// |form_has_street_address| is true, then returns street-address data, e.g. 375
// Merrimack St.
//
// If the focused type is not a street address field and if the form does not
// have a street address, then returns the parts of the address in the form
// other than the focused field. For example, if a user focuses on a city
// field and if the state and zip code can be in the label, then MA 01852 is
// returned. If there are no other non-street-address fields or if the data is
// not present in |profile|, then an empty string is returned.
base::string16 GetLabelForFocusedAddress(
    ServerFieldType focused_field_type,
    bool form_has_street_address,
    const AutofillProfile& profile,
    const std::string& app_locale,
    const std::vector<ServerFieldType>& types);

// If |use_street_address| is true and if |profile| is associated with a street
// address, then returns the street address, e.g. 24 Beacon St.
//
// If |use_street_address| is false and |profile| is associated with address
// fields other than street addresses, then returns the non-street-
// address-related data corresponding to |types|.
base::string16 GetLabelAddress(bool use_street_address,
                               const AutofillProfile& profile,
                               const std::string& app_locale,
                               const std::vector<ServerFieldType>& types);

// Returns the national address associated with |profile|, e.g.
// 24 Beacon St., Boston, MA 02133.
base::string16 GetLabelNationalAddress(
    const std::vector<ServerFieldType>& types,
    const AutofillProfile& profile,
    const std::string& app_locale);

// Returns the street address associated with |profile|, e.g. 24 Beacon St.
base::string16 GetLabelStreetAddress(const std::vector<ServerFieldType>& types,
                                     const AutofillProfile& profile,
                                     const std::string& app_locale);

// Returns a label to show the user when |focused_field_type_| is not part of
// a street address. For example, city and postal code are non-street-address
// field types.
base::string16 GetLabelForProfileOnFocusedNonStreetAddress(
    bool form_has_street_address,
    const AutofillProfile& profile,
    const std::string& app_locale,
    const std::vector<ServerFieldType>& types,
    const base::string16& contact_info);

// Returns a name that is (A) associated with |profile| and (B) found in
// |types|; otherwise, returns an empty string.
base::string16 GetLabelName(const std::vector<ServerFieldType>& types,
                            const AutofillProfile& profile,
                            const std::string& app_locale);

// Returns the first name associated with |profile|, if any; otherwise, returns
// an empty string.
base::string16 GetLabelFirstName(const AutofillProfile& profile,
                                 const std::string& app_locale);

// Returns the email address associated with |profile|, if any; otherwise,
// returns an empty string.
base::string16 GetLabelEmail(const AutofillProfile& profile,
                             const std::string& app_locale);

// Returns the phone number associated with |profile|, if any; otherwise,
// returns an empty string. Phone numbers are given in |profile|'s country's
// national format, if possible.
base::string16 GetLabelPhone(const AutofillProfile& profile,
                             const std::string& app_locale);

// Each HaveSame* function below returns true if all |profiles| have the same
// specified data. Note that the absence of data and actual data, e.g.
// joe.bray@aol.com, are considered different pieces of data.
//
// Near-duplicate data, such as Düsseldorf and Dusseldorf or 3 Elm St and 3 Elm
// St., should be filtered out before calling this function.
bool HaveSameEmailAddresses(const std::vector<AutofillProfile*>& profiles,
                            const std::string& app_locale);
bool HaveSameFirstNames(const std::vector<AutofillProfile*>& profiles,
                        const std::string& app_locale);
bool HaveSameNonStreetAddresses(const std::vector<AutofillProfile*>& profiles,
                                const std::string& app_locale,
                                const std::vector<ServerFieldType>& types);
bool HaveSamePhoneNumbers(const std::vector<AutofillProfile*>& profiles,
                          const std::string& app_locale);
bool HaveSameStreetAddresses(const std::vector<AutofillProfile*>& profiles,
                             const std::string& app_locale,
                             const std::vector<ServerFieldType>& types);

// Each HasUnfocused* function below returns true if the form described by
// |form_groups| includes a field corresponding to the specified data and if
// this field is not being interacted with by the user, i.e. the specified field
// is not associated with the |focused_group|.
bool HasUnfocusedEmailField(FieldTypeGroup focused_group, uint32_t form_groups);
bool HasUnfocusedNameField(FieldTypeGroup focused_group, uint32_t form_groups);
bool HasUnfocusedNonStreetAddressField(
    ServerFieldType focused_field,
    FieldTypeGroup focused_group,
    const std::vector<ServerFieldType>& types);
bool HasUnfocusedPhoneField(FieldTypeGroup focused_group, uint32_t form_groups);
bool HasUnfocusedStreetAddressField(ServerFieldType focused_field,
                                    FieldTypeGroup focused_group,
                                    const std::vector<ServerFieldType>& types);

// Returns true if the form has only non-street-address-related fields, such as
// city, state, and zip code.
bool FormHasOnlyNonStreetAddressFields(
    const std::vector<ServerFieldType>& types,
    uint32_t form_groups);
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_LABEL_FORMATTER_UTILS_H_
