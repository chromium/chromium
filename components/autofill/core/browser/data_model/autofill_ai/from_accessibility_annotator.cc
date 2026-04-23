// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"

#include <optional>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/common/dense_set.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

namespace aa = accessibility_annotator;

namespace {

constexpr std::optional<EntityType> FromAccessibilityAnnotator(
    aa::EntityType src_entity) {
  std::optional<EntityTypeName> name = [&]() -> std::optional<EntityTypeName> {
    using Src = aa::EntityType;
    using Tgt = EntityTypeName;
    switch (src_entity) {
      case Src::kUnknown:
        return std::nullopt;
      case Src::kFlightReservation:
        return Tgt::kFlightReservation;
      case Src::kOrder:
        return Tgt::kOrder;
      case Src::kShipment:
        return Tgt::kShipment;
      case Src::kDriversLicense:
        return Tgt::kDriversLicense;
      case Src::kPassport:
        return Tgt::kPassport;
      case Src::kNationalId:
        return Tgt::kNationalIdCard;
      case Src::kVehicle:
        return Tgt::kVehicle;
    }
    NOTREACHED();
  }();
  return name.transform([](EntityTypeName name) { return EntityType(name); });
}

// Converts a member of `EntitySpecifics` (e.g., `aa::Passport::name`) into an
// AttributeInstance and adds it to `attribute` if the conversion is successful.
//
// For example, to convert a passport number from Accessibility Annotator to
// Autofill AI,
// - `entity` must be an `aa::Passport`,
// - `member` must be `&aa::Passport::number`, and
// - `attribute_type_name` must be `autofill::AttributeTypeName::kPassportName`.
template <typename Value, typename EntitySpecifics>
void AddAttributeInstance(const EntitySpecifics& entity,
                          const Value EntitySpecifics::* member,
                          AttributeTypeName attribute_type_name,
                          std::vector<AttributeInstance>& attributes) {
  // Converts an Accessibility Annotator attribute value into a std::u16string
  // as expected by Autofill AI's AttributeInstance.
  // Returns std::nullopt if the input value is empty or invalid.
  auto convert = absl::Overload{
      [](std::string_view s) {
        return !s.empty() ? std::optional(base::UTF8ToUTF16(s)) : std::nullopt;
      },

      [](const aa::Date& src_date) {
        data_util::Date date = {.year = src_date.year,
                                .month = src_date.month,
                                .day = src_date.day};
        return data_util::IsValidDateForFormat(date, u"YYYY-MM-DD")
                   ? std::optional(data_util::FormatDate(date, u"YYYY-MM-DD"))
                   : std::nullopt;
      },

      [](this auto&& self, base::Time time) {
        // TODO(b/40283901): This conversion is not correct for flight departure
        // dates: the timezone should be the airport's, not the current local
        // timezone.
        base::Time::Exploded exploded;
        time.LocalExplode(&exploded);
        return self(aa::Date{.day = exploded.day_of_month,
                             .month = exploded.month,
                             .year = exploded.year});
      },

      [](this auto&& self, const GURL& url) {
        return url.is_valid() ? self(url.spec()) : std::nullopt;
      },

      [](this auto&& self,
         base::span<const aa::Order::ItemDescription> products) {
        // Joins the product names with commas.
        return self(base::JoinString(
            base::ToVector(products, &aa::Order::ItemDescription::name), ", "));
      },

      []<typename T>(this auto&& self, const std::optional<T>& value) {
        return value.and_then(self);
      },
  };

  std::optional<std::u16string> value = convert(entity.*member);
  if (!value) {
    return;
  }
  AttributeInstance& a =
      attributes.emplace_back(AttributeType(attribute_type_name));
  a.SetRawInfo(a.type().field_type(), *std::move(value),
               VerificationStatus::kNoStatus);
}

}  // namespace

