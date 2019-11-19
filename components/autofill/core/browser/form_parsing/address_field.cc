// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_field.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

using base::UTF8ToUTF16;

namespace autofill {

namespace {

bool SetFieldAndAdvanceCursor(AutofillScanner* scanner, AutofillField** field) {
  *field = scanner->Cursor();
  scanner->Advance();
  return true;
}

}  // namespace

// Some sites use type="tel" for zip fields (to get a numerical input).
// http://crbug.com/426958
const int AddressField::kZipCodeMatchType =
    MATCH_DEFAULT | MATCH_TELEPHONE | MATCH_NUMBER;

// Select fields are allowed here.  This occurs on top-100 site rediff.com.
const int AddressField::kCityMatchType =
    MATCH_DEFAULT | MATCH_SELECT | MATCH_SEARCH;

const int AddressField::kStateMatchType =
    MATCH_DEFAULT | MATCH_SELECT | MATCH_SEARCH;

std::unique_ptr<FormField> AddressField::Parse(AutofillScanner* scanner,
                                               LogManager* log_manager) {
  if (scanner->IsEnd())
    return nullptr;

  std::unique_ptr<AddressField> address_field(new AddressField(log_manager));
  const AutofillField* const initial_field = scanner->Cursor();
  size_t saved_cursor = scanner->SaveCursor();

  base::string16 attention_ignored = UTF8ToUTF16(kAttentionIgnoredRe);
  base::string16 region_ignored = UTF8ToUTF16(kRegionIgnoredRe);

  // Allow address fields to appear in any order.
  size_t begin_trailing_non_labeled_fields = 0;
  bool has_trailing_non_labeled_fields = false;
  while (!scanner->IsEnd()) {
    const size_t cursor = scanner->SaveCursor();
    // Ignore "Address Lookup" field. http://crbug.com/427622
    if (ParseField(scanner, base::UTF8ToUTF16(kAddressLookupRe), nullptr,
                   {log_manager, "kAddressLookupRe"}) ||
        ParseField(scanner, base::UTF8ToUTF16(kAddressNameIgnoredRe), nullptr,
                   {log_manager, "kAddressNameIgnoreRe"})) {
      continue;
      // Ignore email addresses.
    } else if (ParseFieldSpecifics(scanner, base::UTF8ToUTF16(kEmailRe),
                                   MATCH_DEFAULT | MATCH_TEXT_AREA, nullptr,
                                   {log_manager, "kEmailRe"})) {
      continue;
    } else if (address_field->ParseAddressLines(scanner) ||
               address_field->ParseCityStateZipCode(scanner) ||
               address_field->ParseCountry(scanner) ||
               address_field->ParseCompany(scanner)) {
      has_trailing_non_labeled_fields = false;
      continue;
    } else if (ParseField(scanner, attention_ignored, nullptr,
                          {log_manager, "kAttentionIgnoredRe"}) ||
               ParseField(scanner, region_ignored, nullptr,
                          {log_manager, "kRegionIgnoredRe"})) {
      // We ignore the following:
      // * Attention.
      // * Province/Region/Other.
      continue;
    } else if (scanner->Cursor() != initial_field &&
               ParseEmptyLabel(scanner, nullptr)) {
      // Ignore non-labeled fields within an address; the page
      // MapQuest Driving Directions North America.html contains such a field.
      // We only ignore such fields after we've parsed at least one other field;
      // otherwise we'd effectively parse address fields before other field
      // types after any non-labeled fields, and we want email address fields to
      // have precedence since some pages contain fields labeled
      // "Email address".
      if (!has_trailing_non_labeled_fields) {
        has_trailing_non_labeled_fields = true;
        begin_trailing_non_labeled_fields = cursor;
      }

      continue;
    } else {
      // No field found.
      break;
    }
  }

  // If we have identified any address fields in this field then it should be
  // added to the list of fields.
  if (address_field->company_ || address_field->address1_ ||
      address_field->address2_ || address_field->address3_ ||
      address_field->street_address_ || address_field->city_ ||
      address_field->state_ || address_field->zip_ || address_field->zip4_ ||
      address_field->country_) {
    // Don't slurp non-labeled fields at the end into the address.
    if (has_trailing_non_labeled_fields)
      scanner->RewindTo(begin_trailing_non_labeled_fields);
    return std::move(address_field);
  }

  scanner->RewindTo(saved_cursor);
  return nullptr;
}

AddressField::AddressField(LogManager* log_manager)
    : log_manager_(log_manager),
      company_(nullptr),
      address1_(nullptr),
      address2_(nullptr),
      address3_(nullptr),
      street_address_(nullptr),
      city_(nullptr),
      state_(nullptr),
      zip_(nullptr),
      zip4_(nullptr),
      country_(nullptr) {}

void AddressField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  // The page can request the address lines as a single textarea input or as
  // multiple text fields (or not at all), but it shouldn't be possible to
  // request both.
  DCHECK(!(address1_ && street_address_));
  DCHECK(!(address2_ && street_address_));
  DCHECK(!(address3_ && street_address_));

