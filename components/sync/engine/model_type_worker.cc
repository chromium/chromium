// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/model_type_worker.h"

#include <stdint.h>

#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/uuid.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/features.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_invalidation_adapter.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/bookmark_update_preprocessing.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/commit_contribution.h"
#include "components/sync/engine/commit_contribution_impl.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/model_type_state_helper.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/sync_entity.pb.h"

namespace syncer {

namespace {

const char kTimeUntilEncryptionKeyFoundHistogramName[] =
    "Sync.ModelTypeTimeUntilEncryptionKeyFound2";
const char kUndecryptablePendingUpdatesDroppedHistogramName[] =
    "Sync.ModelTypeUndecryptablePendingUpdatesDropped";
const char kBlockedByUndecryptableUpdateHistogramName[] =
    "Sync.ModelTypeBlockedDueToUndecryptableUpdate";
const char kPasswordNotesStateHistogramName[] =
    "Sync.PasswordNotesStateInUpdate";

BASE_FEATURE(kSyncKeepGcDirectiveDuringSyncCycle,
             "SyncKeepGcDirectiveDuringSyncCycle",
             base::FEATURE_ENABLED_BY_DEFAULT);

void LogPasswordNotesState(PasswordNotesStateForUMA state) {
  base::UmaHistogramEnumeration(kPasswordNotesStateHistogramName, state);
}

// A proxy which can be called from any sequence and delegates the work to the
// commit queue injected on construction.
class CommitQueueProxy : public CommitQueue {
 public:
  // Must be called from the sequence where |commit_queue| lives.
  explicit CommitQueueProxy(const base::WeakPtr<CommitQueue>& commit_queue)
      : commit_queue_(commit_queue) {}
  ~CommitQueueProxy() override = default;

  void NudgeForCommit() override {
    commit_queue_thread_->PostTask(
        FROM_HERE, base::BindOnce(&CommitQueue::NudgeForCommit, commit_queue_));
  }

 private:
  const base::WeakPtr<CommitQueue> commit_queue_;
  const scoped_refptr<base::SequencedTaskRunner> commit_queue_thread_ =
      base::SequencedTaskRunner::GetCurrentDefault();
};

void AdaptClientTagForFullUpdateData(ModelType model_type,
                                     syncer::EntityData* data) {
  // Server does not send any client tags for wallet data entities or offer data
  // entities. This code manually asks the bridge to create the client tags for
  // each entity, so that we can use ClientTagBasedModelTypeProcessor for
  // AUTOFILL_WALLET_DATA or AUTOFILL_WALLET_OFFER.
  if (data->legacy_parent_id == "0") {
    // Ignore the permanent root node as that one should have no client tag
    // hash.
    return;
  }
  DCHECK(!data->specifics.has_encrypted());
  if (model_type == AUTOFILL_WALLET_DATA) {
    DCHECK(data->specifics.has_autofill_wallet());
    data->client_tag_hash = ClientTagHash::FromUnhashed(
        AUTOFILL_WALLET_DATA, GetUnhashedClientTagFromAutofillWalletSpecifics(
                                  data->specifics.autofill_wallet()));
  } else if (model_type == AUTOFILL_WALLET_OFFER) {
    DCHECK(data->specifics.has_autofill_offer());
    data->client_tag_hash = ClientTagHash::FromUnhashed(
        AUTOFILL_WALLET_OFFER, GetUnhashedClientTagFromAutofillOfferSpecifics(
                                   data->specifics.autofill_offer()));
  } else {
    NOTREACHED();
  }
}

void AdaptWebAuthnClientTagHash(syncer::EntityData* data) {
  // Google Play Services may create entities where the client_tag_hash doesn't
  // conform to the form expected by Chromium. These values are the hex-encoded,
  // 16-byte random `sync_id` value, and will therefore always be 32 bytes long.
  // Valid ClientTagHash values are Base64(SHA1(protobuf_prefix + client_tag))
  // and therefore always 28 bytes.
  const std::string& client_tag_hash = data->client_tag_hash.value();
  if (client_tag_hash.size() == 32 &&
      // base::HexEncode() returns upper case, `client_tag_hash` is lower case.
      base::ToUpperASCII(client_tag_hash) ==
          base::HexEncode(base::as_bytes(base::make_span(
              data->specifics.webauthn_credential().sync_id())))) {
    data->client_tag_hash = ClientTagHash::FromUnhashed(
        ModelType::WEBAUTHN_CREDENTIAL,
        data->specifics.webauthn_credential().sync_id());
  }
}

// Returns empty string if |entity| is not encrypted.
// TODO(crbug.com/1109221): Consider moving this to a util file and converting
// UpdateResponseData::encryption_key_name into a method that calls it. Consider
// returning a struct containing also the encrypted blob, which would make the
// code of PopulateUpdateResponseData() simpler.
std::string GetEncryptionKeyName(const sync_pb::SyncEntity& entity) {
  if (entity.deleted()) {
    return std::string();
  }
  // Passwords use their own legacy encryption scheme.
  if (entity.specifics().password().has_encrypted()) {
    return entity.specifics().password().encrypted().key_name();
  }
  if (entity.specifics().has_encrypted()) {
    return entity.specifics().encrypted().key_name();
  }
  return std::string();
}

// Attempts to decrypt the given specifics and return them in the |out|
// parameter. The cryptographer must know the decryption key, i.e.
// cryptographer.CanDecrypt(specifics.encrypted()) must return true.
//
// Returns false if the decryption failed. There are no guarantees about the
// contents of |out| when that happens.
//
// In theory, this should never fail. Only corrupt or invalid entries could
// cause this to fail, and no clients are known to create such entries. The
// failure case is an attempt to be defensive against bad input.
bool DecryptSpecifics(const Cryptographer& cryptographer,
                      const sync_pb::EntitySpecifics& in,
                      sync_pb::EntitySpecifics* out) {
  DCHECK(!in.has_password());
  DCHECK(in.has_encrypted());
  DCHECK(cryptographer.CanDecrypt(in.encrypted()));

  if (!cryptographer.Decrypt(in.encrypted(), out)) {
    DLOG(ERROR) << "Failed to decrypt a decryptable specifics";
    return false;
  }
  return true;
}

// Attempts to decrypt the given password specifics and return them in the
// |out| parameter. The cryptographer must know the decryption key, i.e.
// cryptographer.CanDecrypt(in.password().encrypted()) must return true.
//
// Returns false if the decryption failed. There are no guarantees about the
// contents of |out| when that happens.
//
// In theory, this should never fail. Only corrupt or invalid entries could
// cause this to fail, and no clients are known to create such entries. The
// failure case is an attempt to be defensive against bad input.
bool DecryptPasswordSpecifics(const Cryptographer& cryptographer,
                              const sync_pb::EntitySpecifics& in,
                              sync_pb::EntitySpecifics* out) {
  DCHECK(in.has_password());
  DCHECK(in.password().has_encrypted());
  DCHECK(cryptographer.CanDecrypt(in.password().encrypted()));

  if (!cryptographer.Decrypt(
          in.password().encrypted(),
          out->mutable_password()->mutable_client_only_encrypted_data())) {
    DLOG(ERROR) << "Failed to decrypt a decryptable password";
    return false;
  }
  // The `notes` field in the PasswordSpecificsData is the authoritative value.
  // When set, it disregards whatever `encrypted_notes_backup` contains.
  if (out->password().client_only_encrypted_data().has_notes()) {
    LogPasswordNotesState(PasswordNotesStateForUMA::kSetInSpecificsData);
    return true;
  }
  if (!in.password().has_encrypted_notes_backup()) {
    LogPasswordNotesState(PasswordNotesStateForUMA::kUnset);
    return true;
  }
  if (!base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup)) {
    return true;
  }
  // It is guaranteed that if `encrypted()` is decryptable, then
  // `encrypted_notes_backup()` must be decryptable too. Failure to decrypt
  // `encrypted_notes_backup()` indicates a data corruption.
  if (!cryptographer.Decrypt(in.password().encrypted_notes_backup(),
                             out->mutable_password()
                                 ->mutable_client_only_encrypted_data()
                                 ->mutable_notes())) {
    LogPasswordNotesState(
        PasswordNotesStateForUMA::kSetOnlyInBackupButCorrupted);
    return false;
  }
  LogPasswordNotesState(PasswordNotesStateForUMA::kSetOnlyInBackup);
  // TODO(crbug.com/1326554): Properly handle the case when both blobs are
  // decryptable but with different keys. Ideally the password should be
  // re-uploaded potentially by setting needs_reupload boolean in
  // UpdateResponseData or EntityData.
  return true;
}

}  // namespace