constinit const DenseSet<aa::EntityType>
    kAllEntityTypesSharedWithAccessibilityAnnotator = []() {
      DenseSet<aa::EntityType> result;
      for (aa::EntityType entity : DenseSet<aa::EntityType>::all()) {
        if (FromAccessibilityAnnotator(entity)) {
          result.insert(entity);
        }
      }
      return result;
    }();

DenseSet<EntityType> FromAccessibilityAnnotator(
    aa::EntityTypeEnumSet src_entities) {
  DenseSet<EntityType> entities;
  for (aa::EntityType src_entity : src_entities) {
    if (std::optional<EntityType> entity =
            FromAccessibilityAnnotator(src_entity)) {
      entities.insert(*entity);
    }
  }
  return entities;
}

std::optional<EntityInstance> FromAccessibilityAnnotator(
    const aa::Entity& entity) {
  using enum AttributeTypeName;

  std::optional<EntityType> entity_type =
      FromAccessibilityAnnotator(entity.GetType());
  if (!entity_type) {
    return std::nullopt;
  }

  std::vector<AttributeInstance> attributes;
  // Populates `attributes` with the members of `entity.specifics`.
  //
  // When adding new entities or attributes that are not supported by Autofill,
  // and "Bug: b:480204898" to the CL description and add a comment
  // "// TODO(b/480204898): To be added." or "// Out of scope for autofill."
  //
  // LINT.IfChange(AttributeConversions)
  std::visit(
      absl::Overload{
          [&](const aa::FlightReservation& src) {
            auto add = [&](auto member, AttributeTypeName type_name) {
              AddAttributeInstance(src, member, type_name, attributes);
            };
            add(&aa::FlightReservation::flight_number,
                kFlightReservationFlightNumber);
            add(&aa::FlightReservation::ticket_number,
                kFlightReservationTicketNumber);
            add(&aa::FlightReservation::confirmation_code,
                kFlightReservationConfirmationCode);
            add(&aa::FlightReservation::passenger_name,
                kFlightReservationPassengerName);
            add(&aa::FlightReservation::departure_airport,
                kFlightReservationDepartureAirport);
            add(&aa::FlightReservation::arrival_airport,
                kFlightReservationArrivalAirport);
            add(&aa::FlightReservation::departure_date,
                kFlightReservationDepartureDate);
            // aa::FlightReservation::arrival_date has no match in Autofill AI.
          },

          [&](const aa::Order& src) {
            auto add = [&](auto member, AttributeTypeName type_name) {
              AddAttributeInstance(src, member, type_name, attributes);
            };
            add(&aa::Order::id, kOrderId);
            add(&aa::Order::account, kOrderAccount);
            add(&aa::Order::order_date, kOrderDate);
            add(&aa::Order::merchant_name, kOrderMerchantName);
            add(&aa::Order::merchant_domain, kOrderMerchantDomain);
            add(&aa::Order::products, kOrderProductNames);
          },

          [&](const aa::Shipment& src) {
            auto add = [&](auto member, AttributeTypeName type_name) {
              AddAttributeInstance(src, member, type_name, attributes);
            };
            add(&aa::Shipment::tracking_number, kShipmentTrackingNumber);
            add(&aa::Shipment::associated_order_id, kShipmentOrderIds);
            add(&aa::Shipment::carrier_name, kShipmentCarrierName);
            add(&aa::Shipment::carrier_domain, kShipmentCarrierDomain);
            add(&aa::Shipment::estimated_delivery_date,
                kShipmentEstimatedDeliveryDate);
            add(&aa::Shipment::delivery_zip_code, kShipmentDeliveryZipCode);
          },

          [&](const aa::Passport& src) {
            auto add = [&](auto member, AttributeTypeName type_name) {
              AddAttributeInstance(src, member, type_name, attributes);
            };
            add(&aa::Passport::name, kPassportName);
            add(&aa::Passport::number, kPassportNumber);
            add(&aa::Passport::expiration_date, kPassportExpirationDate);
            add(&aa::Passport::issue_date, kPassportIssueDate);
            add(&aa::Passport::issuing_country, kPassportCountry);
          },

          [&](const aa::DriversLicense& src) {
            auto add = [&](auto member, AttributeTypeName type_name) {
              AddAttributeInstance(src, member, type_name, attributes);
            };
            add(&aa::DriversLicense::name, kDriversLicenseName);
            add(&aa::DriversLicense::number, kDriversLicenseNumber);
            add(&aa::DriversLicense::expiration_date,
                kDriversLicenseExpirationDate);
            add(&aa::DriversLicense::issue_date, kDriversLicenseIssueDate);
            add(&aa::DriversLicense::state, kDriversLicenseState);
          },

          [&](const aa::NationalId& src) {
            auto add = [&](auto member, AttributeTypeName type_name) {
              AddAttributeInstance(src, member, type_name, attributes);
            };
            add(&aa::NationalId::name, kNationalIdCardName);
            add(&aa::NationalId::number, kNationalIdCardNumber);
            add(&aa::NationalId::expiration_date,
                kNationalIdCardExpirationDate);
            add(&aa::NationalId::issue_date, kNationalIdCardIssueDate);
            add(&aa::NationalId::issuing_country, kNationalIdCardCountry);
          },

          [&](const aa::Vehicle& src) {
            auto add = [&](auto member, AttributeTypeName type_name) {
              AddAttributeInstance(src, member, type_name, attributes);
            };
            add(&aa::Vehicle::model, kVehicleModel);
            add(&aa::Vehicle::make, kVehicleMake);
            add(&aa::Vehicle::year, kVehicleYear);
            add(&aa::Vehicle::owner, kVehicleOwner);
            add(&aa::Vehicle::plate_number, kVehiclePlateNumber);
            add(&aa::Vehicle::plate_state, kVehiclePlateState);
            add(&aa::Vehicle::vin, kVehicleVin);
          }},
      entity.specifics);
  // LINT.ThenChange(//components/accessibility_annotator/core/data_models/entity.h:AttributeDefinitions)
  if (attributes.empty()) {
    return std::nullopt;
  }

  return EntityInstance(*entity_type, std::move(attributes),
                        EntityInstance::EntityId(entity.entity_id),
                        /*nickname=*/"",
                        /*date_modified=*/base::Time::Now(),
                        /*use_count=*/0,
                        /*use_date=*/base::Time::FromTimeT(0),
                        EntityInstance::RecordType::kAccessibilityAnnotator,
                        EntityInstance::AreAttributesReadOnly(true),
                        /*frecency_override=*/"");
}

