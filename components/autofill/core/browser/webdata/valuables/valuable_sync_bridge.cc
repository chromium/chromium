// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuable_sync_bridge.h"

#include <algorithm>
#include <optional>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/webdata/common/web_database.h"

namespace autofill {
namespace {

using ValuableDatabaseOperationResult =
    ValuableSyncBridge::ValuableDatabaseOperationResult;

// The address of this variable is used as the user data key.
static const int kAutofillValuableSyncBridgeUserDataKey = 0;

template <class Item>
bool AreAnyItemsDifferent(const std::vector<Item>& old_data,
                          const std::vector<Item>& new_data) {
  if (old_data.size() != new_data.size()) {
    return true;
  }

  return base::MakeFlatSet<Item>(old_data) != base::MakeFlatSet<Item>(new_data);
}

// Tests if the valuable `specifics` are valid and can be converted into an
// Autofill class instance using `CreateAutofillLoyaltyCardFromSpecifics()`.
bool AreAutofillLoyaltyCardSpecificsValid(
    const sync_pb::AutofillValuableSpecifics& specifics) {
  const auto HasEmptyOrValidProgramLogo =
      [](const sync_pb::AutofillValuableSpecifics& specifics) {
        return !specifics.loyalty_card().has_program_logo() ||
               specifics.loyalty_card().program_logo().empty() ||
               GURL(specifics.loyalty_card().program_logo()).is_valid();
      };

  return !specifics.id().empty() && specifics.has_loyalty_card() &&
         !specifics.loyalty_card().loyalty_card_number().empty() &&
         !specifics.loyalty_card().merchant_name().empty() &&
         HasEmptyOrValidProgramLogo(specifics);
}

bool IsSyncWalletFlightReservationsEnabled() {
  return base::FeatureList::IsEnabled(syncer::kSyncWalletFlightReservations);
}

bool IsSyncWalletVehicleRegistrationsEnabled() {
  return base::FeatureList::IsEnabled(syncer::kSyncWalletVehicleRegistrations);
}

bool IsSyncAutofillValuableMetadataEnabled() {
  return base::FeatureList::IsEnabled(syncer::kSyncAutofillValuableMetadata);
}

// Returns if the entity `change` should be uploaded to AUTOFILL_VALUABLE.
bool ShouldUploadEntityChange(const EntityInstanceChange& change) {
  switch (change.data_model().record_type()) {
    case EntityInstance::RecordType::kLocal:
      // Local entities are not uploaded as AUTOFILL_VALUABLE.
      return false;
    case EntityInstance::RecordType::kServerWallet:
      return true;
  }
  NOTREACHED();
}

// Creates an `EntityInstance` from `specifics` and loads its metadata from
// `entity_table` if it exists. Server entities do not come with metadata
// attached. Therefore, we update the entity's metadata with the client's
// existing metadata. This prevents the client from removing entity-related
// metadata when replacing an old entity instance with a new one during
// `EntityTable::AddOrUpdateEntityInstance`.
std::optional<EntityInstance> CreateEntityInstanceFromSpecificsAndLoadMetadata(
    const sync_pb::AutofillValuableSpecifics& specifics,
    const EntityTable& entity_table) {
  if (std::optional<EntityInstance> entity =
          CreateEntityInstanceFromSpecifics(specifics)) {
    if (std::optional<EntityInstance::EntityMetadata> metadata =
            entity_table.GetEntityMetadata(entity->guid())) {
      entity->set_metadata(std::move(*metadata));
    }
    return entity;
  }

  return std::nullopt;
}

}  // namespace

ValuableSyncBridge::ValuableSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    AutofillWebDataBackend* backend)
    : DataTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(backend) {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetValuablesTable()) {
    DataTypeSyncBridge::change_processor()->ReportError(
        {FROM_HERE,
         syncer::ModelError::Type::kAutofillValuableFailedToLoadDatabase});
    return;
  }

  if (IsSyncWalletFlightReservationsEnabled() ||
      IsSyncWalletVehicleRegistrationsEnabled()) {
    scoped_observation_.Observe(web_data_backend_.get());
  }

  LoadMetadata();
}

ValuableSyncBridge::~ValuableSyncBridge() = default;

// static
void ValuableSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData().SetUserData(
      &kAutofillValuableSyncBridgeUserDataKey,
      std::make_unique<ValuableSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::AUTOFILL_VALUABLE,
              /*dump_stack=*/base::DoNothing()),
          web_data_backend));
}

