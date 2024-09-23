// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/data_type_worker.h"

#include <stdint.h>

#include <map>
#include <set>
#include <utility>

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
#include "components/sync/base/data_type.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/features.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/sync_invalidation_adapter.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/bookmark_update_preprocessing.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/commit_contribution.h"
#include "components/sync/engine/commit_contribution_impl.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/engine/data_type_processor.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/data_type_state_helper.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/sync_entity.pb.h"

namespace syncer {

namespace {

const char kUndecryptablePendingUpdatesDroppedHistogramName[] =
    "Sync.DataTypeUndecryptablePendingUpdatesDropped";
const char kBlockedByUndecryptableUpdateHistogramName[] =
    "Sync.DataTypeBlockedDueToUndecryptableUpdate";
const char kPasswordNotesStateHistogramName[] =
    "Sync.PasswordNotesStateInUpdate";
constexpr char kEntityEncryptionResultHistogramName[] =
    "Sync.EntityEncryptionSucceeded";

// Sync ignores updates encrypted with keys that have been missing for too long
// from this client and will proceed normally as if those updates didn't exist.
// The notion of "too long" is measured in number of GetUpdates and is
// determined by this constant. The counter is in-memory only.
constexpr int kMinGuResponsesToIgnoreKey = 3;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CrossUserSharingDecryptionResult)
enum class CrossUserSharingDecryptionResult {
  kSuccess = 0,
  kInvitationMissingFields = 1,
  kFailedToDecryptInvitation = 2,
  kFailedToParseDecryptedInvitation = 3,