ModelTypeWorker::ModelTypeWorker(ModelType type,
                                 const sync_pb::ModelTypeState& initial_state,
                                 Cryptographer* cryptographer,
                                 bool encryption_enabled,
                                 PassphraseType passphrase_type,
                                 NudgeHandler* nudge_handler,
                                 CancelationSignal* cancelation_signal)
    : type_(type),
      cryptographer_(cryptographer),
      nudge_handler_(nudge_handler),
      cancelation_signal_(cancelation_signal),
      model_type_state_(initial_state),
      encryption_enabled_(encryption_enabled),
      passphrase_type_(passphrase_type),
      min_get_updates_to_ignore_key_(kMinGuResponsesToIgnoreKey.Get()) {
  DCHECK(cryptographer_);
  DCHECK(!AlwaysEncryptedUserTypes().Has(type_) || encryption_enabled_);

  // GC directive is stored independently of progress marker and is used during
  // a sync cycle (i.e. in-memory only). Clear GC directive on load to clean up
  // previously persisted values.
  model_type_state_.mutable_progress_marker()->clear_gc_directive();

  if (!model_type_state_.invalidations().empty()) {
    if (base::FeatureList::IsEnabled(kSyncPersistInvalidations)) {
      if (static_cast<size_t>(model_type_state_.invalidations_size()) >
          kMaxPendingInvalidations) {
        DVLOG(1) << "Cleaning invalidations in |model_type_state_| due to "
                    "invalidations overflow.";
        model_type_state_.clear_invalidations();
      }
      // TODO(crbug/1365292): Persisted invaldiations are loaded in
      // ModelTypeWorker::ctor(), but sync cycle is not scheduled. New sync
      // cycle has to be triggered right after we loaded persisted
      // invalidations.
      for (int i = 0; i < model_type_state_.invalidations_size(); ++i) {
        pending_invalidations_.emplace_back(
            std::make_unique<SyncInvalidationAdapter>(
                model_type_state_.invalidations(i).hint(),
                model_type_state_.invalidations(i).has_version()
                    ? absl::optional<int64_t>(
                          model_type_state_.invalidations(i).version())
                    : absl::nullopt),
            false);
      }

      bool is_version_order_correct = true;
      for (size_t i = 1; i < pending_invalidations_.size(); ++i) {
        is_version_order_correct &= (SyncInvalidation::LessThanByVersion(
            *pending_invalidations_[i - 1].pending_invalidation,
            *pending_invalidations_[i].pending_invalidation));
      }
      if (!is_version_order_correct) {
        DVLOG(1) << "Cleaning invalidations in |model_type_state| due to "
                    "incorrect version order.";
        pending_invalidations_.clear();
        model_type_state_.clear_invalidations();
      }
    } else {
      // In case the feature was enabled in previous session, some invalidations
      // might be loaded to |model_type_state_| from storage. As feature is
      // disabled now, invalidations in |model_type_state_| and
      // |pending_invalidations_| should be in sync.
      model_type_state_.clear_invalidations();
    }
  }

  if (!CommitOnlyTypes().Has(GetModelType())) {
    DCHECK_EQ(type, GetModelTypeFromSpecificsFieldNumber(
                        initial_state.progress_marker().data_type_id()));
  }
}

ModelTypeWorker::PendingInvalidation::PendingInvalidation() = default;
ModelTypeWorker::PendingInvalidation::PendingInvalidation(
    PendingInvalidation&&) = default;
ModelTypeWorker::PendingInvalidation&
ModelTypeWorker::PendingInvalidation::operator=(PendingInvalidation&&) =
    default;
ModelTypeWorker::PendingInvalidation::PendingInvalidation(
    std::unique_ptr<SyncInvalidation> invalidation,
    bool is_processed)
    : pending_invalidation(std::move(invalidation)),
      is_processed(is_processed) {}
ModelTypeWorker::PendingInvalidation::~PendingInvalidation() = default;