  AddClassification(company_, COMPANY_NAME, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(address1_, ADDRESS_HOME_LINE1, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(address2_, ADDRESS_HOME_LINE2, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(address3_, ADDRESS_HOME_LINE3, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(street_address_, ADDRESS_HOME_STREET_ADDRESS,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(city_, ADDRESS_HOME_CITY, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(state_, ADDRESS_HOME_STATE, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(zip_, ADDRESS_HOME_ZIP, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(country_, ADDRESS_HOME_COUNTRY, kBaseAddressParserScore,
                    field_candidates);
}

bool AddressField::ParseCompany(AutofillScanner* scanner) {
  if (company_)
    return false;

  return ParseField(scanner, UTF8ToUTF16(kCompanyRe), &company_,
                    {log_manager_, "kCompanyRe"});
}

bool AddressField::ParseAddressLines(AutofillScanner* scanner) {
  // We only match the string "address" in page text, not in element names,
  // because sometimes every element in a group of address fields will have
  // a name containing the string "address"; for example, on the page
  // Kohl's - Register Billing Address.html the text element labeled "city"
  // has the name "BILL_TO_ADDRESS<>city".  We do match address labels
  // such as "address1", which appear as element names on various pages (eg
  // AmericanGirl-Registration.html, BloomingdalesBilling.html,
  // EBay Registration Enter Information.html).
  if (address1_ || street_address_)
    return false;

  base::string16 pattern = UTF8ToUTF16(kAddressLine1Re);
  base::string16 label_pattern = UTF8ToUTF16(kAddressLine1LabelRe);
  if (!ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT, &address1_,
                           {log_manager_, "kAddressLine1Re"}) &&
      !ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                           &address1_,
                           {log_manager_, "kAddressLine1LabelRe"}) &&
      !ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT | MATCH_TEXT_AREA,
                           &street_address_,
                           {log_manager_, "kAddressLine1Re"}) &&
      !ParseFieldSpecifics(scanner, label_pattern,
                           MATCH_LABEL | MATCH_TEXT_AREA, &street_address_,
                           {log_manager_, "kAddressLine1LabelRe"}))
    return false;

  if (street_address_)
    return true;

  // This code may not pick up pages that have an address field consisting of a
  // sequence of unlabeled address fields. If we need to add this, see
  // discussion on https://codereview.chromium.org/741493003/
  pattern = UTF8ToUTF16(kAddressLine2Re);
  label_pattern = UTF8ToUTF16(kAddressLine2LabelRe);
  if (!ParseField(scanner, pattern, &address2_,
                  {log_manager_, "kAddressLine2Re"}) &&
      !ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                           &address2_, {log_manager_, "kAddressLine2LabelRe"}))
    return true;

  // Optionally parse address line 3. This uses the same label regexp as
  // address 2 above.
  pattern = UTF8ToUTF16(kAddressLinesExtraRe);
  if (!ParseField(scanner, pattern, &address3_,
                  {log_manager_, "kAddressLinesExtraRe"}) &&
      !ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                           &address3_, {log_manager_, "kAddressLine2LabelRe"}))
    return true;

  // Try for surplus lines, which we will promptly discard. Some pages have 4
  // address lines (e.g. uk/ShoesDirect2.html)!
  //
  // Since these are rare, don't bother considering unlabeled lines as extra
  // address lines.
  pattern = UTF8ToUTF16(kAddressLinesExtraRe);
  while (ParseField(scanner, pattern, nullptr,
                    {log_manager_, "kAddressLinesExtraRe"})) {
    // Consumed a surplus line, try for another.
  }
  return true;
}

bool AddressField::ParseCountry(AutofillScanner* scanner) {
  if (country_)
    return false;

  scanner->SaveCursor();
  if (ParseFieldSpecifics(scanner, UTF8ToUTF16(kCountryRe),
                          MATCH_DEFAULT | MATCH_SELECT | MATCH_SEARCH,
                          &country_, {log_manager_, "kCountryRe"})) {
    return true;
  }

  // The occasional page (e.g. google account registration page) calls this a
  // "location". However, this only makes sense for select tags.
  scanner->Rewind();
  return ParseFieldSpecifics(
      scanner, UTF8ToUTF16(kCountryLocationRe),
      MATCH_LABEL | MATCH_NAME | MATCH_SELECT | MATCH_SEARCH, &country_,
      {log_manager_, "kCountryLocationRe"});
}

bool AddressField::ParseZipCode(AutofillScanner* scanner) {
  if (zip_)
    return false;

  if (!ParseFieldSpecifics(scanner, UTF8ToUTF16(kZipCodeRe), kZipCodeMatchType,
                           &zip_, {log_manager_, "kZipCodeRe"})) {
    return false;
  }

  // Look for a zip+4, whose field name will also often contain
  // the substring "zip".
  ParseFieldSpecifics(scanner, UTF8ToUTF16(kZip4Re), kZipCodeMatchType, &zip4_,
                      {log_manager_, "kZip4Re"});
  return true;
}

bool AddressField::ParseCity(AutofillScanner* scanner) {
  if (city_)
    return false;

  return ParseFieldSpecifics(scanner, UTF8ToUTF16(kCityRe), kCityMatchType,
                             &city_, {log_manager_, "kCityRe"});
}

bool AddressField::ParseState(AutofillScanner* scanner) {
  if (state_)
    return false;

  return ParseFieldSpecifics(scanner, UTF8ToUTF16(kStateRe), kStateMatchType,
                             &state_, {log_manager_, "kStateRe"});
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelSeparately(
    AutofillScanner* scanner,
    const base::string16& pattern,
    int match_type,
    AutofillField** match,
    const RegExLogging& logging) {
  if (scanner->IsEnd())
    return RESULT_MATCH_NONE;

  AutofillField* cur_match = nullptr;
  size_t saved_cursor = scanner->SaveCursor();
  bool parsed_name = ParseFieldSpecifics(
      scanner, pattern, match_type & ~MATCH_LABEL, &cur_match, logging);
  scanner->RewindTo(saved_cursor);
  bool parsed_label = ParseFieldSpecifics(
      scanner, pattern, match_type & ~MATCH_NAME, &cur_match, logging);
  if (parsed_name && parsed_label) {
    if (match)
      *match = cur_match;
    return RESULT_MATCH_NAME_LABEL;
  }

  scanner->RewindTo(saved_cursor);
  if (parsed_name)
    return RESULT_MATCH_NAME;
  if (parsed_label)
    return RESULT_MATCH_LABEL;
  return RESULT_MATCH_NONE;
}

bool AddressField::ParseCityStateZipCode(AutofillScanner* scanner) {
  // Simple cases.
  if (scanner->IsEnd())
    return false;
  if (city_ && state_ && zip_)
    return false;
  if (state_ && zip_)
    return ParseCity(scanner);
  if (city_ && zip_)
    return ParseState(scanner);
  if (city_ && state_)
    return ParseZipCode(scanner);

  // Check for matches to both name and label.
  ParseNameLabelResult city_result = ParseNameAndLabelForCity(scanner);
  if (city_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult state_result = ParseNameAndLabelForState(scanner);
  if (state_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult zip_result = ParseNameAndLabelForZipCode(scanner);
  if (zip_result == RESULT_MATCH_NAME_LABEL)
    return true;

  // Check if there is only one potential match.
  bool maybe_city = city_result != RESULT_MATCH_NONE;
  bool maybe_state = state_result != RESULT_MATCH_NONE;
  bool maybe_zip = zip_result != RESULT_MATCH_NONE;
  if (maybe_city && !maybe_state && !maybe_zip)
    return SetFieldAndAdvanceCursor(scanner, &city_);
  if (maybe_state && !maybe_city && !maybe_zip)
    return SetFieldAndAdvanceCursor(scanner, &state_);
  if (maybe_zip && !maybe_city && !maybe_state)
    return ParseZipCode(scanner);

  // Otherwise give name priority over label.
  if (city_result == RESULT_MATCH_NAME)
    return SetFieldAndAdvanceCursor(scanner, &city_);
  if (state_result == RESULT_MATCH_NAME)
    return SetFieldAndAdvanceCursor(scanner, &state_);
  if (zip_result == RESULT_MATCH_NAME)
    return ParseZipCode(scanner);

  if (city_result == RESULT_MATCH_LABEL)
    return SetFieldAndAdvanceCursor(scanner, &city_);
  if (state_result == RESULT_MATCH_LABEL)
    return SetFieldAndAdvanceCursor(scanner, &state_);
  if (zip_result == RESULT_MATCH_LABEL)
    return ParseZipCode(scanner);

  return false;
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForZipCode(
    AutofillScanner* scanner) {
  if (zip_)
    return RESULT_MATCH_NONE;

  ParseNameLabelResult result = ParseNameAndLabelSeparately(
      scanner, UTF8ToUTF16(kZipCodeRe), kZipCodeMatchType, &zip_,
      {log_manager_, "kZipCodeRe"});

  if (result != RESULT_MATCH_NAME_LABEL || scanner->IsEnd())
    return result;

  size_t saved_cursor = scanner->SaveCursor();
  bool found_non_zip4 = ParseCity(scanner);
  if (found_non_zip4)
    city_ = nullptr;
  scanner->RewindTo(saved_cursor);
  if (!found_non_zip4) {
    found_non_zip4 = ParseState(scanner);
    if (found_non_zip4)
      state_ = nullptr;
    scanner->RewindTo(saved_cursor);
  }

  if (!found_non_zip4) {
    // Look for a zip+4, whose field name will also often contain
    // the substring "zip".
    ParseFieldSpecifics(scanner, UTF8ToUTF16(kZip4Re), kZipCodeMatchType,
                        &zip4_, {log_manager_, "kZip4Re"});
  }
  return result;
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForCity(
    AutofillScanner* scanner) {
  if (city_)
    return RESULT_MATCH_NONE;

  return ParseNameAndLabelSeparately(scanner, UTF8ToUTF16(kCityRe),
                                     kCityMatchType, &city_,
                                     {log_manager_, "kCityRe"});
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForState(
    AutofillScanner* scanner) {
  if (state_)
    return RESULT_MATCH_NONE;

  return ParseNameAndLabelSeparately(scanner, UTF8ToUTF16(kStateRe),
                                     kStateMatchType, &state_,
                                     {log_manager_, "kStateRe"});
}

}  // namespace autofill