  kMaxValue = kFailedToParseDecryptedInvitation,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:CrossUserSharingDecryptionResult)

void LogPasswordNotesState(PasswordNotesStateForUMA state) {
  base::UmaHistogramEnumeration(kPasswordNotesStateHistogramName, state);
}

void LogEncryptionResult(DataType type, bool success) {
  base::UmaHistogramBoolean(kEntityEncryptionResultHistogramName, success);
  base::UmaHistogramBoolean(
      base::StrCat({kEntityEncryptionResultHistogramName, ".",
                    DataTypeToHistogramSuffix(type)}),
      success);
}

void LogCrossUserSharingDecryptionResult(
    CrossUserSharingDecryptionResult result) {
  base::UmaHistogramEnumeration("Sync.CrossUserSharingDecryptionResult",
                                result);
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

void AdaptClientTagForFullUpdateData(DataType data_type,
                                     syncer::EntityData* data) {
  // Server does not send any client tags for wallet data entities or offer data
  // entities. This code manually asks the bridge to create the client tags for
  // each entity, so that we can use ClientTagBasedDataTypeProcessor for
  // AUTOFILL_WALLET_DATA or AUTOFILL_WALLET_OFFER.
  if (data->legacy_parent_id == "0") {
    // Ignore the permanent root node as that one should have no client tag
    // hash.
    return;
  }
  DCHECK(!data->specifics.has_encrypted());
  if (data_type == AUTOFILL_WALLET_DATA) {
    DCHECK(data->specifics.has_autofill_wallet());
    data->client_tag_hash = ClientTagHash::FromUnhashed(
        AUTOFILL_WALLET_DATA, GetUnhashedClientTagFromAutofillWalletSpecifics(
                                  data->specifics.autofill_wallet()));
  } else if (data_type == AUTOFILL_WALLET_OFFER) {
    DCHECK(data->specifics.has_autofill_offer());
    data->client_tag_hash = ClientTagHash::FromUnhashed(
        AUTOFILL_WALLET_OFFER, GetUnhashedClientTagFromAutofillOfferSpecifics(
                                   data->specifics.autofill_offer()));
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void AdaptWebAuthnClientTagHash(syncer::EntityData* data) {
  // Google Play Services may create entities where the client_tag_hash doesn't
  // conform to the form expected by Chromium. These values are the hex-encoded,
  // 16-byte random `sync_id` value, and will therefore always be 32 bytes long.
  // Valid ClientTagHash values are Base64(SHA1(protobuf_prefix + client_tag))
  // and therefore always 28 bytes.
  const std::string& client_tag_hash = data->client_tag_hash.value();
  std::string sync_id;
  if (client_tag_hash.size() == 32 &&
      base::HexStringToString(client_tag_hash, &sync_id) &&
      // Deletions don't include the specifics, only the client_tag_hash.
      (!data->specifics.has_webauthn_credential() ||
       // Otherwise, check that the client_tag_hash really is the hex encoded
       // sync_id.
       sync_id == data->specifics.webauthn_credential().sync_id())) {
    data->client_tag_hash =
        ClientTagHash::FromUnhashed(DataType::WEBAUTHN_CREDENTIAL, sync_id);
  }
}

// Returns empty string if |entity| is not encrypted.
// TODO(crbug.com/40141634): Consider moving this to a util file and converting
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
  // Passwords and password sharing invitations have their own encryption
  // schemes and they are handled in different helpers.
  CHECK(!in.has_incoming_password_sharing_invitation());
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
// cryptographer.CanDecrypt(in.encrypted()) must return true.
//
// Returns false if the decryption failed. There are no guarantees about the
// contents of |out| when that happens.
//
// In theory, this should never fail. Only corrupt or invalid entries could
// cause this to fail, and no clients are known to create such entries. The
// failure case is an attempt to be defensive against bad input.
bool DecryptPasswordSpecifics(const Cryptographer& cryptographer,
                              const sync_pb::PasswordSpecifics& in,
                              sync_pb::PasswordSpecificsData* out) {
  CHECK(in.has_encrypted());
  CHECK(cryptographer.CanDecrypt(in.encrypted()));

  if (!cryptographer.Decrypt(in.encrypted(), out)) {
    DLOG(ERROR) << "Failed to decrypt a decryptable password";
    return false;
  }
  // The `notes` field in the PasswordSpecificsData is the authoritative value.
  // When set, it disregards whatever `encrypted_notes_backup` contains.
  if (out->has_notes()) {
    LogPasswordNotesState(PasswordNotesStateForUMA::kSetInSpecificsData);
    return true;
  }
  if (!in.has_encrypted_notes_backup()) {
    LogPasswordNotesState(PasswordNotesStateForUMA::kUnset);
    return true;
  }
  // It is guaranteed that if `encrypted()` is decryptable, then
  // `encrypted_notes_backup()` must be decryptable too. Failure to decrypt
  // `encrypted_notes_backup()` indicates a data corruption.
  if (!cryptographer.Decrypt(in.encrypted_notes_backup(),
                             out->mutable_notes())) {
    LogPasswordNotesState(
        PasswordNotesStateForUMA::kSetOnlyInBackupButCorrupted);
    return false;
  }
  LogPasswordNotesState(PasswordNotesStateForUMA::kSetOnlyInBackup);
  // TODO(crbug.com/40225853): Properly handle the case when both blobs are
  // decryptable but with different keys. Ideally the password should be
  // re-uploaded potentially by setting needs_reupload boolean in
  // UpdateResponseData or EntityData.
  return true;
}

bool DecryptIncomingPasswordSharingInvitationSpecifics(
    const Cryptographer& cryptographer,
    const sync_pb::IncomingPasswordSharingInvitationSpecifics& invitation,
    sync_pb::PasswordSharingInvitationData* unencrypted_invitation_data) {
  if (!invitation.has_encrypted_password_sharing_invitation_data() ||
      !invitation.sender_info().has_cross_user_sharing_public_key()) {
    LogCrossUserSharingDecryptionResult(
        CrossUserSharingDecryptionResult::kInvitationMissingFields);
    DLOG(ERROR) << "The invitation is missing required fields";
    return false;
  }

  std::optional<std::vector<uint8_t>> decrypted =
      cryptographer.AuthDecryptForCrossUserSharing(
          base::as_bytes(base::make_span(
              invitation.encrypted_password_sharing_invitation_data())),
          base::as_bytes(base::make_span(invitation.sender_info()
                                             .cross_user_sharing_public_key()
                                             .x25519_public_key())),
          invitation.recipient_key_version());
  if (!decrypted) {
    LogCrossUserSharingDecryptionResult(
        CrossUserSharingDecryptionResult::kFailedToDecryptInvitation);
    DLOG(ERROR) << "Failed to decrypt the invitation";
    return false;
  }

  if (!unencrypted_invitation_data->ParseFromArray(decrypted->data(),
                                                   decrypted->size())) {
    LogCrossUserSharingDecryptionResult(
        CrossUserSharingDecryptionResult::kFailedToParseDecryptedInvitation);
    DLOG(ERROR) << "Failed to parse the decrypted invitation";
    return false;
  }

  LogCrossUserSharingDecryptionResult(
      CrossUserSharingDecryptionResult::kSuccess);
  return true;
}

}  // namespace

DataTypeWorker::DataTypeWorker(DataType type,
                               const sync_pb::DataTypeState& initial_state,
                               Cryptographer* cryptographer,
                               bool encryption_enabled,
                               PassphraseType passphrase_type,
                               NudgeHandler* nudge_handler,
                               CancelationSignal* cancelation_signal)
    : type_(type),
      cryptographer_(cryptographer),
      nudge_handler_(nudge_handler),
      cancelation_signal_(cancelation_signal),
      data_type_state_(initial_state),
      encryption_enabled_(encryption_enabled),
      passphrase_type_(passphrase_type) {
  DCHECK(cryptographer_);
  DCHECK(!AlwaysEncryptedUserTypes().Has(type_) || encryption_enabled_);

  // GC directive is stored independently of progress marker and is used during
  // a sync cycle (i.e. in-memory only). Clear GC directive on load to clean up
  // previously persisted values.
  data_type_state_.mutable_progress_marker()->clear_gc_directive();

  if (!data_type_state_.invalidations().empty()) {
    if (static_cast<size_t>(data_type_state_.invalidations_size()) >
        kMaxPendingInvalidations) {
      DVLOG(1) << "Cleaning invalidations in |data_type_state_| due to "
                  "invalidations overflow.";
      data_type_state_.clear_invalidations();
    }
    // TODO(crbug.com/40239360): Persisted invaldiations are loaded in
    // DataTypeWorker::ctor(), but sync cycle is not scheduled. New sync
    // cycle has to be triggered right after we loaded persisted
    // invalidations.
    for (int i = 0; i < data_type_state_.invalidations_size(); ++i) {
      pending_invalidations_.emplace_back(
          std::make_unique<SyncInvalidationAdapter>(
              data_type_state_.invalidations(i).hint(),
              data_type_state_.invalidations(i).has_version()
                  ? std::optional<int64_t>(
                        data_type_state_.invalidations(i).version())
                  : std::nullopt),
          false);
    }

    bool is_version_order_correct = true;
    for (size_t i = 1; i < pending_invalidations_.size(); ++i) {
      is_version_order_correct &= (SyncInvalidation::LessThanByVersion(
          *pending_invalidations_[i - 1].pending_invalidation,
          *pending_invalidations_[i].pending_invalidation));
    }
    if (!is_version_order_correct) {
      DVLOG(1) << "Cleaning invalidations in |data_type_state| due to "
                  "incorrect version order.";
      pending_invalidations_.clear();
      data_type_state_.clear_invalidations();
    }
  }

  if (!CommitOnlyTypes().Has(GetDataType())) {
    DCHECK_EQ(type, GetDataTypeFromSpecificsFieldNumber(
                        initial_state.progress_marker().data_type_id()));
  }
}

DataTypeWorker::PendingInvalidation::PendingInvalidation() = default;
DataTypeWorker::PendingInvalidation::PendingInvalidation(
    PendingInvalidation&&) = default;
DataTypeWorker::PendingInvalidation&
DataTypeWorker::PendingInvalidation::operator=(PendingInvalidation&&) = default;
DataTypeWorker::PendingInvalidation::PendingInvalidation(
    std::unique_ptr<SyncInvalidation> invalidation,
    bool is_processed)
    : pending_invalidation(std::move(invalidation)),
      is_processed(is_processed) {}
DataTypeWorker::PendingInvalidation::~PendingInvalidation() = default;

DataTypeWorker::~DataTypeWorker() {
  if (data_type_processor_) {
    // This will always be the case in production today.
    data_type_processor_->DisconnectSync();
  }
  for (size_t i = 0; i < pending_invalidations_.size(); ++i) {
    LogPendingInvalidationStatus(PendingInvalidationStatus::kLost);
  }
}

void DataTypeWorker::LogPendingInvalidationStatus(
    PendingInvalidationStatus status) {
  base::UmaHistogramEnumeration("Sync.PendingInvalidationStatus", status);
}

void DataTypeWorker::ConnectSync(
    std::unique_ptr<DataTypeProcessor> data_type_processor) {
  DCHECK(!data_type_processor_);
  DCHECK(data_type_processor);

  data_type_processor_ = std::move(data_type_processor);
  // TODO(crbug.com/346777544): CommitQueueProxy is only needed by the
  // DataTypeProcessorProxy implementation, so it could possibly be moved
  // there % changing ConnectSync() to take a raw pointer. This then allows
  // removing base::test::SingleThreadTaskEnvironment from the unit test.
  data_type_processor_->ConnectSync(
      std::make_unique<CommitQueueProxy>(weak_ptr_factory_.GetWeakPtr()));

  if (!IsInitialSyncDone(data_type_state_.initial_sync_state())) {
    nudge_handler_->NudgeForInitialDownload(type_);
  }

  // |data_type_state_| might have an outdated encryption key name, e.g.
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

DataType DataTypeWorker::GetDataType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return type_;
}

void DataTypeWorker::EnableEncryption() {
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

void DataTypeWorker::OnCryptographerChange() {
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

void DataTypeWorker::UpdatePassphraseType(PassphraseType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  passphrase_type_ = type;
}

bool DataTypeWorker::IsInitialSyncEnded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsInitialSyncDone(data_type_state_.initial_sync_state());
}

const sync_pb::DataTypeProgressMarker& DataTypeWorker::GetDownloadProgress()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return data_type_state_.progress_marker();
}

const sync_pb::DataTypeContext& DataTypeWorker::GetDataTypeContext() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return data_type_state_.type_context();
}

void DataTypeWorker::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const sync_pb::DataTypeContext& mutated_context,
    const SyncEntityList& applicable_updates,
    StatusController* status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool is_initial_sync =
      !IsInitialSyncDone(data_type_state_.initial_sync_state());

  // TODO(rlarocque): Handle data type context conflicts.
  *data_type_state_.mutable_type_context() = mutated_context;

  if (progress_marker.has_gc_directive()) {
    if (progress_marker.gc_directive().has_version_watermark()) {
      // Clean up all the pending updates because a new GC directive has been
      // received which means that all existing data should be cleaned up.
      pending_updates_.clear();
      entries_pending_decryption_.clear();
    }

    // Ignore collaboration GC for non-shared types.
    if (progress_marker.gc_directive().has_collaboration_gc() &&
        SharedTypes().Has(type_)) {
      // Clean up all the pending updates related to inactive collaborations for
      // the shared types.
      auto active_collaborations =
          base::MakeFlatSet<std::string>(progress_marker.gc_directive()
                                             .collaboration_gc()
                                             .active_collaboration_ids());
      std::erase_if(pending_updates_, [&active_collaborations](
                                          const UpdateResponseData& update) {
        return !active_collaborations.contains(update.entity.collaboration_id);
      });
      std::erase_if(entries_pending_decryption_,
                    [&active_collaborations](const auto& pending_decryption) {
                      const sync_pb::SyncEntity& entity =
                          pending_decryption.second;
                      return !active_collaborations.contains(
                          entity.collaboration().collaboration_id());
                    });
    }
  }

  *data_type_state_.mutable_progress_marker() = progress_marker;
  ExtractGcDirective();

  for (const sync_pb::SyncEntity* update_entity : applicable_updates) {
    RecordEntityChangeMetrics(
        type_, is_initial_sync ? DataTypeEntityChange::kRemoteInitialUpdate
                               : DataTypeEntityChange::kRemoteNonInitialUpdate);

    if (update_entity->deleted()) {
      status->increment_num_tombstone_updates_downloaded_by(1);
      if (!is_initial_sync) {
        RecordEntityChangeMetrics(type_, DataTypeEntityChange::kRemoteDeletion);
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
        SyncRecordDataTypeUpdateDropReason(UpdateDropReason::kDecryptionPending,
                                           type_);

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
          DCHECK(!entries_pending_decryption_.contains(server_id) ||
                 GetEncryptionKeyName(entries_pending_decryption_[server_id]) !=
                     key_name);
          SyncRecordDataTypeUpdateDropReason(
              UpdateDropReason::kDecryptionPendingForTooLong, type_);
          break;
        }
        // Copy the sync entity for later decryption.
        // TODO(crbug.com/40805099): Any write to |entries_pending_decryption_|
        // should do like DeduplicatePendingUpdatesBasedOnServerId() and honor
        // entity version. Additionally, it should look up the same server id
        // in |pending_updates_| and compare versions. In fact, the 2 containers
        // should probably be moved to a separate class with unit tests.
        entries_pending_decryption_[server_id] = *update_entity;
        break;
      }
      case FAILED_TO_DECRYPT:
        // Failed to decrypt the entity. Likely it is corrupt. Move on.
        SyncRecordDataTypeUpdateDropReason(UpdateDropReason::kFailedToDecrypt,
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
                                  DataTypeHistogramValue(type_));
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
DataTypeWorker::DecryptionStatus DataTypeWorker::PopulateUpdateResponseData(
    const Cryptographer& cryptographer,
    DataType data_type,
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
  // DataTypeWorker receives some even though its type doesn't use encryption.
  // If so, still try to decrypt with the available keys regardless.
  if (specifics.password().has_encrypted()) {
    // Passwords use their own legacy encryption scheme.
    if (!cryptographer.CanDecrypt(specifics.password().encrypted())) {
      return DECRYPTION_PENDING;
    }
    if (!DecryptPasswordSpecifics(cryptographer, specifics.password(),
                                  data.specifics.mutable_password()
                                      ->mutable_client_only_encrypted_data())) {
      return FAILED_TO_DECRYPT;
    }
    specifics_were_encrypted = true;
  } else if (specifics.has_incoming_password_sharing_invitation()) {
    // IncomingPasswordSharingInvitationSpecifics contains a mix of encrypted
    // and unencrypted fields. We start by copying over everything to make sure
    // all unecrypted fields are carried over to the UpdateResponseData, and
    // then decrypt the encrypted part.
    *data.specifics.mutable_incoming_password_sharing_invitation() =
        specifics.incoming_password_sharing_invitation();
    // Password sharing invitations use their own encryption scheme.
    // DECRYPTION_PENDING is not used for sharing invitations since the password
    // should be encrypted using recipient's public key (i.e. it's committed to
    // the server), and hence it's expected to be present.
    if (!DecryptIncomingPasswordSharingInvitationSpecifics(
            cryptographer, specifics.incoming_password_sharing_invitation(),
            data.specifics.mutable_incoming_password_sharing_invitation()
                ->mutable_client_only_unencrypted_data())) {
      return FAILED_TO_DECRYPT;
    }
    data.specifics.mutable_incoming_password_sharing_invitation()
        ->clear_encrypted_password_sharing_invitation_data();
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

  // Populate shared type fields.
  if (SharedTypes().Has(data_type)) {
    data.collaboration_id = update_entity.collaboration().collaboration_id();
  }

  // Populate |originator_cache_guid| and |originator_client_item_id|. This is
  // currently relevant only for bookmarks.
  data.originator_cache_guid = update_entity.originator_cache_guid();
  data.originator_client_item_id = update_entity.originator_client_item_id();

  // Adapt the update for compatibility.
  if (data_type == BOOKMARKS) {
    data.is_bookmark_unique_position_in_specifics_preprocessed =
        AdaptUniquePositionForBookmark(update_entity, &data.specifics);
    AdaptTypeForBookmark(update_entity, &data.specifics);
    AdaptTitleForBookmark(update_entity, &data.specifics,
                          specifics_were_encrypted);
    AdaptGuidForBookmark(update_entity, &data.specifics);
    // Note that the parent GUID in specifics cannot be adapted/populated here,
    // because the logic requires access to tracked entities. Hence, it is
    // done by BookmarkDataTypeProcessor, with logic implemented in
    // components/sync_bookmarks/parent_guid_preprocessing.cc.
  } else if (data_type == AUTOFILL_WALLET_DATA ||
             data_type == AUTOFILL_WALLET_OFFER) {
    AdaptClientTagForFullUpdateData(data_type, &data);
  } else if (data_type == WEBAUTHN_CREDENTIAL) {
    AdaptWebAuthnClientTagHash(&data);
  }

  response_data->entity = std::move(data);
  return SUCCESS;
}

void DataTypeWorker::ApplyUpdates(StatusController* status, bool cycle_done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Indicate the new initial-sync state to the processor: If the current sync
  // cycle was completed, the initial sync must be done. Otherwise, it's started
  // now. The latter can only happen for ApplyUpdatesImmediatelyTypes(), since
  // other types wait for the cycle to complete before applying any updates.
  // Note that the initial sync technically isn't started/done yet but by the
  // time this value is persisted to disk on the model thread it will be.
  if (cycle_done) {
    data_type_state_.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  } else {
    DCHECK(ApplyUpdatesImmediatelyTypes().Has(type_));
    if (data_type_state_.initial_sync_state() !=
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE) {
      data_type_state_.set_initial_sync_state(
          sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_PARTIALLY_DONE);
    }
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
    UpdateDataTypeStateInvalidations();

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

void DataTypeWorker::SendPendingUpdatesToProcessorIfReady() {
  DCHECK(data_type_processor_);

  if (!IsInitialSyncAtLeastPartiallyDone(
          data_type_state_.initial_sync_state())) {
    return;
  }

  if (BlockForEncryption()) {
    return;
  }

  DCHECK(!AlwaysEncryptedUserTypes().Has(type_) || encryption_enabled_);
  DCHECK(!encryption_enabled_ ||
         !data_type_state_.encryption_key_name().empty());
  DCHECK(entries_pending_decryption_.empty());

  DVLOG(1) << DataTypeToDebugString(type_) << ": "
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

  data_type_processor_->OnUpdateReceived(data_type_state_,
                                         std::move(pending_updates_),
                                         std::move(pending_gc_directive_));
  pending_updates_.clear();
  pending_gc_directive_.reset();
}

void DataTypeWorker::NudgeForCommit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_local_changes_state_ = kNewlyNudgedLocalChanges;
  NudgeIfReadyToCommit();
}

void DataTypeWorker::NudgeIfReadyToCommit() {
  // TODO(crbug.com/40173160): |kNoNudgedLocalChanges| is used to keep the
  // existing behaviour. But perhaps there is no need to nudge for commit if all
  // known changes are already in flight.
  if (has_local_changes_state_ != kNoNudgedLocalChanges && CanCommitItems()) {
    nudge_handler_->NudgeForCommit(type_);
  }
}

std::unique_ptr<CommitContribution> DataTypeWorker::GetContribution(
    size_t max_entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(
      IsInitialSyncAtLeastPartiallyDone(data_type_state_.initial_sync_state()));
  DCHECK(data_type_processor_);

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
      base::MakeRefCounted<GetLocalChangesRequest>();
  data_type_processor_->GetLocalChanges(
      max_entries,
      base::BindOnce(&GetLocalChangesRequest::SetResponse, request));
  request->WaitForResponseOrCancelation(cancelation_signal_);
  CommitRequestDataList response;
  if (!cancelation_signal_->IsSignalled()) {
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

  if (type_ == OUTGOING_PASSWORD_SHARING_INVITATION) {
    // Password sharing invitation types have different encryption scheme and
    // are handled separately.
    EncryptOutgoingPasswordSharingInvitations(&response);
  } else if (type_ == PASSWORDS) {
    EncryptPasswordSpecificsData(&response);
  } else if (encryption_enabled_) {
    EncryptSpecifics(&response);
  }

  DCHECK(!AlwaysEncryptedUserTypes().Has(type_) || encryption_enabled_);
  DCHECK(!encryption_enabled_ ||
         (data_type_state_.encryption_key_name() ==
          cryptographer_->GetDefaultEncryptionKeyName()));

  return std::make_unique<CommitContributionImpl>(
      type_, data_type_state_.type_context(), std::move(response),
      base::BindOnce(&DataTypeWorker::OnCommitResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DataTypeWorker::OnFullCommitFailure,
                     weak_ptr_factory_.GetWeakPtr()),
      passphrase_type_);
}

bool DataTypeWorker::HasLocalChanges() const {
  return has_local_changes_state_ != kNoNudgedLocalChanges;
}

void DataTypeWorker::OnCommitResponse(
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Send the responses back to the model thread. It needs to know which
  // items have been successfully committed (it can save that information in
  // permanent storage) and which failed (it can e.g. notify the user).
  data_type_processor_->OnCommitCompleted(
      data_type_state_, committed_response_list, error_response_list);

  if (has_local_changes_state_ == kAllNudgedLocalChangesInFlight) {
    // There are no new nudged changes since last commit.
    has_local_changes_state_ = kNoNudgedLocalChanges;
  }
}

void DataTypeWorker::OnFullCommitFailure(SyncCommitError commit_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  data_type_processor_->OnCommitFailed(commit_error);
}

size_t DataTypeWorker::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(data_type_state_);
  memory_usage += EstimateMemoryUsage(entries_pending_decryption_);
  memory_usage += EstimateMemoryUsage(pending_updates_);
  return memory_usage;
}

bool DataTypeWorker::CanCommitItems() const {
  // We can only commit if we've received the initial update response and aren't
  // blocked by missing encryption keys.
  return IsInitialSyncAtLeastPartiallyDone(
             data_type_state_.initial_sync_state()) &&
         !BlockForEncryption();
}

bool DataTypeWorker::BlockForEncryption() const {
  if (!entries_pending_decryption_.empty()) {
    return true;
  }

  // Should be using encryption, but we do not have the keys.
  return encryption_enabled_ && !cryptographer_->CanEncrypt();
}

bool DataTypeWorker::UpdateTypeEncryptionKeyName() {
  if (!encryption_enabled_) {
    // The type encryption key is expected to be empty.
    if (data_type_state_.encryption_key_name().empty()) {
      return false;
    }
    DLOG(WARNING) << DataTypeToDebugString(type_)
                  << " : Had encryption disabled but non-empty encryption key "
                  << data_type_state_.encryption_key_name()
                  << ". Setting key to empty.";
    data_type_state_.clear_encryption_key_name();
    return true;
  }

  if (!cryptographer_->CanEncrypt()) {
    // There's no selected default key. Let's wait for one to be selected before
    // updating.
    return false;
  }

  std::string default_key_name = cryptographer_->GetDefaultEncryptionKeyName();
  DCHECK(!default_key_name.empty());
  DVLOG(1) << DataTypeToDebugString(type_) << ": Updating encryption key "
           << data_type_state_.encryption_key_name() << " -> "
           << default_key_name;
  data_type_state_.set_encryption_key_name(default_key_name);
  return true;
}

void DataTypeWorker::DecryptStoredEntities() {
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

  // Note this can perfectly remove keys that were encrypting corrupt updates
  // (FAILED_TO_DECRYPT above).
  RemoveKeysNoLongerUnknown();
}

void DataTypeWorker::DeduplicatePendingUpdatesBasedOnServerId() {
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

void DataTypeWorker::DeduplicatePendingUpdatesBasedOnClientTagHash() {
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

void DataTypeWorker::DeduplicatePendingUpdatesBasedOnOriginatorClientItemId() {
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

bool DataTypeWorker::ShouldIgnoreUpdatesEncryptedWith(
    const std::string& key_name) {
  return unknown_encryption_keys_by_name_.contains(key_name) &&
         unknown_encryption_keys_by_name_.at(key_name)
                 .get_updates_while_should_have_been_known >=
             kMinGuResponsesToIgnoreKey;
}

void DataTypeWorker::MaybeDropPendingUpdatesEncryptedWith(
    const std::string& key_name) {
  if (!ShouldIgnoreUpdatesEncryptedWith(key_name)) {
    return;
  }

  size_t updates_before_dropping = entries_pending_decryption_.size();
  std::erase_if(entries_pending_decryption_, [&](const auto& id_and_update) {
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
                      DataTypeToHistogramSuffix(type_)}),
        dropped_updates);
  }
}

void DataTypeWorker::RemoveKeysNoLongerUnknown() {
  std::set<std::string> keys_blocking_updates;
  for (const auto& [id, update] : entries_pending_decryption_) {
    const std::string key_name = GetEncryptionKeyName(update);
    DCHECK(!key_name.empty());
    keys_blocking_updates.insert(key_name);
  }

  std::erase_if(unknown_encryption_keys_by_name_,
                [&](const auto& key_and_info) {
                  return !keys_blocking_updates.contains(key_and_info.first);
                });
}

bool DataTypeWorker::HasNonDeletionUpdates() const {
  for (const UpdateResponseData& update : pending_updates_) {
    if (!update.entity.is_deleted()) {
      return true;
    }
  }
  return false;
}

void DataTypeWorker::ExtractGcDirective() {
  DCHECK(data_type_state_.has_progress_marker());
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
  // TODO(crbug.com/40860698): consider a better approach instead of this
  // workaround.

  if (data_type_state_.progress_marker().has_gc_directive()) {
    // Keep a new GC directive if received.
    // TODO(b/325917757): cover the case when a collaboration was removed and
    // then added in the next GetUpdates request again. All the previous
    // entities should be removed from the tracker (it's expected that the
    // server returns all the entities anyway and some entities could be removed
    // in the meantime).
    pending_gc_directive_ = data_type_state_.progress_marker().gc_directive();
    data_type_state_.mutable_progress_marker()->clear_gc_directive();
    return;
  }

  // Note that normally if the server returns non-empty updates for a
  // download-only data type, it returns a non-empty |gc_directive| as well.
  // However, it's safer to keep the GC directive until it's applied even if the
  // server returns non-empty updates without GC directive within the same sync
  // cycle.
}

void DataTypeWorker::RecordRemoteInvalidation(
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
  SendPendingInvalidationsToProcessor();
}

void DataTypeWorker::CollectPendingInvalidations(
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

bool DataTypeWorker::HasPendingInvalidations() const {
  return !pending_invalidations_.empty() || has_dropped_invalidation_;
}

void DataTypeWorker::SendPendingInvalidationsToProcessor() {
  CHECK(data_type_processor_);
  DVLOG(1) << "Storing pending invalidations for "
           << DataTypeToDebugString(type_);
  UpdateDataTypeStateInvalidations();
  data_type_processor_->StorePendingInvalidations(
      std::vector<sync_pb::DataTypeState::Invalidation>(
          data_type_state_.invalidations().begin(),
          data_type_state_.invalidations().end()));
}

void DataTypeWorker::UpdateDataTypeStateInvalidations() {
  data_type_state_.clear_invalidations();
  for (const auto& inv : pending_invalidations_) {
    SyncInvalidation* invalidation = inv.pending_invalidation.get();
    sync_pb::DataTypeState_Invalidation* invalidation_to_store =
        data_type_state_.add_invalidations();
    invalidation_to_store->set_hint(invalidation->GetPayload());
    if (!invalidation->IsUnknownVersion()) {
      invalidation_to_store->set_version(invalidation->GetVersion());
    }
  }
}

void DataTypeWorker::EncryptPasswordSpecificsData(
    CommitRequestDataList* request_data_list) {
  CHECK(cryptographer_);
  CHECK(encryption_enabled_);
  CHECK_EQ(type_, PASSWORDS);

  for (std::unique_ptr<CommitRequestData>& request_data : *request_data_list) {
    EntityData* entity_data = request_data->entity.get();
    if (entity_data->is_deleted()) {
      continue;
    }

    const sync_pb::PasswordSpecifics& password_specifics =
        entity_data->specifics.password();
    const sync_pb::PasswordSpecificsData& password_data =
        password_specifics.client_only_encrypted_data();
    sync_pb::EntitySpecifics encrypted_password;

    // Keep the unencrypted metadata for non-custom passphrase users.
    if (!IsExplicitPassphrase(passphrase_type_)) {
      *encrypted_password.mutable_password()->mutable_unencrypted_metadata() =
          password_specifics.unencrypted_metadata();
    }

    bool result = cryptographer_->Encrypt(
        password_data,
        encrypted_password.mutable_password()->mutable_encrypted());
    LogEncryptionResult(type_, result);

    // `encrypted_notes_backup` field needs to be populated regardless of
    // whether or not there are any notes.
    result = cryptographer_->Encrypt(password_data.notes(),
                                     encrypted_password.mutable_password()
                                         ->mutable_encrypted_notes_backup());
    CHECK(result);

    // When encrypting both blobs succeeds, both encrypted blobs must use the
    // key name.
    CHECK_EQ(encrypted_password.password().encrypted().key_name(),
             encrypted_password.password().encrypted_notes_backup().key_name());

    // Replace the entire specifics, among other things to ensure that any
    // client-only fields are cleared.
    entity_data->specifics = std::move(encrypted_password);
    entity_data->name = "encrypted";
  }
}

void DataTypeWorker::EncryptOutgoingPasswordSharingInvitations(
    CommitRequestDataList* request_data_list) {
  CHECK(cryptographer_);
  CHECK_EQ(type_, OUTGOING_PASSWORD_SHARING_INVITATION);

  for (std::unique_ptr<CommitRequestData>& request_data : *request_data_list) {
    EntityData* entity_data = request_data->entity.get();
    sync_pb::OutgoingPasswordSharingInvitationSpecifics* specifics =
        entity_data->specifics.mutable_outgoing_password_sharing_invitation();

    CHECK(specifics->has_client_only_unencrypted_data());
    std::string serialized_password_data;
    bool success = specifics->client_only_unencrypted_data().SerializeToString(
        &serialized_password_data);
    specifics->clear_client_only_unencrypted_data();
    CHECK(success);

    std::optional<std::vector<uint8_t>> encrypted_data =
        cryptographer_->AuthEncryptForCrossUserSharing(
            base::as_bytes(base::make_span(serialized_password_data)),
            base::as_bytes(base::make_span(
                entity_data->recipient_public_key.x25519_public_key())));
    // There should not be encryption failure but DCHECK is not used because
    // it's not guaranteed. In the worst case, the entity will be committed with
    // empty specifics (no unencrypted data will be committed to the server).
    LogEncryptionResult(type_, encrypted_data.has_value());
    if (encrypted_data) {
      specifics->set_encrypted_password_sharing_invitation_data(
          encrypted_data->data(), encrypted_data->size());
      specifics->set_recipient_key_version(
          entity_data->recipient_public_key.version());
    } else {
      DLOG(ERROR) << "Failed to encrypt outgoing password sharing invitation";
    }
  }
}

void DataTypeWorker::EncryptSpecifics(
    CommitRequestDataList* request_data_list) {
  CHECK(cryptographer_);
  CHECK(encryption_enabled_);
  CHECK_NE(type_, PASSWORDS);
  CHECK_NE(type_, OUTGOING_PASSWORD_SHARING_INVITATION);

  for (std::unique_ptr<CommitRequestData>& request_data : *request_data_list) {
    EntityData* entity_data = request_data->entity.get();
    entity_data->name = "encrypted";
    if (entity_data->is_deleted()) {
      // EntityData::is_deleted() means that the specifics is empty, so nothing
      // to encrypt.
      continue;
    }
    sync_pb::EntitySpecifics encrypted_specifics;
    bool success = cryptographer_->Encrypt(
        entity_data->specifics, encrypted_specifics.mutable_encrypted());
    LogEncryptionResult(type_, success);
    entity_data->specifics.CopyFrom(encrypted_specifics);
  }
}

GetLocalChangesRequest::GetLocalChangesRequest()
    : response_accepted_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED) {}

GetLocalChangesRequest::~GetLocalChangesRequest() = default;

void GetLocalChangesRequest::OnCancelationSignalReceived() {
  response_accepted_.Signal();
}

void GetLocalChangesRequest::WaitForResponseOrCancelation(
    CancelationSignal* cancelation_signal) {
  if (!cancelation_signal->TryRegisterHandler(this)) {
    return;
  }

  {
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    response_accepted_.Wait();
  }

  cancelation_signal->UnregisterHandler(this);
}

void GetLocalChangesRequest::SetResponse(
    CommitRequestDataList&& local_changes) {
  response_ = std::move(local_changes);
  response_accepted_.Signal();
}

CommitRequestDataList&& GetLocalChangesRequest::ExtractResponse() {
  return std::move(response_);
}

}  // namespace syncer