ModelTypeWorker::~ModelTypeWorker() {
  if (model_type_processor_) {
    // This will always be the case in production today.
    model_type_processor_->DisconnectSync();
  }
  for (size_t i = 0; i < pending_invalidations_.size(); ++i) {
    LogPendingInvalidationStatus(PendingInvalidationStatus::kLost);
  }
}

void ModelTypeWorker::LogPendingInvalidationStatus(
    PendingInvalidationStatus status) {
  base::UmaHistogramEnumeration("Sync.PendingInvalidationStatus", status);
}

void ModelTypeWorker::ConnectSync(
    std::unique_ptr<ModelTypeProcessor> model_type_processor) {
  DCHECK(!model_type_processor_);
  DCHECK(model_type_processor);

  model_type_processor_ = std::move(model_type_processor);
  // TODO(victorvianna): CommitQueueProxy is only needed by the
  // ModelTypeProcessorProxy implementation, so it could possibly be moved
  // there % changing ConnectSync() to take a raw pointer. This then allows
  // removing base::test::SingleThreadTaskEnvironment from the unit test.
  model_type_processor_->ConnectSync(
      std::make_unique<CommitQueueProxy>(weak_ptr_factory_.GetWeakPtr()));

  if (!IsInitialSyncDone(model_type_state_.initial_sync_state())) {
    nudge_handler_->NudgeForInitialDownload(type_);
  }

  // |model_type_state_| might have an outdated encryption key name, e.g.
  // because |cryptographer_| was updated before this worker was constructed.
  // OnCryptographerChange() might never be called, so update the key manually
  // here and push it to the processor. SendPendingUpdatesToProcessorIfReady()
  // takes care to only send updated if initial sync is (at least partially)
  // done, otherwise this violates some of the processor assumptions; if initial
  // sync isn't done, the now-updated key will be pushed on the first
  // ApplyUpdates() call anyway.
  bool had_outdated_key_name = UpdateTypeEncryptionKeyName();
  if (had_outdated_key_name) {
    SendPendingUpdatesToProcessorIfReady();
  }
}

ModelType ModelTypeWorker::GetModelType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return type_;
}

void ModelTypeWorker::EnableEncryption() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (encryption_enabled_) {
    // No-op.
    return;
  }

  encryption_enabled_ = true;
  // UpdateTypeEncryptionKeyName() might return false if the cryptographer does
  // not have a default key yet.
  if (UpdateTypeEncryptionKeyName()) {
    // Push the new key name to the processor.
    SendPendingUpdatesToProcessorIfReady();
  }
}

void ModelTypeWorker::OnCryptographerChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Always try to decrypt, regardless of |encryption_enabled_|. This might
  // add some elements to |pending_updates_|.
  DecryptStoredEntities();
  bool had_oudated_key_name = UpdateTypeEncryptionKeyName();
  if (had_oudated_key_name || !pending_updates_.empty()) {
    // Push the newly decrypted updates and/or the new key name to the
    // processor.
    SendPendingUpdatesToProcessorIfReady();
  }
  // If the worker couldn't commit before due to BlockForEncryption(), this
  // might now be resolved. The call is a no-op if there's nothing to commit.
  NudgeIfReadyToCommit();
}

void ModelTypeWorker::UpdatePassphraseType(PassphraseType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  passphrase_type_ = type;
}

bool ModelTypeWorker::IsInitialSyncEnded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsInitialSyncDone(model_type_state_.initial_sync_state());
}

const sync_pb::DataTypeProgressMarker& ModelTypeWorker::GetDownloadProgress()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_type_state_.progress_marker();
}

const sync_pb::DataTypeContext& ModelTypeWorker::GetDataTypeContext() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_type_state_.type_context();
}

void ModelTypeWorker::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const sync_pb::DataTypeContext& mutated_context,
    const SyncEntityList& applicable_updates,
    StatusController* status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool is_initial_sync =
      !IsInitialSyncDone(model_type_state_.initial_sync_state());

  // TODO(rlarocque): Handle data type context conflicts.
  *model_type_state_.mutable_type_context() = mutated_context;

  if (progress_marker.has_gc_directive() &&
      base::FeatureList::IsEnabled(kSyncKeepGcDirectiveDuringSyncCycle)) {
    // Clean up all the pending updates because a new GC directive has been
    // received which means that all existing data should be cleaned up.
    pending_updates_.clear();
    entries_pending_decryption_.clear();
  }

  *model_type_state_.mutable_progress_marker() = progress_marker;
  ExtractGcDirective();

  for (const sync_pb::SyncEntity* update_entity : applicable_updates) {
    RecordEntityChangeMetrics(
        type_, is_initial_sync
                   ? ModelTypeEntityChange::kRemoteInitialUpdate
                   : ModelTypeEntityChange::kRemoteNonInitialUpdate);

    if (update_entity->deleted()) {
      status->increment_num_tombstone_updates_downloaded_by(1);
      if (!is_initial_sync) {
        RecordEntityChangeMetrics(type_,
                                  ModelTypeEntityChange::kRemoteDeletion);
      }
    }

    UpdateResponseData response_data;
    switch (PopulateUpdateResponseData(*cryptographer_, type_, *update_entity,
                                       &response_data)) {
      case SUCCESS:
        pending_updates_.push_back(std::move(response_data));
        // Override any previously undecryptable update for the same id.
        entries_pending_decryption_.erase(update_entity->id_string());
        break;
      case DECRYPTION_PENDING: {
        SyncRecordModelTypeUpdateDropReason(
            UpdateDropReason::kDecryptionPending, type_);

        const std::string& key_name = response_data.encryption_key_name;
        DCHECK(!key_name.empty());
        // If there's no entry for this unknown encryption key, create one.
        unknown_encryption_keys_by_name_.emplace(key_name,
                                                 UnknownEncryptionKeyInfo());

        const std::string& server_id = update_entity->id_string();
        if (ShouldIgnoreUpdatesEncryptedWith(key_name)) {
          // Don't queue the incoming update. If there's a queued entry for
          // |server_id|, don't clear it: outdated data is better than nothing.
          // Such entry should be encrypted with another key, since |key_name|'s
          // queued updates would've have been dropped by now.
          DCHECK(!base::Contains(entries_pending_decryption_, server_id) ||
                 GetEncryptionKeyName(entries_pending_decryption_[server_id]) !=
                     key_name);
          SyncRecordModelTypeUpdateDropReason(
              UpdateDropReason::kDecryptionPendingForTooLong, type_);
          break;
        }
        // Copy the sync entity for later decryption.
        // TODO(crbug.com/1270734): Any write to |entries_pending_decryption_|
        // should do like DeduplicatePendingUpdatesBasedOnServerId() and honor
        // entity version. Additionally, it should look up the same server id
        // in |pending_updates_| and compare versions. In fact, the 2 containers
        // should probably be moved to a separate class with unit tests.
        entries_pending_decryption_[server_id] = *update_entity;
        break;
      }
      case FAILED_TO_DECRYPT:
        // Failed to decrypt the entity. Likely it is corrupt. Move on.
        SyncRecordModelTypeUpdateDropReason(UpdateDropReason::kFailedToDecrypt,
                                            type_);
        break;
    }
  }

  // Some updates pending decryption might have been overwritten by decryptable
  // ones. So some encryption keys may no longer fit the definition of unknown.
  RemoveKeysNoLongerUnknown();

  if (!entries_pending_decryption_.empty() &&
      (!encryption_enabled_ || cryptographer_->CanEncrypt())) {
    base::UmaHistogramEnumeration(kBlockedByUndecryptableUpdateHistogramName,
                                  ModelTypeHistogramValue(type_));
  }

  // Usually, updates must only be applied at the end of a sync cycle, once all
  // updates have been downloaded. This is mostly important during initial sync,
  // so that the merge of local and remote data can happen.
  // Data types that do not do an actual merge also don't have to download all
  // remote data first. Instead, apply updates as they come in. This saves the
  // need to accumulate all data in memory.
  if (ApplyUpdatesImmediatelyTypes().Has(type_)) {
    ApplyUpdates(status, /*cycle_done=*/false);
  }
}

