// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_ENTITY_DATA_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_ENTITY_DATA_TEST_UTILS_H_

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"

namespace autofill::test {

template <typename = void>
struct PassportEntityOptionsT {
  const char16_t* name = u"Pippi Långstrump";
  const char16_t* number = u"LR1234567";
  const char16_t* country = u"Sweden";
  const char16_t* expiry_date = u"2019-08-30";
  const char16_t* issue_date = u"2010-09-01";
  std::string_view guid = "00000000-0000-4000-8000-000000000000";
  std::string_view nickname = "Passie";
  base::Time date_modified = kJune2017;
  base::Time use_date = kJune2017;
  std::string_view app_locale = "en-US";
  EntityInstance::RecordType record_type = EntityInstance::RecordType::kLocal;
  EntityInstance::AreAttributesReadOnly are_attributes_read_only =
      EntityInstance::AreAttributesReadOnly(false);
  int use_count = 0;
};
using PassportEntityOptions = PassportEntityOptionsT<>;

// Creates a test passport instance with the values from `options`.
// Attributes whose value in `options` is `nullptr` are left absent.
// `options.date_modified` is rounded to seconds so that writing and reading the
// entity from the database obtains the original entity (the resolution of
// base::Time in the database is seconds).
EntityInstance GetPassportEntityInstance(PassportEntityOptions options = {});

EntityInstance GetPassportEntityInstanceWithRandomGuid(
    PassportEntityOptions options = {});

template <typename = void>
struct DriversLicenseOptionsT {
  const char16_t* name = u"Knecht Ruprecht";
  const char16_t* region = u"California";
  const char16_t* number = u"12312345";
  const char16_t* expiration_date = u"01/12/2019";
  const char16_t* issue_date = u"01/01/2010";
  std::string_view guid = "00000000-0000-4000-8000-100000000000";
  std::string_view nickname = "License";
  base::Time date_modified = kJune2017;
  base::Time use_date = kJune2017;
  std::string_view app_locale = "en-US";
  EntityInstance::RecordType record_type = EntityInstance::RecordType::kLocal;
  EntityInstance::AreAttributesReadOnly are_attributes_read_only =
      EntityInstance::AreAttributesReadOnly(false);
  int use_count = 0;
};
using DriversLicenseOptions = DriversLicenseOptionsT<>;

EntityInstance GetDriversLicenseEntityInstance(
    DriversLicenseOptions options = {});

EntityInstance GetDriversLicenseEntityInstanceWithRandomGuid(
    DriversLicenseOptions options = {});

template <typename = void>
struct VehicleOptionsT {
  const char16_t* name = u"Knecht Ruprecht";
  const char16_t* plate = u"123456";
  const char16_t* number = u"12312345";
  const char16_t* make = u"BMW";
  const char16_t* model = u"Series 2";
  const char16_t* year = u"2025";
  const char16_t* state = u"California";
  std::string_view guid = "00000000-0000-4000-8000-200000000000";
  std::string_view nickname = "Vehicle";
  base::Time date_modified = kJune2017;
  base::Time use_date = kJune2017;
  std::string_view app_locale = "en-US";
  EntityInstance::RecordType record_type = EntityInstance::RecordType::kLocal;
  EntityInstance::AreAttributesReadOnly are_attributes_read_only =
      EntityInstance::AreAttributesReadOnly(false);
  int use_count = 0;
};
using VehicleOptions = VehicleOptionsT<>;

EntityInstance GetVehicleEntityInstance(VehicleOptions options = {});

EntityInstance GetVehicleEntityInstanceWithRandomGuid(
    VehicleOptions options = {});

template <typename = void>
struct NationalIdCardOptionsT {
  const char16_t* name = u"Name";
  const char16_t* number = u"987654321";
  const char16_t* country = u"United States";
  const char16_t* issue_date = u"01/12/2020";
  const char16_t* expiry_date = u"01/12/2030";
  std::string_view guid = "00000000-0000-4000-8000-300000000000";
  std::string_view nickname = "IdCard";
  std::string_view app_locale = "en-US";
  base::Time date_modified = kJune2017;
  base::Time use_date = kJune2017;
  EntityInstance::RecordType record_type = EntityInstance::RecordType::kLocal;
  EntityInstance::AreAttributesReadOnly are_attributes_read_only =
      EntityInstance::AreAttributesReadOnly(false);
  int use_count = 0;
};
using NationalIdCardOptions = NationalIdCardOptionsT<>;

EntityInstance GetNationalIdCardEntityInstance(
    NationalIdCardOptions options = {});

template <typename = void>
struct KnownTravelerNumberOptionsT {
  const char16_t* name = u"Name";
  const char16_t* number = u"987654321";
  const char16_t* expiration_date = u"01/12/2030";
  std::string_view guid = "00000000-0000-4000-8000-400000000000";
  std::string_view nickname = "Known Traveler Number";
  std::string_view app_locale = "en-US";
  base::Time date_modified = kJune2017;
  base::Time use_date = kJune2017;
  EntityInstance::RecordType record_type = EntityInstance::RecordType::kLocal;
  EntityInstance::AreAttributesReadOnly are_attributes_read_only =
      EntityInstance::AreAttributesReadOnly(false);
  int use_count = 0;
};
using KnownTravelerNumberOptions = KnownTravelerNumberOptionsT<>;

EntityInstance GetKnownTravelerNumberInstance(
    KnownTravelerNumberOptions options = {});

template <typename = void>
struct RedressNumberOptionsT {
  const char16_t* name = u"Name";
  const char16_t* number = u"987654321";
  std::string_view guid = "00000000-0000-4000-8000-500000000000";
  std::string_view nickname = "RedressNumber";
  std::string_view app_locale = "en-US";
  base::Time date_modified = kJune2017;
  base::Time use_date = kJune2017;
  EntityInstance::RecordType record_type = EntityInstance::RecordType::kLocal;
  EntityInstance::AreAttributesReadOnly are_attributes_read_only =
      EntityInstance::AreAttributesReadOnly(false);
  int use_count = 0;
};
using RedressNumberOptions = RedressNumberOptionsT<>;

EntityInstance GetRedressNumberEntityInstance(
    RedressNumberOptions options = {});

template <typename = void>
struct FlightReservationOptionsT {
  const char16_t* flight_number = u"AA123";
  const char16_t* ticket_number = u"123123456";
  const char16_t* confirmation_code = u"AB4KW5";
  const char16_t* name = u"John Doe";
  const char16_t* departure_airport = u"MUC";
  const char16_t* arrival_airport = u"BEY";
  std::optional<base::Time> departure_time = std::nullopt;
  base::TimeDelta departure_time_zone_offset = base::TimeDelta();
  std::string_view guid = "00000000-0000-4000-8000-500000000000";
  std::string_view nickname = "FlightReservation";
  std::string_view app_locale = "en-US";
  base::Time date_modified = kJune2017;
  base::Time use_date = kJune2017;
  EntityInstance::RecordType record_type = EntityInstance::RecordType::kLocal;
  EntityInstance::AreAttributesReadOnly are_attributes_read_only =
      EntityInstance::AreAttributesReadOnly(false);
  int use_count = 0;
};
using FlightReservationOptions = FlightReservationOptionsT<>;

EntityInstance GetFlightReservationEntityInstance(
    FlightReservationOptions options = {});

EntityInstance GetFlightReservationEntityInstanceWithRandomGuid(
    FlightReservationOptions options = {});

template <typename = void>
struct OrderOptionsT {
  const char16_t* id = u"12345";
  const char16_t* account = u"test@gmail.com";
  const char16_t* date = u"2025-01-15";
  const char16_t* merchant_name = u"Example";
  const char16_t* merchant_domain = u"example.com";
  const char16_t* product_names = u"Product A, Product B";
  const char16_t* grand_total = u"12.34";
  std::string_view guid = "00000000-0000-4000-8000-600000000000";
  std::string_view nickname = "Order";
  std::string_view app_locale = "en-US";
  base::Time date_modified = kJune2017;
  base::Time use_date = kJune2017;
  EntityInstance::RecordType record_type = EntityInstance::RecordType::kLocal;
  EntityInstance::AreAttributesReadOnly are_attributes_read_only =
      EntityInstance::AreAttributesReadOnly(false);
  int use_count = 0;
};
using OrderOptions = OrderOptionsT<>;

EntityInstance GetOrderEntityInstance(OrderOptions options = {});

template <typename = void>
struct EntityOptionsT {
  std::string_view guid = "00000000-0000-4000-8000-000000000000";
  std::string_view nickname = "Mine";
  base::Time date_modified = kJune2017;
  base::Time use_date = kJune2017;
  std::string_view app_locale = "en-US";
  EntityInstance::RecordType record_type = EntityInstance::RecordType::kLocal;
  EntityInstance::AreAttributesReadOnly are_attributes_read_only =
      EntityInstance::AreAttributesReadOnly(false);
  std::string_view frecency_override = "";
  int use_count = 0;
};
using EntityOptions = EntityOptionsT<>;

EntityInstance GetEntityInstance(std::vector<AttributeInstance> attributes,
                                 EntityOptions options = {});

// Returns a copy of `entity_instance` with all obfuscated attributes masked.
// Note that the masking is a toy version of what the server might do - it
// simply takes the last 4 characters of obfuscated attributes.
EntityInstance MaskEntityInstance(const EntityInstance& entity_instance);

}  // namespace autofill::test

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_ENTITY_DATA_TEST_UTILS_H_