// static
syncer::DataTypeSyncBridge* ValuableSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<ValuableSyncBridge*>(
      web_data_service->GetDBUserData().GetUserData(
          &kAutofillValuableSyncBridgeUserDataKey));
}

bool ValuableSyncBridge::SupportsIncrementalUpdates() const {
  // To allow write requests for sync types that are always fully downloaded,
  // the client must be able to process incremental updates locally, despite the
  // server not supporting them.
  return IsSyncWalletFlightReservationsEnabled() ||
         IsSyncWalletVehicleRegistrationsEnabled();
}

AutofillSyncMetadataTable* ValuableSyncBridge::GetSyncMetadataStore() {
  return AutofillSyncMetadataTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

std::unique_ptr<syncer::MetadataChangeList>
ValuableSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetSyncMetadataStore(), syncer::AUTOFILL_VALUABLE,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

ValuableDatabaseOperationResult ValuableSyncBridge::HandleDeleteRequest(
    const std::string& storage_key) {
  if (std::optional<LoyaltyCard> loyalty_card =
          GetValuablesTable()->GetLoyaltyCardById(ValuableId(storage_key))) {
    if (!GetValuablesTable()->RemoveLoyaltyCard(loyalty_card->id())) {
      return ValuableDatabaseOperationResult::kDatabaseError;
    }
    return ValuableDatabaseOperationResult::kDataChanged;
  }

  if (!IsSyncWalletFlightReservationsEnabled() &&
      !IsSyncWalletVehicleRegistrationsEnabled()) {
    return ValuableDatabaseOperationResult::kNoChange;
  }
  EntityInstance::EntityId entity_id(storage_key);
  if (GetEntityTable()->EntityInstanceExists(entity_id)) {
    // Requesting the associated metadata before the entity removed.
    std::optional<EntityInstance::EntityMetadata> metadata =
        GetEntityTable()->GetEntityMetadata(entity_id);
    if (!GetEntityTable()->RemoveEntityInstance(entity_id)) {
      return ValuableDatabaseOperationResult::kDatabaseError;
    }

    // Server entities can not be removed directly by the user in the client.
    // They are only removed via a ACTION_DELETE directive received through the
    // valuables bridge. When the bridge removes an entity instance and its
    // associated metadata directly from the local table, server metadata
    // observers (e.g. ValuableMetadataSyncBridge) must be manually notified of
    // the deletion so it can be committed to the server.
    if (IsSyncAutofillValuableMetadataEnabled() && metadata) {
      web_data_backend_->NotifyOnServerEntityMetadataChanged(
          EntityInstanceMetadataChange(EntityInstanceMetadataChange::REMOVE,
                                       entity_id, std::move(*metadata)));
    }
    return ValuableDatabaseOperationResult::kDataChanged;
  }

  return ValuableDatabaseOperationResult::kNoChange;
}

std::optional<syncer::ModelError> ValuableSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  return SetSyncData(entity_data);
}

std::optional<syncer::ModelError>
ValuableSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  // Although the `AUTOFILL_VALUABLE` type does not support incremental update
  // on the server, it has been implemented as a workaround to
  // crbug.com/40668179.
  if (!SupportsIncrementalUpdates()) {
    CHECK(entity_changes.empty())
        << "Received an unsupported incremental update.";
    return std::nullopt;
  }

  std::unique_ptr<sql::Transaction> transaction =
      web_data_backend_->GetDatabase()->AcquireTransaction();
  ValuableDatabaseOperationResult db_operation_result =
      ValuableDatabaseOperationResult::kNoChange;

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const syncer::EntityData& entity_data = change->data();

    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        const sync_pb::AutofillValuableSpecifics& specifics =
            entity_data.specifics.autofill_valuable();
        switch (specifics.valuable_data_case()) {
          case sync_pb::AutofillValuableSpecifics::kLoyaltyCard: {
            const LoyaltyCard loyalty_card =
                CreateAutofillLoyaltyCardFromSpecifics(specifics);
            if (!GetValuablesTable()->AddOrUpdateLoyaltyCard(loyalty_card)) {
              db_operation_result =
                  ValuableDatabaseOperationResult::kDatabaseError;
              break;
            }
            break;
          }
          case sync_pb::AutofillValuableSpecifics::kVehicleRegistration:
          case sync_pb::AutofillValuableSpecifics::kFlightReservation:
            if (std::optional<EntityInstance> entity =
                    CreateEntityInstanceFromSpecificsAndLoadMetadata(
                        specifics, *GetEntityTable())) {
              if (!GetEntityTable()->AddOrUpdateEntityInstance(*entity)) {
                db_operation_result =
                    ValuableDatabaseOperationResult::kDatabaseError;
              }
            }
            break;
          case sync_pb::AutofillValuableSpecifics::VALUABLE_DATA_NOT_SET:
            break;
        }
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        if (HandleDeleteRequest(change->storage_key()) ==
            ValuableDatabaseOperationResult::kDatabaseError) {
          db_operation_result = ValuableDatabaseOperationResult::kDatabaseError;
        }

        break;
    }
  }

  if (db_operation_result == ValuableDatabaseOperationResult::kDatabaseError) {
    return syncer::ModelError(
        FROM_HERE,
        syncer::ModelError::Type::kAutofillValuableFailedToWriteToDatabase);
  }

  web_data_backend_->CommitChanges();
  if (transaction && !transaction->Commit()) {
    return syncer::ModelError(
        FROM_HERE,
        syncer::ModelError::Type::kAutofillValuableFailedToWriteToDatabase);
  }

  if (!entity_changes.empty()) {
    web_data_backend_->NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE);
  }

  return std::nullopt;
}