// static
// |response_data| must be not null.
ModelTypeWorker::DecryptionStatus ModelTypeWorker::PopulateUpdateResponseData(
    const Cryptographer& cryptographer,
    ModelType model_type,
    const sync_pb::SyncEntity& update_entity,
    UpdateResponseData* response_data) {
  syncer::EntityData data;

  // Deleted entities must use the default instance of EntitySpecifics in
  // order for EntityData to correctly reflect that they are deleted.
  const sync_pb::EntitySpecifics& specifics =
      update_entity.deleted() ? sync_pb::EntitySpecifics::default_instance()
                              : update_entity.specifics();
  bool specifics_were_encrypted = false;

  response_data->encryption_key_name = GetEncryptionKeyName(update_entity);
  // Try to decrypt any encrypted data. Per crbug.com/1178418, in rare cases
  // ModelTypeWorker receives some even though its type doesn't use encryption.
  // If so, still try to decrypt with the available keys regardless.
  if (specifics.password().has_encrypted()) {
    // Passwords use their own legacy encryption scheme.
    if (!cryptographer.CanDecrypt(specifics.password().encrypted())) {
      return DECRYPTION_PENDING;
    }
    if (!DecryptPasswordSpecifics(cryptographer, specifics, &data.specifics)) {
      return FAILED_TO_DECRYPT;
    }
    specifics_were_encrypted = true;
  } else if (specifics.has_encrypted()) {
    DCHECK(!update_entity.deleted()) << "Tombstones shouldn't be encrypted";
    if (!cryptographer.CanDecrypt(specifics.encrypted())) {
      return DECRYPTION_PENDING;
    }
    if (!DecryptSpecifics(cryptographer, specifics, &data.specifics)) {
      return FAILED_TO_DECRYPT;
    }
    specifics_were_encrypted = true;
  } else {
    // No encryption.
    data.specifics = specifics;
  }

  response_data->response_version = update_entity.version();
  // Prepare the message for the model thread.
  data.id = update_entity.id_string();
  data.client_tag_hash =
      ClientTagHash::FromHashed(update_entity.client_tag_hash());
  data.creation_time = ProtoTimeToTime(update_entity.ctime());
  data.modification_time = ProtoTimeToTime(update_entity.mtime());
  data.name = update_entity.name();
  data.legacy_parent_id = update_entity.parent_id_string();
  data.server_defined_unique_tag = update_entity.server_defined_unique_tag();

  // Populate |originator_cache_guid| and |originator_client_item_id|. This is
  // currently relevant only for bookmarks.
  data.originator_cache_guid = update_entity.originator_cache_guid();
  data.originator_client_item_id = update_entity.originator_client_item_id();

  // Adapt the update for compatibility.
  if (model_type == BOOKMARKS) {
    data.is_bookmark_unique_position_in_specifics_preprocessed =
        AdaptUniquePositionForBookmark(update_entity, &data.specifics);
    AdaptTypeForBookmark(update_entity, &data.specifics);
    AdaptTitleForBookmark(update_entity, &data.specifics,
                          specifics_were_encrypted);
    AdaptGuidForBookmark(update_entity, &data.specifics);
    // Note that the parent GUID in specifics cannot be adapted/populated here,
    // because the logic requires access to tracked entities. Hence, it is
    // done by BookmarkModelTypeProcessor, with logic implemented in
    // components/sync_bookmarks/parent_guid_preprocessing.cc.
  } else if (model_type == AUTOFILL_WALLET_DATA ||
             model_type == AUTOFILL_WALLET_OFFER) {
    AdaptClientTagForFullUpdateData(model_type, &data);
  } else if (model_type == WEBAUTHN_CREDENTIAL) {
    AdaptWebAuthnClientTagHash(&data);
  }

  response_data->entity = std::move(data);
  return SUCCESS;
}

