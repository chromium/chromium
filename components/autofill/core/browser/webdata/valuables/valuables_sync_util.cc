// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"

#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "url/gurl.h"

namespace autofill {

using sync_pb::AutofillValuableMetadataSpecifics;
using sync_pb::AutofillValuableSpecifics;

AutofillValuableSpecifics CreateSpecificsFromLoyaltyCard(
    const LoyaltyCard& card) {
  AutofillValuableSpecifics specifics = sync_pb::AutofillValuableSpecifics();
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
      specifics.loyalty_card().loyalty_card_number(), std::move(domains));
}

std::unique_ptr<syncer::EntityData> CreateEntityDataFromLoyaltyCard(
    const LoyaltyCard& loyalty_card) {
  AutofillValuableSpecifics card_specifics =
      CreateSpecificsFromLoyaltyCard(loyalty_card);
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = card_specifics.id();
  AutofillValuableSpecifics* specifics =
      entity_data->specifics.mutable_autofill_valuable();
  specifics->CopyFrom(card_specifics);

  return entity_data;
}

std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntityInstance(
    EntityInstance entity) {
  sync_pb::AutofillValuableSpecifics valuable_specifics =
      CreateSpecificsFromEntityInstance(entity);
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = valuable_specifics.id();
  AutofillValuableSpecifics* specifics =
      entity_data->specifics.mutable_autofill_valuable();
  specifics->CopyFrom(valuable_specifics);

  return entity_data;
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
      trimmed_specifics.mutable_loyalty_card()->clear_merchant_name();
      trimmed_specifics.mutable_loyalty_card()->clear_program_name();
      trimmed_specifics.mutable_loyalty_card()->clear_program_logo();
      trimmed_specifics.mutable_loyalty_card()->clear_loyalty_card_number();
      trimmed_specifics.mutable_loyalty_card()->clear_merchant_domains();
      if (trimmed_specifics.mutable_loyalty_card()->ByteSizeLong() == 0) {
        trimmed_specifics.clear_loyalty_card();
      }
      break;
    }
    case AutofillValuableSpecifics::kVehicleRegistration: {
      trimmed_specifics.mutable_vehicle_registration()->clear_vehicle_make();
      trimmed_specifics.mutable_vehicle_registration()->clear_vehicle_model();
      trimmed_specifics.mutable_vehicle_registration()->clear_vehicle_year();
      trimmed_specifics.mutable_vehicle_registration()
          ->clear_vehicle_identification_number();
      trimmed_specifics.mutable_vehicle_registration()
          ->clear_vehicle_license_plate();
      trimmed_specifics.mutable_vehicle_registration()
          ->clear_license_plate_region();
      trimmed_specifics.mutable_vehicle_registration()
          ->clear_license_plate_country();
      trimmed_specifics.mutable_vehicle_registration()->clear_owner_name();
      if (trimmed_specifics.mutable_vehicle_registration()->ByteSizeLong() ==
          0) {
        trimmed_specifics.clear_vehicle_registration();
      }
      break;
    }
    case AutofillValuableSpecifics::kFlightReservation: {
      trimmed_specifics.mutable_flight_reservation()->clear_flight_number();
      trimmed_specifics.mutable_flight_reservation()
          ->clear_flight_ticket_number();
      trimmed_specifics.mutable_flight_reservation()
          ->clear_flight_confirmation_code();
      trimmed_specifics.mutable_flight_reservation()->clear_passenger_name();
      trimmed_specifics.mutable_flight_reservation()->clear_departure_airport();
      trimmed_specifics.mutable_flight_reservation()->clear_arrival_airport();
      trimmed_specifics.mutable_flight_reservation()
          ->clear_departure_date_unix_epoch_micros();
      trimmed_specifics.mutable_flight_reservation()
          ->clear_arrival_date_unix_epoch_micros();
      trimmed_specifics.mutable_flight_reservation()->clear_airline_logo();
      trimmed_specifics.mutable_flight_reservation()->clear_carrier_code();
      trimmed_specifics.mutable_flight_reservation()
          ->clear_departure_airport_utc_offset_seconds();
      trimmed_specifics.mutable_flight_reservation()
          ->clear_arrival_airport_utc_offset_seconds();
      if (trimmed_specifics.mutable_flight_reservation()->ByteSizeLong() == 0) {
        trimmed_specifics.clear_flight_reservation();
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
  // LINT.ThenChange(//components/sync/protocol/autofill_valuable_metadata_specifics.proto:AutofillValuableMetadataSpecifics)
  return trimmed_specifics;
}

}  // namespace autofill