std::unique_ptr<syncer::MutableDataBatch> ValuableSyncBridge::GetData() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const LoyaltyCard& card : GetValuablesTable()->GetLoyaltyCards()) {
    const std::string& id = card.id().value();
    batch->Put(id, CreateEntityDataFromLoyaltyCard(card));
  }

  if (!base::FeatureList::IsEnabled(syncer::kSyncMoveValuablesToProfileDb)) {
    return batch;
  }

  const bool is_sync_flight_reservations_enabled =
      IsSyncWalletFlightReservationsEnabled();

  const bool is_sync_vehicle_registrations_enabled =
      IsSyncWalletVehicleRegistrationsEnabled();

  for (const EntityInstance& instance : GetEntityTable()->GetEntityInstances(
           EntityInstance::RecordType::kServerWallet)) {
    if (instance.type().name() == EntityTypeName::kFlightReservation &&
        is_sync_flight_reservations_enabled) {
      const std::string& id = instance.guid().value();
      batch->Put(id, CreateEntityDataFromEntityInstance(instance));
    }
    if (instance.type().name() == EntityTypeName::kVehicle &&
        is_sync_vehicle_registrations_enabled) {
      const std::string& id = instance.guid().value();
      batch->Put(id, CreateEntityDataFromEntityInstance(instance));
    }
  }

  return batch;
}

std::unique_ptr<syncer::DataBatch> ValuableSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  absl::flat_hash_set<std::string> keys_set(storage_keys.begin(),
                                            storage_keys.end());
  std::unique_ptr<syncer::DataBatch> all_data = GetData();
  while (all_data->HasNext()) {
    syncer::KeyAndData item = all_data->Next();
    if (keys_set.contains(item.first)) {
      batch->Put(item.first, std::move(item.second));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
ValuableSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetData();
}

bool ValuableSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK(entity_data.specifics.has_autofill_valuable());
  const sync_pb::AutofillValuableSpecifics& autofill_valuable =
      entity_data.specifics.autofill_valuable();

  // Valuables must contain a non-empty id.
  if (autofill_valuable.id().empty()) {
    return false;
  }

  switch (autofill_valuable.valuable_data_case()) {
    case sync_pb::AutofillValuableSpecifics::kLoyaltyCard:
      return AreAutofillLoyaltyCardSpecificsValid(autofill_valuable);
    case sync_pb::AutofillValuableSpecifics::kFlightReservation:
      return IsSyncWalletFlightReservationsEnabled();
    case sync_pb::AutofillValuableSpecifics::kVehicleRegistration:
      return IsSyncWalletVehicleRegistrationsEnabled();
    case sync_pb::AutofillValuableSpecifics::VALUABLE_DATA_NOT_SET:
      // Ignore new entry types that the client doesn't know about.
      return false;
  }
}

std::string ValuableSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string ValuableSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK(IsEntityDataValid(entity_data));
  return entity_data.specifics.autofill_valuable().id();
}

void ValuableSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  SetSyncData(syncer::EntityChangeList());
}