void ModelTypeWorker::ApplyUpdates(StatusController* status, bool cycle_done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Indicate the new initial-sync state to the processor: If the current sync
  // cycle was completed, the initial sync must be done. Otherwise, it's started
  // now. The latter can only happen for ApplyUpdatesImmediatelyTypes(), since
  // other types wait for the cycle to complete before applying any updates.
  // Note that the initial sync technically isn't started/done yet but by the
  // time this value is persisted to disk on the model thread it will be.
  if (cycle_done) {
    model_type_state_.set_initial_sync_state(
        sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  } else {
    DCHECK(ApplyUpdatesImmediatelyTypes().Has(type_));
    model_type_state_.set_initial_sync_state(
        sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_PARTIALLY_DONE);
  }

  if (!entries_pending_decryption_.empty() &&
      (!encryption_enabled_ || cryptographer_->CanEncrypt())) {
    DCHECK(BlockForEncryption());
    for (auto& [key, info] : unknown_encryption_keys_by_name_) {
      info.get_updates_while_should_have_been_known++;
      // If the key is now missing for too long, drop pending updates encrypted
      // with it. This eventually unblocks a worker having undecryptable data.
      MaybeDropPendingUpdatesEncryptedWith(key);
    }
  }

  // At the end of a sync cycle, clean up any invalidations that were used.
  // (If the cycle is still ongoing, i.e. there are more updates to download,
  // the invalidations must be kept and sent again in the next request, since
  // they may still be relevant.)
  if (cycle_done) {
    // Processed pending invalidations are deleted, and unprocessed
    // invalidations will be used again in the next sync cycle.
    auto it = pending_invalidations_.begin();
    while (it != pending_invalidations_.end()) {
      if (it->is_processed) {
        LogPendingInvalidationStatus(PendingInvalidationStatus::kAcknowledged);
        it->pending_invalidation->Acknowledge();
        it = pending_invalidations_.erase(it);
      } else {
        ++it;
      }
    }
    if (base::FeatureList::IsEnabled(kSyncPersistInvalidations)) {
      UpdateModelTypeStateInvalidations();
    }

    has_dropped_invalidation_ = false;

    nudge_handler_->SetHasPendingInvalidations(type_,
                                               HasPendingInvalidations());
  }

  if (HasNonDeletionUpdates()) {
    status->add_updated_type(type_);
  }

  // Download cycle is done, pass all updates to the processor.
  SendPendingUpdatesToProcessorIfReady();
}

void ModelTypeWorker::SendPendingUpdatesToProcessorIfReady() {
  DCHECK(model_type_processor_);

  if (!IsInitialSyncAtLeastPartiallyDone(
          model_type_state_.initial_sync_state())) {
    return;
  }

  if (BlockForEncryption()) {
    return;
  }

  DCHECK(!AlwaysEncryptedUserTypes().Has(type_) || encryption_enabled_);
  DCHECK(!encryption_enabled_ ||
         !model_type_state_.encryption_key_name().empty());
  DCHECK(entries_pending_decryption_.empty());

  DVLOG(1) << ModelTypeToDebugString(type_) << ": "
           << base::StringPrintf("Delivering %" PRIuS " applicable updates.",
                                 pending_updates_.size());

  // Deduplicate updates first based on server ids, which is the only legit
  // source of duplicates, specially due to pagination.
  DeduplicatePendingUpdatesBasedOnServerId();

  // As extra precaution, and although it shouldn't be necessary without a
  // misbehaving server, deduplicate based on client tags and originator item
  // IDs. This allows further code to use DCHECKs without relying on external
  // behavior.
  DeduplicatePendingUpdatesBasedOnClientTagHash();
  DeduplicatePendingUpdatesBasedOnOriginatorClientItemId();

  model_type_processor_->OnUpdateReceived(model_type_state_,
                                          std::move(pending_updates_),
                                          std::move(pending_gc_directive_));
  pending_updates_.clear();
  pending_gc_directive_.reset();
}

void ModelTypeWorker::NudgeForCommit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_local_changes_state_ = kNewlyNudgedLocalChanges;
  NudgeIfReadyToCommit();
}

void ModelTypeWorker::NudgeIfReadyToCommit() {
  // TODO(crbug.com/1188034): |kNoNudgedLocalChanges| is used to keep the
  // existing behaviour. But perhaps there is no need to nudge for commit if all
  // known changes are already in flight.
  if (has_local_changes_state_ != kNoNudgedLocalChanges && CanCommitItems()) {
    nudge_handler_->NudgeForCommit(type_);
  }
}

std::unique_ptr<CommitContribution> ModelTypeWorker::GetContribution(
    size_t max_entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialSyncAtLeastPartiallyDone(
      model_type_state_.initial_sync_state()));
  DCHECK(model_type_processor_);

  // Early return if type is not ready to commit (initial sync isn't done or
  // cryptographer has pending keys).
  if (!CanCommitItems()) {
    return nullptr;
  }

  // Client shouldn't be committing data to server when it hasn't processed all
  // updates it received.
  DCHECK(entries_pending_decryption_.empty());

  // Pull local changes from the processor (in the model thread/sequence). Note
  // that this takes place independently of nudges (i.e.
  // |has_local_changes_state_|), in case the processor decided a local change
  // was not worth a nudge.
  scoped_refptr<GetLocalChangesRequest> request =
      base::MakeRefCounted<GetLocalChangesRequest>(cancelation_signal_);
  model_type_processor_->GetLocalChanges(
      max_entries,
      base::BindOnce(&GetLocalChangesRequest::SetResponse, request));
  request->WaitForResponseOrCancelation();
  CommitRequestDataList response;
  if (!request->WasCancelled()) {
    response = request->ExtractResponse();
  }
  if (response.empty()) {
    has_local_changes_state_ = kNoNudgedLocalChanges;
    return nullptr;
  }

  DCHECK(response.size() <= max_entries);
  if (response.size() < max_entries) {
    // In case when response.size() equals to |max_entries|, there will be
    // another commit request (see CommitProcessor::GatherCommitContributions).
    // Hence, in general it should be normal if |has_local_changes_state_| is
    // |kNewlyNudgedLocalChanges| (even if there are no more items in the
    // processor). In other words, |kAllNudgedLocalChangesInFlight| means that
    // there might not be another commit request in the current sync cycle (but
    // still possible if some other data type contributes |max_entities|).
    has_local_changes_state_ = kAllNudgedLocalChangesInFlight;
  }

  DCHECK(!AlwaysEncryptedUserTypes().Has(type_) || encryption_enabled_);
  DCHECK(!encryption_enabled_ ||
         (model_type_state_.encryption_key_name() ==
          cryptographer_->GetDefaultEncryptionKeyName()));
  return std::make_unique<CommitContributionImpl>(
      type_, model_type_state_.type_context(), std::move(response),
      base::BindOnce(&ModelTypeWorker::OnCommitResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ModelTypeWorker::OnFullCommitFailure,
                     weak_ptr_factory_.GetWeakPtr()),
      encryption_enabled_ ? cryptographer_.get() : nullptr, passphrase_type_,
      CommitOnlyTypes().Has(type_));
}

