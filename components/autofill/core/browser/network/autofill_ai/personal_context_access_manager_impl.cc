// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/ambient_autofill.pb.h"

namespace autofill {

namespace {

personal_context::proto::EntityType
AutofillEntityTypeToPersonalContextEntityType(EntityType type) {
  switch (type.name()) {
    case EntityTypeName::kOrder:
      return personal_context::proto::EntityType::ORDER;
    case EntityTypeName::kShipment:
      return personal_context::proto::EntityType::SHIPMENT;
    case EntityTypeName::kDriversLicense:
      return personal_context::proto::EntityType::DRIVERS_LICENSE;
    case EntityTypeName::kPassport:
      return personal_context::proto::EntityType::PASSPORT;
    case EntityTypeName::kNationalIdCard:
      return personal_context::proto::EntityType::NATIONAL_ID;
    case EntityTypeName::kFlightReservation:
      return personal_context::proto::EntityType::FLIGHT_RESERVATION;
    case EntityTypeName::kVehicle:
      return personal_context::proto::EntityType::VEHICLE;
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      return personal_context::proto::EntityType::UNSPECIFIED;
  }
}

}  // namespace

PersonalContextAccessManagerImpl::PersonalContextAccessManagerImpl(
    personal_context::PersonalContextService* personal_context_service,
    personal_context::PersonalContextEnablementService*
        personal_context_enablement_service)
    : personal_context_service_(CHECK_DEREF(personal_context_service)),
      personal_context_enablement_service_(
          CHECK_DEREF(personal_context_enablement_service)) {}

PersonalContextAccessManagerImpl::~PersonalContextAccessManagerImpl() = default;

void PersonalContextAccessManagerImpl::FetchAmbientAutofillContext(
    base::span<const EntityType> requested_types,
    FetchAmbientAutofillContextCallback callback) {
  personal_context::proto::ContextMemoryAmbientAutofillRequest request;
  for (const EntityType& type : requested_types) {
    request.add_requested_types(
        AutofillEntityTypeToPersonalContextEntityType(type));
  }

  auto fetch_callback = [](FetchAmbientAutofillContextCallback callback,
                           personal_context::FetchContextResult result) {
    if (!result.response.has_value()) {
      std::move(callback).Run(base::unexpected(result.response.error()));
      return;
    }

    personal_context::proto::Any any_response = result.response.value();
    // TODO(crbug.com/516721244): Parse the response.
    std::move(callback).Run(any_response.value());
  };

  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, request,
      /*options=*/{},
      base::BindOnce(std::move(fetch_callback), std::move(callback)));
}

}  // namespace autofill
