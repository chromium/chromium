// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_PERSONAL_CONTEXT_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_PERSONAL_CONTEXT_TEST_UTIL_H_

#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/personal_context/proto/features/common_data.pb.h"

namespace autofill::test {

// Creates a test passport proto entity with values from `options`.
personal_context::proto::Entity CreatePassportProto(
    PassportEntityOptions options = {});

// Creates a test driver's license proto entity with values from `options`.
personal_context::proto::Entity CreateDriversLicenseProto(
    DriversLicenseOptions options = {});

// Creates a test national ID card proto entity with values from `options`.
personal_context::proto::Entity CreateNationalIdProto(
    NationalIdCardOptions options = {});

// Creates a test order proto entity with values from `options`.
personal_context::proto::Entity CreateOrderProto(OrderOptions options = {});

// Creates a test shipment proto entity with values from `options`.
personal_context::proto::Entity CreateShipmentProto(
    ShipmentOptions options = {});

// Creates a test flight reservation proto entity with values from `options`.
personal_context::proto::Entity CreateFlightReservationProto(
    FlightReservationOptions options = {});

// Creates a test vehicle proto entity with values from `options`.
personal_context::proto::Entity CreateVehicleProto(VehicleOptions options = {});

}  // namespace autofill::test

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_PERSONAL_CONTEXT_TEST_UTIL_H_
