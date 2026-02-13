// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/private_pass_conversion_util.h"

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/proto/autofill_ai_chrome_metadata.pb.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"
#include "components/wallet/core/browser/proto/common.pb.h"
#include "components/wallet/core/browser/proto/private_pass.pb.h"

namespace autofill {

namespace {

using ::wallet::Any;
using ::wallet::PrivatePass;
using DriverLicense = ::wallet::PrivatePass::DriverLicense;
using IdCard = ::wallet::PrivatePass::IdCard;
using KnownTravelerNumber = ::wallet::PrivatePass::KnownTravelerNumber;
using Passport = ::wallet::PrivatePass::Passport;
using RedressNumber = ::wallet::PrivatePass::RedressNumber;

Any SerializeMetadata(const EntityInstance& entity) {
  ChromeValuablesMetadata metadata = SerializeChromeValuablesMetadata(entity);
  Any serialised_metadata;
  serialised_metadata.set_type_url(
      base::StrCat({"type.googleapis.com/", metadata.GetTypeName()}));
  metadata.SerializeToString(serialised_metadata.mutable_value());
  return serialised_metadata;
}

Passport EntityInstanceToPassport(const EntityInstance& entity) {
  CHECK_EQ(entity.type().name(), EntityTypeName::kPassport);
  // TODO(crbug.com/478783796): Implement conversion.
  return Passport();
}

DriverLicense EntityInstanceToDriverLicense(const EntityInstance& entity) {
  CHECK_EQ(entity.type().name(), EntityTypeName::kDriversLicense);
  // TODO(crbug.com/478783796): Implement conversion.
  return DriverLicense();
}

IdCard EntityInstanceToIdCard(const EntityInstance& entity) {
  CHECK_EQ(entity.type().name(), EntityTypeName::kNationalIdCard);
  // TODO(crbug.com/478783796): Implement conversion.
  return IdCard();
}

KnownTravelerNumber EntityInstanceToKnownTravelerNumber(
    const EntityInstance& entity) {
  CHECK_EQ(entity.type().name(), EntityTypeName::kKnownTravelerNumber);
  // TODO(crbug.com/478783796): Implement conversion.
  return KnownTravelerNumber();
}

RedressNumber EntityInstanceToRedressNumber(const EntityInstance& entity) {
  CHECK_EQ(entity.type().name(), EntityTypeName::kRedressNumber);
  // TODO(crbug.com/478783796): Implement conversion.
  return RedressNumber();
}

}  // namespace

PrivatePass EntityInstanceToPrivatePass(const EntityInstance& entity) {
  PrivatePass pass;
  pass.set_pass_id(entity.guid().value());
  *pass.mutable_client_data()->mutable_chrome_client_data() =
      SerializeMetadata(entity);
  switch (entity.type().name()) {
    case EntityTypeName::kPassport:
      *pass.mutable_passport() = EntityInstanceToPassport(entity);
      break;
    case EntityTypeName::kDriversLicense:
      *pass.mutable_driver_license() = EntityInstanceToDriverLicense(entity);
      break;
    case EntityTypeName::kNationalIdCard:
      *pass.mutable_id_card() = EntityInstanceToIdCard(entity);
      break;
    case EntityTypeName::kKnownTravelerNumber:
      *pass.mutable_known_traveler_number() =
          EntityInstanceToKnownTravelerNumber(entity);
      break;
    case EntityTypeName::kRedressNumber:
      *pass.mutable_redress_number() = EntityInstanceToRedressNumber(entity);
      break;
    // Non-private pass types are not supported.
    case EntityTypeName::kVehicle:
    case EntityTypeName::kFlightReservation:
      NOTREACHED();
  }
  return pass;
}

}  // namespace autofill
