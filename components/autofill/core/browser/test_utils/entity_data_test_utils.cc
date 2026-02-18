// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"

#include "base/containers/to_vector.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/proto/server.pb.h"

namespace autofill::test {

namespace {

// Converts a struct with entity-type-specific options to a generic
// EntityOptions.
template <typename Options>
EntityOptions ToEntityOptions(Options options) {
  return EntityOptions{
      .guid = options.guid,
      .nickname = options.nickname,
      .date_modified = options.date_modified,
      .use_date = options.use_date,
      .app_locale = options.app_locale,
      .record_type = options.record_type,
      .are_attributes_read_only = options.are_attributes_read_only,
      .use_count = options.use_count,
  };
}

}  // namespace

EntityInstance GetPassportEntityInstance(PassportEntityOptions options) {
  using enum AttributeTypeName;
  std::vector<AttributeInstance> attributes;
  if (options.number) {
    attributes.emplace_back(AttributeType(kPassportNumber));
    attributes.back().SetInfo(
        PASSPORT_NUMBER, options.number, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.name) {
    attributes.emplace_back(AttributeType(kPassportName));
    attributes.back().SetInfo(
        NAME_FULL, options.name, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
    attributes.back().FinalizeInfo();
  }
  if (options.country) {
    attributes.emplace_back(AttributeType(kPassportCountry));
    attributes.back().SetInfo(PASSPORT_ISSUING_COUNTRY, options.country,
                              std::string(options.app_locale),
                              /*format_string=*/std::nullopt,
                              VerificationStatus::kNoStatus);
  }
  if (options.expiry_date) {
    attributes.emplace_back(AttributeType(kPassportExpirationDate));
    attributes.back().SetInfo(
        PASSPORT_EXPIRATION_DATE, options.expiry_date,
        std::string(options.app_locale),
        AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE),
        VerificationStatus::kNoStatus);
  }
  if (options.issue_date) {
    attributes.emplace_back(AttributeType(kPassportIssueDate));
    attributes.back().SetInfo(
        PASSPORT_ISSUE_DATE, options.issue_date,
        std::string(options.app_locale),
        AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE),
        VerificationStatus::kNoStatus);
  }
  return GetEntityInstance(std::move(attributes), ToEntityOptions(options));
}

EntityInstance GetPassportEntityInstanceWithRandomGuid(
    PassportEntityOptions options) {
  base::Uuid guid = base::Uuid::GenerateRandomV4();
  options.guid = guid.AsLowercaseString();
  return GetPassportEntityInstance(options);
}

EntityInstance GetDriversLicenseEntityInstance(DriversLicenseOptions options) {
  using enum AttributeTypeName;
  std::vector<AttributeInstance> attributes;
  if (options.name) {
    attributes.emplace_back(AttributeType(kDriversLicenseName));
    attributes.back().SetInfo(
        NAME_FULL, options.name, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
    attributes.back().FinalizeInfo();
  }
  if (options.region) {
    attributes.emplace_back(AttributeType(kDriversLicenseState));
    attributes.back().SetInfo(
        DRIVERS_LICENSE_REGION, options.region, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.number) {
    attributes.emplace_back(AttributeType(kDriversLicenseNumber));
    attributes.back().SetInfo(
        DRIVERS_LICENSE_NUMBER, options.number, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.expiration_date) {
    attributes.emplace_back(AttributeType(kDriversLicenseExpirationDate));
    attributes.back().SetInfo(
        DRIVERS_LICENSE_EXPIRATION_DATE, options.expiration_date,
        std::string(options.app_locale),
        AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE),
        VerificationStatus::kNoStatus);
  }
  if (options.issue_date) {
    attributes.emplace_back(AttributeType(kDriversLicenseIssueDate));
    attributes.back().SetInfo(
        DRIVERS_LICENSE_ISSUE_DATE, options.issue_date,
        std::string(options.app_locale),
        AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE),
        VerificationStatus::kNoStatus);
  }
  return GetEntityInstance(std::move(attributes), ToEntityOptions(options));
}

EntityInstance GetDriversLicenseEntityInstanceWithRandomGuid(
    DriversLicenseOptions options) {
  base::Uuid guid = base::Uuid::GenerateRandomV4();
  options.guid = guid.AsLowercaseString();
  return GetDriversLicenseEntityInstance(options);
}

EntityInstance GetKnownTravelerNumberInstance(
    KnownTravelerNumberOptions options) {
  using enum AttributeTypeName;
  std::vector<AttributeInstance> attributes;
  if (options.name) {
    attributes.emplace_back(AttributeType(kKnownTravelerNumberName));
    attributes.back().SetInfo(
        NAME_FULL, options.name, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.number) {
    attributes.emplace_back(AttributeType(kKnownTravelerNumberNumber));
    attributes.back().SetInfo(
        KNOWN_TRAVELER_NUMBER, options.number, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.expiration_date) {
    attributes.emplace_back(AttributeType(kKnownTravelerNumberExpirationDate));
    attributes.back().SetInfo(
        KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE, options.expiration_date,
        std::string(options.app_locale),
        AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE),
        VerificationStatus::kNoStatus);
  }
  return GetEntityInstance(std::move(attributes), ToEntityOptions(options));
}

EntityInstance GetRedressNumberEntityInstance(RedressNumberOptions options) {
  using enum AttributeTypeName;
  std::vector<AttributeInstance> attributes;
  if (options.name) {
    attributes.emplace_back(AttributeType(kRedressNumberName));
    attributes.back().SetInfo(
        NAME_FULL, options.name, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.number) {
    attributes.emplace_back(AttributeType(kRedressNumberNumber));
    attributes.back().SetInfo(
        REDRESS_NUMBER, options.number, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  return GetEntityInstance(std::move(attributes), ToEntityOptions(options));
}

EntityInstance GetVehicleEntityInstance(VehicleOptions options) {
  using enum AttributeTypeName;
  std::vector<AttributeInstance> attributes;
  if (options.name) {
    attributes.emplace_back(AttributeType(kVehicleOwner));
    attributes.back().SetInfo(
        NAME_FULL, options.name, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
    attributes.back().FinalizeInfo();
  }
  if (options.plate) {
    attributes.emplace_back(AttributeType(kVehiclePlateNumber));
    attributes.back().SetInfo(
        VEHICLE_LICENSE_PLATE, options.plate, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.number) {
    attributes.emplace_back(AttributeType(kVehicleVin));
    attributes.back().SetInfo(
        VEHICLE_VIN, options.number, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.make) {
    attributes.emplace_back(AttributeType(kVehicleMake));
    attributes.back().SetInfo(
        VEHICLE_MAKE, options.make, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.model) {
    attributes.emplace_back(AttributeType(kVehicleModel));
    attributes.back().SetInfo(
        VEHICLE_MODEL, options.model, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.year) {
    attributes.emplace_back(AttributeType(kVehicleYear));
    attributes.back().SetInfo(
        VEHICLE_YEAR, options.year, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.state) {
    attributes.emplace_back(AttributeType(kVehiclePlateState));
    attributes.back().SetInfo(
        VEHICLE_PLATE_STATE, options.state, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  return GetEntityInstance(std::move(attributes), ToEntityOptions(options));
}

EntityInstance GetVehicleEntityInstanceWithRandomGuid(VehicleOptions options) {
  base::Uuid guid = base::Uuid::GenerateRandomV4();
  options.guid = guid.AsLowercaseString();
  return GetVehicleEntityInstance(options);
}

EntityInstance GetNationalIdCardEntityInstance(NationalIdCardOptions options) {
  using enum AttributeTypeName;
  std::vector<AttributeInstance> attributes;
  if (options.name) {
    attributes.emplace_back(AttributeType(kNationalIdCardName));
    attributes.back().SetInfo(
        NAME_FULL, options.name, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.number) {
    attributes.emplace_back(AttributeType(kNationalIdCardNumber));
    attributes.back().SetInfo(NATIONAL_ID_CARD_NUMBER, options.number,
                              std::string(options.app_locale),
                              /*format_string=*/std::nullopt,
                              VerificationStatus::kNoStatus);
  }
  if (options.country) {
    attributes.emplace_back(AttributeType(kNationalIdCardCountry));
    attributes.back().SetInfo(NATIONAL_ID_CARD_ISSUING_COUNTRY, options.country,
                              std::string(options.app_locale),
                              /*format_string=*/std::nullopt,
                              VerificationStatus::kNoStatus);
  }
  if (options.issue_date) {
    attributes.emplace_back(AttributeType(kNationalIdCardIssueDate));
    attributes.back().SetInfo(
        NATIONAL_ID_CARD_ISSUE_DATE, options.issue_date,
        std::string(options.app_locale),
        AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE),
        VerificationStatus::kNoStatus);
  }
  if (options.expiry_date) {
    attributes.emplace_back(AttributeType(kNationalIdCardExpirationDate));
    attributes.back().SetInfo(
        NATIONAL_ID_CARD_EXPIRATION_DATE, options.expiry_date,
        std::string(options.app_locale),
        AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE),
        VerificationStatus::kNoStatus);
  }
  return GetEntityInstance(std::move(attributes), ToEntityOptions(options));
}

EntityInstance GetFlightReservationEntityInstance(
    FlightReservationOptions options) {
  using enum AttributeTypeName;
  std::vector<AttributeInstance> attributes;
  if (options.name) {
    attributes.emplace_back(AttributeType(kFlightReservationPassengerName));
    attributes.back().SetInfo(
        NAME_FULL, options.name, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
    attributes.back().FinalizeInfo();
  }
  if (options.flight_number) {
    attributes.emplace_back(AttributeType(kFlightReservationFlightNumber));
    attributes.back().SetInfo(
        FLIGHT_RESERVATION_FLIGHT_NUMBER, options.flight_number,
        std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.ticket_number) {
    attributes.emplace_back(AttributeType(kFlightReservationTicketNumber));
    attributes.back().SetInfo(
        FLIGHT_RESERVATION_TICKET_NUMBER, options.ticket_number,
        std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.confirmation_code) {
    attributes.emplace_back(AttributeType(kFlightReservationConfirmationCode));
    attributes.back().SetInfo(
        FLIGHT_RESERVATION_CONFIRMATION_CODE, options.confirmation_code,
        std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.departure_airport) {
    attributes.emplace_back(AttributeType(kFlightReservationDepartureAirport));
    attributes.back().SetInfo(
        FLIGHT_RESERVATION_DEPARTURE_AIRPORT, options.departure_airport,
        std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.arrival_airport) {
    attributes.emplace_back(AttributeType(kFlightReservationArrivalAirport));
    attributes.back().SetInfo(
        FLIGHT_RESERVATION_ARRIVAL_AIRPORT, options.arrival_airport,
        std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }

  std::string frecency_override;
  if (options.departure_time) {
    frecency_override = base::TimeFormatAsIso8601(*options.departure_time);

    attributes.emplace_back(AttributeType(kFlightReservationDepartureDate));
    // The departure date must be stored in the departure airport's time zone.
    std::string offsetted_departure_time = base::TimeFormatAsIso8601(
        *options.departure_time + options.departure_time_zone_offset);
    std::string date = offsetted_departure_time.substr(
        0, offsetted_departure_time.find_first_of('T'));
    attributes.back().SetInfo(
        FLIGHT_RESERVATION_DEPARTURE_DATE, base::UTF8ToUTF16(date),
        std::string(options.app_locale),
        /*format_string=*/
        AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE),
        VerificationStatus::kNoStatus);
  }
  EntityOptions entity_options = ToEntityOptions(options);
  entity_options.frecency_override = frecency_override;
  return GetEntityInstance(std::move(attributes), std::move(entity_options));
}

EntityInstance GetFlightReservationEntityInstanceWithRandomGuid(
    FlightReservationOptions options) {
  base::Uuid guid = base::Uuid::GenerateRandomV4();
  options.guid = guid.AsLowercaseString();
  return GetFlightReservationEntityInstance(options);
}

EntityInstance GetOrderEntityInstance(OrderOptions options) {
  using enum AttributeTypeName;
  std::vector<AttributeInstance> attributes;
  if (options.id) {
    attributes.emplace_back(AttributeType(kOrderId));
    attributes.back().SetInfo(
        ORDER_ID, options.id, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.account) {
    attributes.emplace_back(AttributeType(kOrderAccount));
    attributes.back().SetInfo(
        ORDER_ACCOUNT, options.account, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  if (options.date) {
    attributes.emplace_back(AttributeType(kOrderDate));
    attributes.back().SetInfo(
        ORDER_DATE, options.date, std::string(options.app_locale),
        AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE),
        VerificationStatus::kNoStatus);
  }
  if (options.merchant_name) {
    attributes.emplace_back(AttributeType(kOrderMerchantName));
    attributes.back().SetInfo(ORDER_MERCHANT_NAME, options.merchant_name,
                              std::string(options.app_locale),
                              /*format_string=*/std::nullopt,
                              VerificationStatus::kNoStatus);
  }
  if (options.merchant_domain) {
    attributes.emplace_back(AttributeType(kOrderMerchantDomain));
    attributes.back().SetInfo(ORDER_MERCHANT_DOMAIN, options.merchant_domain,
                              std::string(options.app_locale),
                              /*format_string=*/std::nullopt,
                              VerificationStatus::kNoStatus);
  }
  if (options.product_names) {
    attributes.emplace_back(AttributeType(kOrderProductNames));
    attributes.back().SetInfo(ORDER_PRODUCT_NAMES, options.product_names,
                              std::string(options.app_locale),
                              /*format_string=*/std::nullopt,
                              VerificationStatus::kNoStatus);
  }
  if (options.grand_total) {
    attributes.emplace_back(AttributeType(kOrderGrandTotal));
    attributes.back().SetInfo(
        ORDER_GRAND_TOTAL, options.grand_total, std::string(options.app_locale),
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
  }
  return GetEntityInstance(std::move(attributes), ToEntityOptions(options));
}

EntityInstance GetEntityInstance(std::vector<AttributeInstance> attributes,
                                 EntityOptions options) {
  CHECK(!attributes.empty()) << "Attributes must be non-empty.";
  const EntityType type = attributes[0].type().entity_type();
  CHECK(std::ranges::all_of(attributes,
                            [&](const AttributeInstance& attribute) {
                              return attribute.type().entity_type() == type;
                            }))
      << "All attribute types must belong to the same entity type";
  return EntityInstance(
      type, std::move(attributes),
      EntityInstance::EntityId(base::Uuid::ParseLowercase(options.guid)),
      std::string(options.nickname),
      base::Time::FromTimeT(options.date_modified.ToTimeT()), options.use_count,
      base::Time::FromTimeT(options.use_date.ToTimeT()), options.record_type,
      options.are_attributes_read_only, std::string(options.frecency_override));
}

EntityInstance MaskEntityInstance(const EntityInstance& entity_instance) {
  CHECK_EQ(entity_instance.record_type(),
           EntityInstance::RecordType::kServerWallet)
      << "Masking only makes sense for server Wallet entities.";
  std::vector<AttributeInstance> attributes =
      base::ToVector(entity_instance.attributes());
  for (AttributeInstance& attribute : attributes) {
    if (!attribute.type().is_obfuscated()) {
      continue;
    }
    const FieldType field_type = attribute.type().field_type();
    const std::u16string full_value =
        attribute.GetInfo(field_type, "en-US", /*format_string=*/std::nullopt);
    // Do some simple masking to simulate what the server might do.
    const size_t masked_length = std::min<size_t>(full_value.size(), 4);
    attribute.SetInfo(
        attribute.type().field_type(),
        /*value=*/full_value.substr(full_value.size() - masked_length),
        /*app_locale=*/"en-US",
        /*format_string=*/std::nullopt, VerificationStatus::kNoStatus);
    test_api(attribute).mark_as_masked();
  }
  return EntityInstance(
      entity_instance.type(), std::move(attributes), entity_instance.guid(),
      entity_instance.nickname(), entity_instance.date_modified(),
      entity_instance.use_count(), entity_instance.use_date(),
      entity_instance.record_type(), entity_instance.are_attributes_read_only(),
      test_api(entity_instance).frecency_override());
}

}  // namespace autofill::test