sync_pb::EntitySpecifics
ValuableSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  sync_pb::AutofillValuableSpecifics trimmed_autofill_valuable_specifics =
      TrimAutofillValuableSpecificsDataForCaching(
          entity_specifics.autofill_valuable());

  // If all fields are cleared from the valuable specifics, return a fresh
  // EntitySpecifics to avoid caching a few residual bytes.
  if (trimmed_autofill_valuable_specifics.ByteSizeLong() == 0u) {
    return sync_pb::EntitySpecifics();
  }

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  *trimmed_entity_specifics.mutable_autofill_valuable() =
      std::move(trimmed_autofill_valuable_specifics);

  return trimmed_entity_specifics;
}

// TODO(crbug.com/40253286): Consider moving this logic to processor.
bool ValuableSyncBridge::SyncMetadataCacheContainsSupportedFields(
    const syncer::EntityMetadataMap& metadata_map) const {
  for (const auto& metadata_entry : metadata_map) {
    // Serialize the cached specifics and parse them back to a proto. Any fields
    // that were cached as unknown and are known in the current browser version
    // should be parsed correctly.
    std::string serialized_specifics;
    metadata_entry.second->possibly_trimmed_base_specifics().SerializeToString(
        &serialized_specifics);
    sync_pb::EntitySpecifics parsed_specifics;
    parsed_specifics.ParseFromString(serialized_specifics);

    // If `parsed_specifics` contain any supported fields, they would be cleared
    // by the trimming function.
    if (parsed_specifics.ByteSizeLong() !=
        TrimAllSupportedFieldsFromRemoteSpecifics(parsed_specifics)
            .ByteSizeLong()) {
      return true;
    }
  }

  return false;
}

