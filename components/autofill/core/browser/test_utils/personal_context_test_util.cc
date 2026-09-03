// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_utils/personal_context_test_util.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/data_model_util.h"
#include "components/personal_context/proto/features/common_data.pb.h"

namespace autofill::test {

personal_context::proto::Entity CreatePassportProto(
    PassportEntityOptions options) {
  personal_context::proto::Entity entity;
  auto* p = entity.mutable_passport();
  if (options.number) {
    p->set_number(base::UTF16ToUTF8(options.number));
  }
  if (options.name) {
    p->set_name(base::UTF16ToUTF8(options.name));
  }
  if (options.country) {
    p->set_issuing_country(base::UTF16ToUTF8(options.country));
  }
  if (options.expiry_date) {
    data_util::Date date;
    if (data_util::ParseDate(options.expiry_date, u"YYYY-MM-DD", date)) {
      p->mutable_expiration_date()->set_year(date.year);
      p->mutable_expiration_date()->set_month(date.month);
      p->mutable_expiration_date()->set_day(date.day);
    }
  }
  return entity;
}

personal_context::proto::Entity CreateDriversLicenseProto(
    DriversLicenseOptions options) {
  personal_context::proto::Entity entity;
  auto* dl = entity.mutable_drivers_license();
  if (options.number) {
    dl->set_number(base::UTF16ToUTF8(options.number));
  }
  if (options.name) {
    dl->set_name(base::UTF16ToUTF8(options.name));
  }
  if (options.region) {
    dl->set_state(base::UTF16ToUTF8(options.region));
  }
  if (options.expiration_date) {
    data_util::Date date;
    if (data_util::ParseDate(options.expiration_date, u"DD/MM/YYYY", date)) {
      dl->mutable_expiration_date()->set_year(date.year);
      dl->mutable_expiration_date()->set_month(date.month);
      dl->mutable_expiration_date()->set_day(date.day);
    }
  }
  return entity;
}

personal_context::proto::Entity CreateNationalIdProto(
    NationalIdCardOptions options) {
  personal_context::proto::Entity entity;
  auto* nid = entity.mutable_national_id();
  if (options.number) {
    nid->set_number(base::UTF16ToUTF8(options.number));
  }
  if (options.name) {
    nid->set_name(base::UTF16ToUTF8(options.name));
  }
  if (options.country) {
    nid->set_issuing_country(base::UTF16ToUTF8(options.country));
  }
  if (options.expiry_date) {
    data_util::Date date;
    if (data_util::ParseDate(options.expiry_date, u"DD/MM/YYYY", date)) {
      nid->mutable_expiration_date()->set_year(date.year);
      nid->mutable_expiration_date()->set_month(date.month);
      nid->mutable_expiration_date()->set_day(date.day);
    }
  }
  return entity;
}

personal_context::proto::Entity CreateOrderProto(OrderOptions options) {
  personal_context::proto::Entity entity;
  auto* o = entity.mutable_order();
  if (options.id) {
    o->set_order_id(base::UTF16ToUTF8(options.id));
  }
  if (options.merchant_name) {
    o->set_merchant_name(base::UTF16ToUTF8(options.merchant_name));
  }
  if (options.date) {
    data_util::Date date;
    if (data_util::ParseDate(options.date, u"YYYY-MM-DD", date)) {
      o->mutable_order_date()->set_year(date.year);
      o->mutable_order_date()->set_month(date.month);
      o->mutable_order_date()->set_day(date.day);
    }
  }
  return entity;
}

personal_context::proto::Entity CreateShipmentProto(ShipmentOptions options) {
  personal_context::proto::Entity entity;
  auto* s = entity.mutable_shipment();
  if (options.tracking_number) {
    s->set_tracking_number(base::UTF16ToUTF8(options.tracking_number));
  }
  if (options.merchant_name) {
    s->set_merchant_name(base::UTF16ToUTF8(options.merchant_name));
  }
  if (options.shipped_date) {
    data_util::Date date;
    if (data_util::ParseDate(options.shipped_date, u"YYYY-MM-DD", date)) {
      s->mutable_ship_date()->set_year(date.year);
      s->mutable_ship_date()->set_month(date.month);
      s->mutable_ship_date()->set_day(date.day);
    }
  }
  return entity;
}

personal_context::proto::Entity CreateFlightReservationProto(
    FlightReservationOptions options) {
  personal_context::proto::Entity entity;
  auto* f = entity.mutable_flight_reservation();
  if (options.flight_number) {
    f->set_flight_number(base::UTF16ToUTF8(options.flight_number));
  }
  if (options.ticket_number) {
    f->set_flight_ticket_number(base::UTF16ToUTF8(options.ticket_number));
  }
  if (options.confirmation_code) {
    f->set_flight_confirmation_code(
        base::UTF16ToUTF8(options.confirmation_code));
  }
  if (options.departure_time) {
    base::Time::Exploded exploded;
    options.departure_time->LocalExplode(&exploded);
    f->mutable_departure_time()->set_year(exploded.year);
    f->mutable_departure_time()->set_month(exploded.month);
    f->mutable_departure_time()->set_day(exploded.day_of_month);
  }
  return entity;
}

personal_context::proto::Entity CreateVehicleProto(VehicleOptions options) {
  personal_context::proto::Entity entity;
  auto* v = entity.mutable_vehicle();
  if (options.number) {
    v->set_vehicle_identification_number(base::UTF16ToUTF8(options.number));
  }
  if (options.plate) {
    v->set_vehicle_license_plate(base::UTF16ToUTF8(options.plate));
  }
  if (options.make) {
    v->set_vehicle_make(base::UTF16ToUTF8(options.make));
  }
  if (options.model) {
    v->set_vehicle_model(base::UTF16ToUTF8(options.model));
  }
  return entity;
}

}  // namespace autofill::test
