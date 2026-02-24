// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"

#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "url/gurl.h"

namespace autofill {

namespace {

using sync_pb::AutofillValuableMetadataSpecifics;
using sync_pb::AutofillValuableSpecifics;

void TrimLoyaltyCard(sync_pb::LoyaltyCard& card) {
  card.clear_merchant_name();
  card.clear_program_name();
  card.clear_program_logo();
  card.clear_loyalty_card_number();
  card.clear_merchant_domains();
}

void TrimVehicleRegistration(sync_pb::VehicleRegistration& vehicle) {
  vehicle.clear_vehicle_make();
  vehicle.clear_vehicle_model();
  vehicle.clear_vehicle_year();
  vehicle.clear_vehicle_identification_number();
  vehicle.clear_vehicle_license_plate();
  vehicle.clear_license_plate_region();
  vehicle.clear_license_plate_country();
  vehicle.clear_owner_name();
  vehicle.clear_issuer_name();
}

void TrimFlightReservation(sync_pb::FlightReservation& flight_reservation) {
  flight_reservation.clear_flight_number();
  flight_reservation.clear_flight_ticket_number();
  flight_reservation.clear_flight_confirmation_code();
  flight_reservation.clear_passenger_name();
  flight_reservation.clear_departure_airport();
  flight_reservation.clear_arrival_airport();
  flight_reservation.clear_departure_date_unix_epoch_micros();
  flight_reservation.clear_arrival_date_unix_epoch_micros();
  flight_reservation.clear_airline_logo();
  flight_reservation.clear_carrier_code();
  flight_reservation.clear_departure_airport_utc_offset_seconds();
  flight_reservation.clear_arrival_airport_utc_offset_seconds();
}

void TrimPassport(sync_pb::Passport& passport) {
  passport.clear_owner_name();
  passport.clear_masked_number();
  passport.clear_country_code();
  passport.clear_issue_date();
  passport.clear_expiration_date();
}

void TrimDriverLicense(sync_pb::DriverLicense& driver_license) {
  driver_license.clear_owner_name();
  driver_license.clear_masked_number();
  driver_license.clear_region();
  driver_license.clear_issue_date();
  driver_license.clear_expiration_date();
}

void TrimNationalIdCard(sync_pb::NationalIdCard& national_id_card) {
  national_id_card.clear_owner_name();
  national_id_card.clear_masked_number();
  national_id_card.clear_country_code();
  national_id_card.clear_issue_date();
  national_id_card.clear_expiration_date();
}

void TrimRedressNumber(sync_pb::RedressNumber& redress_number) {
  redress_number.clear_owner_name();
  redress_number.clear_masked_number();
}

void TrimKnownTravelerNumber(sync_pb::KnownTravelerNumber& ktn) {
  ktn.clear_owner_name();
  ktn.clear_masked_number();
  ktn.clear_expiration_date();
}

}  // namespace

std::unique_ptr<syncer::EntityData> CreateEntityDataFromLoyaltyCard(
    const LoyaltyCard& loyalty_card,
    const sync_pb::AutofillValuableSpecifics& base_specifics) {
  AutofillValuableSpecifics card_specifics =
      CreateSpecificsFromLoyaltyCard(loyalty_card, base_specifics);
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = card_specifics.id();
  *entity_data->specifics.mutable_autofill_valuable() =
      std::move(card_specifics);

  return entity_data;
}

AutofillValuableSpecifics CreateSpecificsFromLoyaltyCard(
    const LoyaltyCard& card,
    const sync_pb::AutofillValuableSpecifics& base_specifics) {
  AutofillValuableSpecifics specifics = base_specifics;
  specifics.set_id(card.id().value());
  sync_pb::LoyaltyCard* loyalty_card = specifics.mutable_loyalty_card();
  loyalty_card->set_merchant_name(card.merchant_name());
  loyalty_card->set_program_name(card.program_name());
  loyalty_card->set_program_logo(card.program_logo().possibly_invalid_spec());
  loyalty_card->set_loyalty_card_number(card.loyalty_card_number());
  for (const GURL& merchant_domain : card.merchant_domains()) {
    *loyalty_card->add_merchant_domains() = merchant_domain.spec();
  }

  return specifics;
}

LoyaltyCard CreateAutofillLoyaltyCardFromSpecifics(
    const AutofillValuableSpecifics& specifics) {
  // Since the specifics are guaranteed to be valid by `IsEntityDataValid()`,
  // the conversion will succeed.
  const auto& repeated_domains = specifics.loyalty_card().merchant_domains();
  std::vector<GURL> domains(repeated_domains.begin(), repeated_domains.end());
  return LoyaltyCard(
      ValuableId(specifics.id()), specifics.loyalty_card().merchant_name(),
      specifics.loyalty_card().program_name(),
      GURL(specifics.loyalty_card().program_logo()),
      specifics.loyalty_card().loyalty_card_number(), std::move(domains),
      /*use_date=*/{}, /*use_count=*/0);
}

std::unique_ptr<syncer::EntityData> CreateEntityDataFromValuableMetadata(
    const ValuableMetadata& metadata,
    const AutofillValuableMetadataSpecifics::PassType pass_type,
    const sync_pb::AutofillValuableMetadataSpecifics& base_specifics) {
  sync_pb::AutofillValuableMetadataSpecifics metadata_specifics =
      CreateSpecificsFromValuableMetadata(metadata, pass_type, base_specifics);
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = metadata_specifics.valuable_id();
  *entity_data->specifics.mutable_autofill_valuable_metadata() =
      std::move(metadata_specifics);
  return entity_data;
}

sync_pb::AutofillValuableMetadataSpecifics CreateSpecificsFromValuableMetadata(
    const ValuableMetadata& metadata,
    const AutofillValuableMetadataSpecifics::PassType pass_type,
    const sync_pb::AutofillValuableMetadataSpecifics& base_specifics) {
  sync_pb::AutofillValuableMetadataSpecifics specifics = base_specifics;
  specifics.set_valuable_id(*metadata.valuable_id);
  specifics.set_use_count(metadata.use_count);
  specifics.set_last_used_date_unix_epoch_micros(
      metadata.use_date.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_pass_type(pass_type);
  return specifics;
}

AutofillValuableSpecifics TrimAutofillValuableSpecificsDataForCaching(
    const AutofillValuableSpecifics& specifics) {
  // LINT.IfChange(TrimAutofillValuableSpecificsDataForCaching)
  AutofillValuableSpecifics trimmed_specifics =
      AutofillValuableSpecifics(specifics);
  trimmed_specifics.clear_id();
  trimmed_specifics.clear_is_editable();
  trimmed_specifics.clear_serialized_chrome_valuables_metadata();

  switch (trimmed_specifics.valuable_data_case()) {
    case AutofillValuableSpecifics::kLoyaltyCard: {
      TrimLoyaltyCard(*trimmed_specifics.mutable_loyalty_card());
      if (trimmed_specifics.loyalty_card().ByteSizeLong() == 0) {
        trimmed_specifics.clear_loyalty_card();
      }
      break;
    }
    case AutofillValuableSpecifics::kVehicleRegistration: {
      TrimVehicleRegistration(
          *trimmed_specifics.mutable_vehicle_registration());
      if (trimmed_specifics.vehicle_registration().ByteSizeLong() == 0) {
        trimmed_specifics.clear_vehicle_registration();
      }
      break;
    }
    case AutofillValuableSpecifics::kFlightReservation: {
      TrimFlightReservation(*trimmed_specifics.mutable_flight_reservation());
      if (trimmed_specifics.flight_reservation().ByteSizeLong() == 0) {
        trimmed_specifics.clear_flight_reservation();
      }
      break;
    }
    case AutofillValuableSpecifics::kPassport: {
      TrimPassport(*trimmed_specifics.mutable_passport());
      if (trimmed_specifics.passport().ByteSizeLong() == 0) {
        trimmed_specifics.clear_passport();
      }
      break;
    }
    case AutofillValuableSpecifics::kDriverLicense: {
      TrimDriverLicense(*trimmed_specifics.mutable_driver_license());
      if (trimmed_specifics.driver_license().ByteSizeLong() == 0) {
        trimmed_specifics.clear_driver_license();
      }
      break;
    }
    case AutofillValuableSpecifics::kNationalIdCard: {
      TrimNationalIdCard(*trimmed_specifics.mutable_national_id_card());
      if (trimmed_specifics.national_id_card().ByteSizeLong() == 0) {
        trimmed_specifics.clear_national_id_card();
      }
      break;
    }
    case AutofillValuableSpecifics::kRedressNumber: {
      TrimRedressNumber(*trimmed_specifics.mutable_redress_number());
      if (trimmed_specifics.redress_number().ByteSizeLong() == 0) {
        trimmed_specifics.clear_redress_number();
      }
      break;
    }
    case AutofillValuableSpecifics::kKnownTravelerNumber: {
      TrimKnownTravelerNumber(
          *trimmed_specifics.mutable_known_traveler_number());
      if (trimmed_specifics.known_traveler_number().ByteSizeLong() == 0) {
        trimmed_specifics.clear_known_traveler_number();
      }
      break;
    }
    case AutofillValuableSpecifics::VALUABLE_DATA_NOT_SET:
      break;
  }

  // LINT.ThenChange(//components/sync/protocol/autofill_valuable_specifics.proto:AutofillValuableSpecifics)
  return trimmed_specifics;
}

AutofillValuableMetadataSpecifics
TrimAutofillValuableMetadataSpecificsDataForCaching(
    const sync_pb::AutofillValuableMetadataSpecifics& specifics) {
  // LINT.IfChange(TrimAutofillValuableMetadataSpecificsDataForCaching)
  AutofillValuableMetadataSpecifics trimmed_specifics(specifics);
  trimmed_specifics.clear_valuable_id();
  trimmed_specifics.clear_use_count();
  trimmed_specifics.clear_last_used_date_unix_epoch_micros();
  trimmed_specifics.clear_last_modified_date_unix_epoch_micros();
  trimmed_specifics.clear_pass_type();
  // LINT.ThenChange(//components/sync/protocol/autofill_valuable_metadata_specifics.proto:AutofillValuableMetadataSpecifics)
  return trimmed_specifics;
}

}  // namespace autofill