void ValuableSyncBridge::LoadMetadata() {
  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetSyncMetadataStore()->GetAllSyncMetadata(syncer::AUTOFILL_VALUABLE,
                                                  batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE,
         syncer::ModelError::Type::kAutofillValuableFailedToLoadMetadata});
    return;
  } else if (SyncMetadataCacheContainsSupportedFields(
                 batch->GetAllMetadata())) {
    // Caching entity specifics is meant to preserve fields not supported in a
    // given browser version during commits to the server. If the cache
    // contains supported fields, this means that the browser was updated and
    // we should force the initial sync flow to propagate the cached data into
    // the local model.
    GetSyncMetadataStore()->DeleteAllSyncMetadata(
        syncer::DataType::AUTOFILL_VALUABLE);

    batch = std::make_unique<syncer::MetadataBatch>();
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

ValuableDatabaseOperationResult ValuableSyncBridge::SetLoyaltyCards(
    std::vector<LoyaltyCard> loyalty_cards) {
  const bool valuables_data_changed = AreAnyItemsDifferent(
      GetValuablesTable()->GetLoyaltyCards(), loyalty_cards);

  if (!valuables_data_changed) {
    return ValuableDatabaseOperationResult::kNoChange;
  }

  if (!GetValuablesTable()->SetLoyaltyCards(std::move(loyalty_cards))) {
    return ValuableDatabaseOperationResult::kDatabaseError;
  }

  return ValuableDatabaseOperationResult::kDataChanged;
}

ValuableDatabaseOperationResult ValuableSyncBridge::SetEntities(
    std::vector<EntityInstance> entities) {
  if (!base::FeatureList::IsEnabled(syncer::kSyncMoveValuablesToProfileDb)) {
    return ValuableDatabaseOperationResult::kNoChange;
  }

  EntityTable* entity_table = GetEntityTable();
  // No updates are necessary if both the local and the server list of entities
  // are empty.
  if (entities.empty() &&
      entity_table
          ->GetEntityInstances(EntityInstance::RecordType::kServerWallet)
          .empty()) {
    return ValuableDatabaseOperationResult::kNoChange;
  }
  bool success = entity_table->DeleteEntityInstances(
      EntityInstance::RecordType::kServerWallet);

  const bool is_sync_wallet_flight_reservations_enabled =
      IsSyncWalletFlightReservationsEnabled();

  const bool is_sync_wallet_vehicle_registrations_enabled =
      IsSyncWalletVehicleRegistrationsEnabled();

  for (const EntityInstance& entity : entities) {
    if (entity.type().name() == EntityTypeName::kVehicle &&
        is_sync_wallet_vehicle_registrations_enabled) {
      success &= entity_table->AddOrUpdateEntityInstance(entity);
    }

    if (entity.type().name() == EntityTypeName::kFlightReservation &&
        is_sync_wallet_flight_reservations_enabled) {
      success &= entity_table->AddOrUpdateEntityInstance(entity);
    }
  }

  return success ? ValuableDatabaseOperationResult::kDataChanged
                 : ValuableDatabaseOperationResult::kDatabaseError;
}

std::optional<syncer::ModelError> ValuableSyncBridge::SetSyncData(
    const syncer::EntityChangeList& entity_data) {
  std::unique_ptr<sql::Transaction> transaction =
      web_data_backend_->GetDatabase()->AcquireTransaction();

  std::vector<LoyaltyCard> loyalty_cards;
  std::vector<EntityInstance> entities;
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD: {
        DCHECK(change->data().specifics.has_autofill_valuable());
        // Deserialize the AutofillValuableSpecifics and add them in the DB.
        const sync_pb::AutofillValuableSpecifics& autofill_valuable =
            change->data().specifics.autofill_valuable();
        switch (autofill_valuable.valuable_data_case()) {
          case sync_pb::AutofillValuableSpecifics::kLoyaltyCard: {
            loyalty_cards.push_back(
                CreateAutofillLoyaltyCardFromSpecifics(autofill_valuable));
            break;
          }
          case sync_pb::AutofillValuableSpecifics::kFlightReservation:
          case sync_pb::AutofillValuableSpecifics::kVehicleRegistration:
            if (std::optional<EntityInstance> entity =
                    CreateEntityInstanceFromSpecificsAndLoadMetadata(
                        autofill_valuable, *GetEntityTable())) {
              entities.push_back(std::move(*entity));
            }
            break;
          case sync_pb::AutofillValuableSpecifics::VALUABLE_DATA_NOT_SET:
            // Ignore new entry types that the client doesn't know about.
            break;
        }

        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
      case syncer::EntityChange::ACTION_UPDATE: {
        // Valuables sync does not support incremental updates server side.
        return syncer::ModelError(
            FROM_HERE,
            syncer::ModelError::Type::kAutofillValuableUnsupportedActionType);
      }
    }
  }

  const ValuableDatabaseOperationResult set_loyalty_cards_result =
      SetLoyaltyCards(std::move(loyalty_cards));
  const ValuableDatabaseOperationResult set_entities_result =
      SetEntities(std::move(entities));

  const bool set_valuables_error =
      set_loyalty_cards_result ==
          ValuableDatabaseOperationResult::kDatabaseError ||
      set_entities_result == ValuableDatabaseOperationResult::kDatabaseError;

  if (set_valuables_error) {
    return syncer::ModelError(
        FROM_HERE,
        syncer::ModelError::Type::kAutofillValuableFailedToWriteToDatabase);
  }

  // Commits changes through CommitChanges(...) or through the scoped
  // sql::Transaction `transaction` depending on the
  // 'SqlScopedTransactionWebDatabase' Finch experiment.
  web_data_backend_->CommitChanges();
  if (transaction) {
    transaction->Commit();
  }

  const bool valuables_data_changed =
      set_loyalty_cards_result ==
          ValuableDatabaseOperationResult::kDataChanged ||
      set_entities_result == ValuableDatabaseOperationResult::kDataChanged;

  if (valuables_data_changed) {
    web_data_backend_->NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE);
  }
  return std::nullopt;
}

void ValuableSyncBridge::EntityInstanceChanged(
    const EntityInstanceChange& change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsSyncWalletFlightReservationsEnabled() &&
      !IsSyncWalletVehicleRegistrationsEnabled()) {
    return;
  }

  if (!ShouldUploadEntityChange(change)) {
    return;
  }

  CHECK(change_processor()->IsTrackingMetadata());

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  switch (change.type()) {
    case EntityInstanceChange::ADD:
    case EntityInstanceChange::UPDATE:
      change_processor()->Put(
          *change.key(),
          CreateEntityDataFromEntityInstance(change.data_model()),
          metadata_change_list.get());
      break;
    case EntityInstanceChange::REMOVE:
    case EntityInstanceChange::HIDE_IN_AUTOFILL:
      // Removing valuables is not supported from the client.
      NOTREACHED();
  }
}

ValuablesTable* ValuableSyncBridge::GetValuablesTable() {
  return ValuablesTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

EntityTable* ValuableSyncBridge::GetEntityTable() {
  return EntityTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

}  // namespace autofill
