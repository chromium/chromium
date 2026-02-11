// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager_impl.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "components/wallet/core/browser/proto/private_pass.pb.h"

namespace autofill {

namespace {

using ::wallet::PrivatePass;
using WalletRequestError = ::wallet::WalletHttpClient::WalletRequestError;

// Attempts to extract the pass number from the `response` and constructs an
// `AttributeInstance` of the corresponding `AttributeType`.
std::optional<AttributeInstance> PassNumberFromResponse(
    const PrivatePass& response) {
  auto make_attribute = [](AttributeTypeName type, std::string_view value) {
    AttributeInstance attribute((AttributeType(type)));
    // The verification status is irrelevant for unstructured data like pass
    // numbers.
    attribute.SetRawInfo(attribute.type().field_type(),
                         base::UTF8ToUTF16(value),
                         VerificationStatus::kNoStatus);
    attribute.FinalizeInfo();
    return attribute;
  };
  using enum AttributeTypeName;
  switch (response.data_case()) {
    case PrivatePass::kDriverLicense:
      return make_attribute(kDriversLicenseNumber,
                            response.driver_license().driver_license_number());
    case PrivatePass::kPassport:
      return make_attribute(kPassportNumber,
                            response.passport().passport_number());
    case PrivatePass::kIdCard:
      return make_attribute(kNationalIdCardNumber,
                            response.id_card().id_number());
    case PrivatePass::kRedressNumber:
      return make_attribute(kRedressNumberNumber,
                            response.redress_number().redress_number());
    case PrivatePass::kKnownTravelerNumber:
      return make_attribute(
          kKnownTravelerNumberNumber,
          response.known_traveler_number().known_traveler_number());
    case PrivatePass::DATA_NOT_SET:
      // Since the `response` is received from the network, it might be
      // malformed.
      return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace

WalletPassAccessManagerImpl::WalletPassAccessManagerImpl(
    std::unique_ptr<wallet::WalletHttpClient> http_client,
    const EntityDataManager* data_manager)
    : http_client_(std::move(http_client)),
      data_manager_(CHECK_DEREF(data_manager)) {}

WalletPassAccessManagerImpl::~WalletPassAccessManagerImpl() = default;

void WalletPassAccessManagerImpl::SaveWalletEntityInstance(
    const EntityInstance& entity,
    UpsertEntityInstanceCallback callback) {
  NOTIMPLEMENTED();
}

void WalletPassAccessManagerImpl::UpdateWalletEntityInstance(
    const EntityInstance& entity,
    UpsertEntityInstanceCallback callback) {
  NOTIMPLEMENTED();
}

void WalletPassAccessManagerImpl::GetUnmaskedWalletEntityInstance(
    const EntityInstance::EntityId& entity_id,
    GetUnmaskedEntityInstanceCallback callback) {
  base::optional_ref<const EntityInstance> masked_entity =
      data_manager_->GetEntityInstance(entity_id);
  if (!masked_entity) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  CHECK(masked_entity->IsMaskedServerEntity());
  // TODO(crbug.com/478783796): Implement caching.
  http_client_->GetUnmaskedPass(
      entity_id.value(),
      base::BindOnce(
          [](EntityInstance masked_entity,
             const base::expected<PrivatePass, WalletRequestError>& response)
              -> std::optional<EntityInstance> {
            if (!response.has_value()) {
              return std::nullopt;
            }
            std::optional<AttributeInstance> unmasked_pass_number =
                PassNumberFromResponse(response.value());
            // Make sure the response type corresponds to the entity.
            if (!unmasked_pass_number.has_value() ||
                unmasked_pass_number->type().entity_type() !=
                    masked_entity.type() ||
                !masked_entity.attribute(unmasked_pass_number->type())
                     .has_value()) {
              return std::nullopt;
            }
            CHECK(!unmasked_pass_number->masked());
            EntityInstance unmasked_entity =
                masked_entity.CopyWithUpdatedAttribute(
                    std::move(*unmasked_pass_number));
            CHECK(unmasked_entity.IsUnmaskedServerEntity());
            return unmasked_entity;
          },
          *masked_entity)
          .Then(std::move(callback)));
}

}  // namespace autofill
