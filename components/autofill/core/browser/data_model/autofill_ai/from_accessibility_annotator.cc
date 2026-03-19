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
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
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
        // TODO(b/484094746): Map to `EntityTypeName::kShipment` once Autofill
        // supports it.
        return std::nullopt;
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
            add(&aa::Order::grand_total, kOrderGrandTotal);
          },

          [](const aa::Shipment& src) {
            // TODO(b/484094746): Map to `EntityTypeName::kShipment` once
            // Autofill supports it.
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

}  // namespace autofill