bool ModelTypeWorker::HasLocalChangesForTest() const {
  return has_local_changes_state_ != kNoNudgedLocalChanges;
}

void ModelTypeWorker::OnCommitResponse(
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Send the responses back to the model thread. It needs to know which
  // items have been successfully committed (it can save that information in
  // permanent storage) and which failed (it can e.g. notify the user).
  model_type_processor_->OnCommitCompleted(
      model_type_state_, committed_response_list, error_response_list);

  if (has_local_changes_state_ == kAllNudgedLocalChangesInFlight) {
    // There are no new nudged changes since last commit.
    has_local_changes_state_ = kNoNudgedLocalChanges;
  }
}

void ModelTypeWorker::OnFullCommitFailure(SyncCommitError commit_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  model_type_processor_->OnCommitFailed(commit_error);
}

size_t ModelTypeWorker::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(model_type_state_);
  memory_usage += EstimateMemoryUsage(entries_pending_decryption_);
  memory_usage += EstimateMemoryUsage(pending_updates_);
  return memory_usage;
}

bool ModelTypeWorker::CanCommitItems() const {
  // We can only commit if we've received the initial update response and aren't
  // blocked by missing encryption keys.
  return IsInitialSyncAtLeastPartiallyDone(
             model_type_state_.initial_sync_state()) &&
         !BlockForEncryption();
}

bool ModelTypeWorker::BlockForEncryption() const {
  if (!entries_pending_decryption_.empty()) {
    return true;
  }

  // Should be using encryption, but we do not have the keys.
  return encryption_enabled_ && !cryptographer_->CanEncrypt();
}

bool ModelTypeWorker::UpdateTypeEncryptionKeyName() {
  if (!encryption_enabled_) {
    // The type encryption key is expected to be empty.
    if (model_type_state_.encryption_key_name().empty()) {
      return false;
    }
    DLOG(WARNING) << ModelTypeToDebugString(type_)
                  << " : Had encryption disabled but non-empty encryption key "
                  << model_type_state_.encryption_key_name()
                  << ". Setting key to empty.";
    model_type_state_.clear_encryption_key_name();
    return true;
  }

  if (!cryptographer_->CanEncrypt()) {
    // There's no selected default key. Let's wait for one to be selected before
    // updating.
    return false;
  }

  std::string default_key_name = cryptographer_->GetDefaultEncryptionKeyName();
  DCHECK(!default_key_name.empty());
  DVLOG(1) << ModelTypeToDebugString(type_) << ": Updating encryption key "
           << model_type_state_.encryption_key_name() << " -> "
           << default_key_name;
  model_type_state_.set_encryption_key_name(default_key_name);
  return true;
}

void ModelTypeWorker::DecryptStoredEntities() {
  for (auto it = entries_pending_decryption_.begin();
       it != entries_pending_decryption_.end();) {
    const sync_pb::SyncEntity& encrypted_update = it->second;

    UpdateResponseData response_data;
    switch (PopulateUpdateResponseData(*cryptographer_, type_, encrypted_update,
                                       &response_data)) {
      case SUCCESS:
        pending_updates_.push_back(std::move(response_data));
        it = entries_pending_decryption_.erase(it);
        break;
      case DECRYPTION_PENDING:
        // Still cannot decrypt, move on and keep this one for later.
        ++it;
        break;
      case FAILED_TO_DECRYPT:
        // Decryption error should be permanent (e.g. corrupt data), since
        // decryption keys are up-to-date. Let's ignore this update to avoid
        // blocking other updates.
        it = entries_pending_decryption_.erase(it);
        break;
    }
  }

  // Note this can perfectly contain keys that were encrypting corrupt updates
  // (FAILED_TO_DECRYPT above); all that matters is the key was found.
  const std::vector<UnknownEncryptionKeyInfo> newly_found_keys =
      RemoveKeysNoLongerUnknown();
  for (const UnknownEncryptionKeyInfo& newly_found_key : newly_found_keys) {
    // Don't record UMA for the dominant case where the key was only unknown
    // while the cryptographer was pending external interaction.
    if (newly_found_key.get_updates_while_should_have_been_known > 0) {
      base::UmaHistogramCounts1000(
          kTimeUntilEncryptionKeyFoundHistogramName,
          newly_found_key.get_updates_while_should_have_been_known);
      base::UmaHistogramCounts1000(
          base::StrCat({kTimeUntilEncryptionKeyFoundHistogramName, ".",
                        ModelTypeToHistogramSuffix(type_)}),
          newly_found_key.get_updates_while_should_have_been_known);
    }
  }
}

void ModelTypeWorker::DeduplicatePendingUpdatesBasedOnServerId() {
  UpdateResponseDataList candidates;
  pending_updates_.swap(candidates);
  pending_updates_.reserve(candidates.size());

  std::map<std::string, size_t> id_to_index;
  for (UpdateResponseData& candidate : candidates) {
    if (candidate.entity.id.empty()) {
      continue;
    }
    // Try to insert. If we already saw an item with the same server id,
    // this will fail but give us its iterator.
    auto [it, success] =
        id_to_index.emplace(candidate.entity.id, pending_updates_.size());
    if (success) {
      // New server id, append at the end. Note that we already inserted
      // the correct index (|pending_updates_.size()|) above.
      pending_updates_.push_back(std::move(candidate));
      continue;
    }

    // Duplicate! Overwrite the existing update if |candidate| has a more recent
    // version.
    const size_t existing_index = it->second;
    UpdateResponseData& existing_update = pending_updates_[existing_index];
    if (candidate.response_version >= existing_update.response_version) {
      existing_update = std::move(candidate);
    }
  }
}

