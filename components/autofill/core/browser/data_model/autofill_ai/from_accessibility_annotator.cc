// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"

#include <optional>

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

// The current implementation is not ready for production use.
// TODO(b/480204898): Implement this function.
std::optional<EntityInstance> FromAccessibilityAnnotator(
    const aa::Entity& entity) {
  std::optional<EntityType> entity_type =
      FromAccessibilityAnnotator(entity.GetType());
  if (!entity_type) {
    return std::nullopt;
  }

  std::optional<AttributeInstance> attribute = std::visit(
      absl::Overload{
          [](const aa::Passport& pp) -> std::optional<AttributeInstance> {
            AttributeInstance a = AttributeInstance(
                AttributeType(AttributeTypeName::kPassportNumber));
            a.SetRawInfo(a.type().field_type(), base::UTF8ToUTF16(pp.number),
                         VerificationStatus::kNoStatus);
            return a;
          },
          [](const aa::DriversLicense& dl) -> std::optional<AttributeInstance> {
            AttributeInstance a = AttributeInstance(
                AttributeType(AttributeTypeName::kDriversLicenseNumber));
            a.SetRawInfo(a.type().field_type(), base::UTF8ToUTF16(dl.number),
                         VerificationStatus::kNoStatus);
            return a;
          },
          [](const auto& x) -> std::optional<AttributeInstance> {
            return std::nullopt;
          }},
      entity.specifics);
  if (!attribute) {
    return std::nullopt;
  }

  return EntityInstance(*entity_type, {*std::move(attribute)},
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