aa::EntryType AttributeTypeToEntryType(AttributeType type) {
#define ATTRIBUTE_TO_QUERY_INTENT(name) \
  case AttributeTypeName::name:         \
    return aa::EntryType::name

  switch (type.name()) {
    ATTRIBUTE_TO_QUERY_INTENT(kDriversLicenseName);
    ATTRIBUTE_TO_QUERY_INTENT(kDriversLicenseState);
    ATTRIBUTE_TO_QUERY_INTENT(kDriversLicenseNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kDriversLicenseIssueDate);
    ATTRIBUTE_TO_QUERY_INTENT(kDriversLicenseExpirationDate);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationPassengerName);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationFlightNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationTicketNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationConfirmationCode);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationDepartureAirport);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationArrivalAirport);
    ATTRIBUTE_TO_QUERY_INTENT(kFlightReservationDepartureDate);
    ATTRIBUTE_TO_QUERY_INTENT(kKnownTravelerNumberName);
    ATTRIBUTE_TO_QUERY_INTENT(kKnownTravelerNumberNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kKnownTravelerNumberExpirationDate);
    ATTRIBUTE_TO_QUERY_INTENT(kNationalIdCardName);
    ATTRIBUTE_TO_QUERY_INTENT(kNationalIdCardCountry);
    ATTRIBUTE_TO_QUERY_INTENT(kNationalIdCardNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kNationalIdCardIssueDate);
    ATTRIBUTE_TO_QUERY_INTENT(kNationalIdCardExpirationDate);
    ATTRIBUTE_TO_QUERY_INTENT(kOrderAccount);
    ATTRIBUTE_TO_QUERY_INTENT(kOrderDate);
    ATTRIBUTE_TO_QUERY_INTENT(kOrderId);
    ATTRIBUTE_TO_QUERY_INTENT(kOrderMerchantDomain);
    ATTRIBUTE_TO_QUERY_INTENT(kOrderMerchantName);
    ATTRIBUTE_TO_QUERY_INTENT(kOrderProductNames);
    ATTRIBUTE_TO_QUERY_INTENT(kPassportName);
    ATTRIBUTE_TO_QUERY_INTENT(kPassportCountry);
    ATTRIBUTE_TO_QUERY_INTENT(kPassportNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kPassportIssueDate);
    ATTRIBUTE_TO_QUERY_INTENT(kPassportExpirationDate);
    ATTRIBUTE_TO_QUERY_INTENT(kRedressNumberName);
    ATTRIBUTE_TO_QUERY_INTENT(kRedressNumberNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kVehicleOwner);
    ATTRIBUTE_TO_QUERY_INTENT(kVehiclePlateNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kVehiclePlateState);
    ATTRIBUTE_TO_QUERY_INTENT(kVehicleVin);
    ATTRIBUTE_TO_QUERY_INTENT(kVehicleMake);
    ATTRIBUTE_TO_QUERY_INTENT(kVehicleModel);
    ATTRIBUTE_TO_QUERY_INTENT(kVehicleYear);
    ATTRIBUTE_TO_QUERY_INTENT(kShipmentCarrierName);
    ATTRIBUTE_TO_QUERY_INTENT(kShipmentCarrierDomain);
    ATTRIBUTE_TO_QUERY_INTENT(kShipmentTrackingNumber);
    ATTRIBUTE_TO_QUERY_INTENT(kShipmentEstimatedDeliveryDate);
    case AttributeTypeName::kShipmentOrderIds:
      return aa::EntryType::kShipmentAssociatedOrderId;
    case AttributeTypeName::kShipmentOrderDates:
    case AttributeTypeName::kShipmentMerchantName:
    case AttributeTypeName::kShipmentProductNames:
    case AttributeTypeName::kShipmentDeliveryZipCode:
      // TODO(crbug.com/484094746): Map `delivery_address` to
      // `kShipmentDeliveryZipCode`. Since `delivery_address` is a
      // `std::string`, it's unclear how we can process this (here and in
      // general).
      return aa::EntryType::kUnknown;
  }