void ModelTypeWorker::DeduplicatePendingUpdatesBasedOnClientTagHash() {
  UpdateResponseDataList candidates;
  pending_updates_.swap(candidates);
  pending_updates_.reserve(candidates.size());

  std::map<ClientTagHash, size_t> tag_to_index;
  for (UpdateResponseData& candidate : candidates) {
    // Items with empty client tag hash just get passed through.
    if (candidate.entity.client_tag_hash.value().empty()) {
      pending_updates_.push_back(std::move(candidate));
      continue;
    }
    // Try to insert. If we already saw an item with the same client tag hash,
    // this will fail but give us its iterator.
    auto [it, success] = tag_to_index.emplace(candidate.entity.client_tag_hash,
                                              pending_updates_.size());
    if (success) {
      // New client tag hash, append at the end. Note that we already inserted
      // the correct index (|pending_updates_.size()|) above.
      pending_updates_.push_back(std::move(candidate));
      continue;
    }

    // Duplicate! Overwrite the existing update if |candidate| has a more recent
    // version.
    const size_t existing_index = it->second;
    UpdateResponseData& existing_update = pending_updates_[existing_index];
    if (candidate.response_version >= existing_update.response_version) {
      existing_update = std::move(candidate);
    }
  }
}

void ModelTypeWorker::DeduplicatePendingUpdatesBasedOnOriginatorClientItemId() {
  UpdateResponseDataList candidates;
  pending_updates_.swap(candidates);
  pending_updates_.reserve(candidates.size());

  std::map<std::string, size_t> id_to_index;
  for (UpdateResponseData& candidate : candidates) {
    // Entities with an item ID that is not a GUID just get passed through
    // without deduplication, which is the case for all datatypes except
    // bookmarks, as well as bookmarks created before 2015, when the item ID was
    // not globally unique across clients.
    if (!base::Uuid::ParseCaseInsensitive(
             candidate.entity.originator_client_item_id)
             .is_valid()) {
      pending_updates_.push_back(std::move(candidate));
      continue;
    }
    // Try to insert. If we already saw an item with the same originator item
    // ID, this will fail but give us its iterator.
    auto [it, success] = id_to_index.emplace(
        base::ToLowerASCII(candidate.entity.originator_client_item_id),
        pending_updates_.size());
    if (success) {
      // New item ID, append at the end. Note that we already inserted the
      // correct index (|pending_updates_.size()|) above.
      pending_updates_.push_back(std::move(candidate));
      continue;
    }

    // Duplicate! Overwrite the existing update if |candidate| has a more recent
    // version.
    const size_t existing_index = it->second;
    UpdateResponseData& existing_update = pending_updates_[existing_index];
    if (candidate.response_version >= existing_update.response_version) {
      existing_update = std::move(candidate);
    }
  }
}

bool ModelTypeWorker::ShouldIgnoreUpdatesEncryptedWith(
    const std::string& key_name) {
  if (!base::Contains(unknown_encryption_keys_by_name_, key_name)) {
    return false;
  }
  if (unknown_encryption_keys_by_name_.at(key_name)
          .get_updates_while_should_have_been_known <
      min_get_updates_to_ignore_key_) {
    return false;
  }
  return base::FeatureList::IsEnabled(kIgnoreSyncEncryptionKeysLongMissing);
}

void ModelTypeWorker::MaybeDropPendingUpdatesEncryptedWith(
    const std::string& key_name) {
  if (!ShouldIgnoreUpdatesEncryptedWith(key_name)) {
    return;
  }

  size_t updates_before_dropping = entries_pending_decryption_.size();
  base::EraseIf(entries_pending_decryption_, [&](const auto& id_and_update) {
    return key_name == GetEncryptionKeyName(id_and_update.second);
  });

  // If updates were dropped, record how many.
  const size_t dropped_updates =
      updates_before_dropping - entries_pending_decryption_.size();
  if (dropped_updates > 0) {
    base::UmaHistogramCounts1000(
        kUndecryptablePendingUpdatesDroppedHistogramName, dropped_updates);
    base::UmaHistogramCounts1000(
        base::StrCat({kUndecryptablePendingUpdatesDroppedHistogramName, ".",
                      ModelTypeToHistogramSuffix(type_)}),
        dropped_updates);
  }
}

std::vector<ModelTypeWorker::UnknownEncryptionKeyInfo>
ModelTypeWorker::RemoveKeysNoLongerUnknown() {
  std::set<std::string> keys_blocking_updates;
  for (const auto& [id, update] : entries_pending_decryption_) {
    const std::string key_name = GetEncryptionKeyName(update);
    DCHECK(!key_name.empty());
    keys_blocking_updates.insert(key_name);
  }

  std::vector<ModelTypeWorker::UnknownEncryptionKeyInfo> removed_keys;
  base::EraseIf(
      unknown_encryption_keys_by_name_, [&](const auto& key_and_info) {
        if (base::Contains(keys_blocking_updates, key_and_info.first)) {
          return false;
        }
        removed_keys.push_back(key_and_info.second);
        return true;
      });

  return removed_keys;
}

bool ModelTypeWorker::HasNonDeletionUpdates() const {
  for (const UpdateResponseData& update : pending_updates_) {
    if (!update.entity.is_deleted()) {
      return true;
    }
  }
  return false;
}

void ModelTypeWorker::ExtractGcDirective() {
  DCHECK(model_type_state_.has_progress_marker());
  // This is a workaround for multiple GetUpdates during one sync cycle. The
  // server returns gc_directive only if there are updates for the data type.
  // For example, if there are many bookmarks to download and several Wallet
  // entities (which use GC directive), there might be the following sequence of
  // GetUpdates responses:
  //
  // 1. Response with Wallet updates and bookmarks:
  // * wallet_entities: 10
  // ** progress_marker: {progress_token: "w1", gc_directive: "1"}
  // * bookmark_entities: 10
  // ** progress_marker: {progress_token: "b1"}
  //
  // 2. Response with remaining bookmarks only:
  // * wallet_entities: 0
  // ** progress_marker: {progress_token: "w1"}
  // * bookmark_entities: 15
  // ** progress_marker: {progress_token: "b2"}
  //
  // In this case the GC directive from the first request has to be kept until
  // the end of the sync cycle.
  // TODO(crbug.com/1356900): consider a better approach instead of this
  // workaround.

  if (model_type_state_.progress_marker().has_gc_directive()) {
    // Keep a new GC directive if received.
    pending_gc_directive_ = model_type_state_.progress_marker().gc_directive();
    model_type_state_.mutable_progress_marker()->clear_gc_directive();
    return;
  }

  if (pending_gc_directive_.has_value() &&
      !base::FeatureList::IsEnabled(kSyncKeepGcDirectiveDuringSyncCycle)) {
    // Remove the GC directive if not present in the response, to mimic the
    // previous behavior.
    pending_gc_directive_.reset();
    return;
  }

  // Note that normally if the server returns non-empty updates for a
  // download-only data type, it returns a non-empty |gc_directive| as well.
  // However, it's safer to keep the GC directive until it's applied even if the
  // server returns non-empty updates without GC directive within the same sync
  // cycle.
}

void ModelTypeWorker::RecordRemoteInvalidation(
    std::unique_ptr<SyncInvalidation> incoming) {
  DCHECK(incoming);
  // Merge the incoming invalidation into our list of pending invalidations.
  //
  // We won't use STL algorithms here because our concept of equality doesn't
  // quite fit the expectations of set_intersection.  In particular, two
  // invalidations can be equal according to the SingleTopicInvalidationSet's
  // rules (ie. have equal versions), but still have different AckHandle values
  // and need to be acknowledged separately.
  //
  // The invalidations service can only track one outsanding invalidation per
  // type and version, so the acknowledgement here should be redundant.  We'll
  // acknowledge them anyway since it should do no harm, and makes this code a
  // bit easier to test.
  //
  // Overlaps should be extremely rare for most invalidations.  They can happen
  // for unknown version invalidations, though.

  auto it = pending_invalidations_.begin();

  // Find the lower bound.
  while (it != pending_invalidations_.end() &&
         SyncInvalidation::LessThanByVersion(*(it->pending_invalidation),
                                             *incoming)) {
    it++;
  }

  if (it != pending_invalidations_.end() &&
      !SyncInvalidation::LessThanByVersion(*incoming,
                                           *(it->pending_invalidation)) &&
      !SyncInvalidation::LessThanByVersion(*(it->pending_invalidation),
                                           *incoming)) {
    // Incoming overlaps with existing.  Either both are unknown versions
    // (likely) or these two have the same version number (very unlikely).
    // Acknowledge and overwrite existing.

    // Insert before the existing and get iterator to inserted.
    auto it2 = pending_invalidations_.insert(it, {std::move(incoming), false});

    // Increment that iterator to the old one, then acknowledge and remove it.
    LogPendingInvalidationStatus(
        (it2->pending_invalidation)->IsUnknownVersion()
            ? PendingInvalidationStatus::kSameUnknownVersion
            : PendingInvalidationStatus::kSameKnownVersion);
    ++it2;
    (it2->pending_invalidation)->Acknowledge();
    pending_invalidations_.erase(it2);
  } else {
    // The incoming has a version not in the pending_invalidations_ list.
    // Add it to the list at the proper position.
    pending_invalidations_.insert(it, {std::move(incoming), false});
  }

  // The incoming invalidation may have caused us to exceed our buffer size.
  // Trim some items from our list, if necessary.
  while (pending_invalidations_.size() > kMaxPendingInvalidations) {
    has_dropped_invalidation_ = true;
    LogPendingInvalidationStatus(
        PendingInvalidationStatus::kInvalidationsOverflow);
    pending_invalidations_.front().pending_invalidation->Drop();
    pending_invalidations_.erase(pending_invalidations_.begin());
  }
  nudge_handler_->SetHasPendingInvalidations(type_, HasPendingInvalidations());
  if (base::FeatureList::IsEnabled(kSyncPersistInvalidations)) {
    SendPendingInvalidationsToProcessor();
  }
}

void ModelTypeWorker::CollectPendingInvalidations(
    sync_pb::GetUpdateTriggers* msg) {
  // Fill the list of payloads, if applicable.  The payloads must be ordered
  // oldest to newest, so we insert them in the same order as we've been storing
  // them internally.
  for (PendingInvalidation& invalidation : pending_invalidations_) {
    if (!invalidation.pending_invalidation->IsUnknownVersion()) {
      msg->add_notification_hint(
          invalidation.pending_invalidation->GetPayload());
    }
    invalidation.is_processed = true;
  }

  msg->set_server_dropped_hints(
      !pending_invalidations_.empty() &&
      (pending_invalidations_.begin()->pending_invalidation)
          ->IsUnknownVersion());
  msg->set_client_dropped_hints(has_dropped_invalidation_);
}

bool ModelTypeWorker::HasPendingInvalidations() const {
  return !pending_invalidations_.empty() || has_dropped_invalidation_;
}

void ModelTypeWorker::SendPendingInvalidationsToProcessor() {
  DCHECK(base::FeatureList::IsEnabled(kSyncPersistInvalidations));
  UpdateModelTypeStateInvalidations();
  model_type_processor_->StorePendingInvalidations(
      std::vector<sync_pb::ModelTypeState::Invalidation>(
          model_type_state_.invalidations().begin(),
          model_type_state_.invalidations().end()));
}

void ModelTypeWorker::UpdateModelTypeStateInvalidations() {
  DCHECK(base::FeatureList::IsEnabled(kSyncPersistInvalidations));
  model_type_state_.clear_invalidations();
  for (const auto& inv : pending_invalidations_) {
    SyncInvalidation* invalidation = inv.pending_invalidation.get();
    sync_pb::ModelTypeState_Invalidation* invalidation_to_store =
        model_type_state_.add_invalidations();
    invalidation_to_store->set_hint(invalidation->GetPayload());
    if (!invalidation->IsUnknownVersion()) {
      invalidation_to_store->set_version(invalidation->GetVersion());
    }
  }
}

GetLocalChangesRequest::GetLocalChangesRequest(
    CancelationSignal* cancelation_signal)
    : cancelation_signal_(cancelation_signal),
      response_accepted_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED) {}

GetLocalChangesRequest::~GetLocalChangesRequest() = default;

void GetLocalChangesRequest::OnCancelationSignalReceived() {
  response_accepted_.Signal();
}

void GetLocalChangesRequest::WaitForResponseOrCancelation() {
  if (!cancelation_signal_->TryRegisterHandler(this)) {
    return;
  }

  {
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    response_accepted_.Wait();
  }

  cancelation_signal_->UnregisterHandler(this);
}

void GetLocalChangesRequest::SetResponse(
    CommitRequestDataList&& local_changes) {
  response_ = std::move(local_changes);
  response_accepted_.Signal();
}

bool GetLocalChangesRequest::WasCancelled() {
  return cancelation_signal_->IsSignalled();
}

CommitRequestDataList&& GetLocalChangesRequest::ExtractResponse() {
  return std::move(response_);
}

}  // namespace syncer