#undef ATTRIBUTE_TO_QUERY_INTENT
  return aa::EntryType::kUnknown;
}

std::u16string GetEntryTypeNameForI18n(aa::EntryType type) {
  switch (type) {
    case aa::EntryType::kUnknown:
      return u"";
    // Field types:
    // TODO(crbug.com/481979475): Use internationalization for these strings.
    case aa::EntryType::kNameFull:
      return u"Name";
    case aa::EntryType::kAddressFull:
      return u"Address";
    case aa::EntryType::kAddressStreetAddress:
      return u"Street address";
    case aa::EntryType::kAddressCity:
      return u"City";
    case aa::EntryType::kAddressState:
      return u"State";
    case aa::EntryType::kAddressZip:
      return u"Zip";
    case aa::EntryType::kAddressCountry:
      return u"Country";
    case aa::EntryType::kPhone:
      return u"Phone";
    case aa::EntryType::kEmail:
      return u"Email";
    case aa::EntryType::kCompanyName:
      return u"Company";
    case aa::EntryType::kIban:
      return u"IBAN";
    case aa::EntryType::kIbanNickname:
      return u"Name";
    case aa::EntryType::kCreditCardNumber:
      return u"Card number";
    case aa::EntryType::kCreditCardExpirationDate:
      return u"Expiration date";
    case aa::EntryType::kCreditCardSecurityCode:
      return u"Security code";
    case aa::EntryType::kCreditCardNameOnCard:
      return u"Name on card";
    case aa::EntryType::kCreditCardNickname:
      return u"Card Nickname";
    // Entity types:
    case aa::EntryType::kVehicle:
    case aa::EntryType::kPassportFull:
    case aa::EntryType::kFlightReservationFull:
    case aa::EntryType::kNationalIdCardFull:
    case aa::EntryType::kRedressNumberFull:
    case aa::EntryType::kKnownTravelerNumberFull:
    case aa::EntryType::kDriversLicenseFull:
    case aa::EntryType::kOrderFull:
    case aa::EntryType::kShipmentFull: {
      std::optional<AtMemoryDataType> data_type = ToAtMemoryDataType(type);
      const auto* entity_type =
          data_type ? std::get_if<EntityType>(&*data_type) : nullptr;
      return entity_type ? entity_type->GetNameForI18n() : u"";
    }
    // Attribute types:
    case aa::EntryType::kVehicleMake:
    case aa::EntryType::kVehicleModel:
    case aa::EntryType::kVehicleYear:
    case aa::EntryType::kVehicleOwner:
    case aa::EntryType::kVehiclePlateNumber:
    case aa::EntryType::kVehiclePlateState:
    case aa::EntryType::kVehicleVin:
    case aa::EntryType::kPassportName:
    case aa::EntryType::kPassportCountry:
    case aa::EntryType::kPassportNumber:
    case aa::EntryType::kPassportIssueDate:
    case aa::EntryType::kPassportExpirationDate:
    case aa::EntryType::kFlightReservationFlightNumber:
    case aa::EntryType::kFlightReservationTicketNumber:
    case aa::EntryType::kFlightReservationConfirmationCode:
    case aa::EntryType::kFlightReservationPassengerName:
    case aa::EntryType::kFlightReservationDepartureAirport:
    case aa::EntryType::kFlightReservationArrivalAirport:
    case aa::EntryType::kFlightReservationDepartureDate:
    case aa::EntryType::kFlightReservationArrivalDate:
    case aa::EntryType::kNationalIdCardName:
    case aa::EntryType::kNationalIdCardCountry:
    case aa::EntryType::kNationalIdCardNumber:
    case aa::EntryType::kNationalIdCardIssueDate:
    case aa::EntryType::kNationalIdCardExpirationDate:
    case aa::EntryType::kRedressNumberName:
    case aa::EntryType::kRedressNumberNumber:
    case aa::EntryType::kKnownTravelerNumberName:
    case aa::EntryType::kKnownTravelerNumberNumber:
    case aa::EntryType::kKnownTravelerNumberExpirationDate:
    case aa::EntryType::kDriversLicenseName:
    case aa::EntryType::kDriversLicenseState:
    case aa::EntryType::kDriversLicenseNumber:
    case aa::EntryType::kDriversLicenseIssueDate:
    case aa::EntryType::kDriversLicenseExpirationDate:
    case aa::EntryType::kOrderId:
    case aa::EntryType::kOrderAccount:
    case aa::EntryType::kOrderDate:
    case aa::EntryType::kOrderMerchantName:
    case aa::EntryType::kOrderMerchantDomain:
    case aa::EntryType::kOrderProductNames:
    case aa::EntryType::kOrderGrandTotal:
    case aa::EntryType::kShipmentTrackingNumber:
    case aa::EntryType::kShipmentAssociatedOrderId:
    case aa::EntryType::kShipmentDeliveryAddress:
    case aa::EntryType::kShipmentDeliveryZipCode:
    case aa::EntryType::kShipmentCarrierName:
    case aa::EntryType::kShipmentCarrierDomain:
    case aa::EntryType::kShipmentEstimatedDeliveryDate: {
      std::optional<AtMemoryDataType> data_type = ToAtMemoryDataType(type);
      const auto* attribute_type =
          data_type ? std::get_if<AttributeType>(&*data_type) : nullptr;
      return attribute_type ? attribute_type->GetNameForI18n() : u"";
    }
  }
}

}  // namespace autofill
