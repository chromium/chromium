// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_DATA_TYPE_WORKER_H_
#define COMPONENTS_SYNC_ENGINE_DATA_TYPE_WORKER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/commit_contributor.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/nigori/cryptographer.h"
#include "components/sync/engine/nudge_handler.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/update_handler.h"
#include "components/sync/protocol/data_type_state.pb.h"

namespace sync_pb {
class DataTypeContext;
class DataTypeProgressMarker;
class DataTypeState;
class GarbageCollectionDirective;
class GetUpdateTriggers;
class SyncEntity;
}  // namespace sync_pb

namespace syncer {

class CancelationSignal;
class DataTypeProcessor;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SyncPasswordNotesStateInUpdate)
enum class PasswordNotesStateForUMA {
  // No password note is set in the proto or the backup.
  kUnset = 0,
  // Password note is set in the password specifics data. Indicates an entity
  // created on a client supporting password notes. This doesn't guarantee the
  // backup is set too, but in practice it will be set in both (in the
  // foreseeable future).
  kSetInSpecificsData = 1,
  // Password notes is set in the backup, indicates that after the note was
  // created on the client, and update from a legacy client was committed and
  // the server carried the backup blob over.
  kSetOnlyInBackup = 2,
  // Similar to kSetOnlyInBackup, but the backup is corrupted. This should be
  // rare
  // to happen.
  kSetOnlyInBackupButCorrupted = 3,
  kMaxValue = kSetOnlyInBackupButCorrupted,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncPasswordNotesStateInUpdate)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PendingInvalidationStatus)
enum class PendingInvalidationStatus {
  kAcknowledged = 0,
  kLost = 1,
  kInvalidationsOverflow = 2,
  // kSameVersion = 3,
  // Invalidation list already has another invalidation with the same version.
  kSameKnownVersion = 4,
  kSameUnknownVersion = 5,
  kDataTypeNotConnected = 6,
  kMaxValue = kDataTypeNotConnected,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:PendingInvalidationStatus)

// A smart cache for sync types to communicate with the sync sequence.
//
// When the sync data type wants to talk to the sync server, it will
// send a message from its sequence to this object on the sync sequence. This
// object ensures the appropriate sync server communication gets scheduled and
// executed. The response, if any, will be returned to the data types's sequence
// eventually.
//
// This object also has a role to play in communications in the opposite
// direction. Sometimes the sync sequence will receive changes from the sync
// server and deliver them here. This object will post this information back to
// the appropriate component on the data type's sequence.
//
// This object does more than just pass along messages. It understands the sync
// protocol, and it can make decisions when it sees conflicting messages. For
// example, if the sync server sends down an update for a sync entity that is
// currently pending for commit, this object will detect this condition and
// cancel the pending commit.
//
// Lives on the sync sequence.
class DataTypeWorker : public UpdateHandler,
                       public CommitContributor,
                       public CommitQueue {
 public:
  // Public for testing.
  enum DecryptionStatus { SUCCESS, DECRYPTION_PENDING, FAILED_TO_DECRYPT };

  // This enum reflects the processor's state of having local changes.
  enum HasLocalChangesState {
    // There are no new nudged pending changes in the processor.
    kNoNudgedLocalChanges,

    // There are new pending changes in the processor which are not committed
    // yet.
    kNewlyNudgedLocalChanges,

    // All known local changes are contributed in the last commit request (and
    // there is no commit response yet).
    kAllNudgedLocalChangesInFlight,
  };

  // |cryptographer|, |nudge_handler| and |cancelation_signal| must be non-null
  // and outlive the worker. Calling this will construct the object but not
  // more, ConnectSync() must be called immediately afterwards.
  DataTypeWorker(DataType type,
                 const sync_pb::DataTypeState& initial_state,
                 Cryptographer* cryptographer,
                 bool encryption_enabled,
                 PassphraseType passphrase_type,
                 NudgeHandler* nudge_handler,
                 CancelationSignal* cancelation_signal);

  DataTypeWorker(const DataTypeWorker&) = delete;
  DataTypeWorker& operator=(const DataTypeWorker&) = delete;

  ~DataTypeWorker() override;

  // Public for testing.
  // |response_data| must be not null.
  static DecryptionStatus PopulateUpdateResponseData(
      const Cryptographer& cryptographer,
      DataType data_type,
      const sync_pb::SyncEntity& update_entity,
      UpdateResponseData* response_data);

  static void LogPendingInvalidationStatus(PendingInvalidationStatus status);

  // Initializes the two relevant communication channels: DataTypeWorker ->
  // DataTypeProcessor (GetUpdates) and DataTypeProcessor -> DataTypeWorker
  // (Commit). Both channels are closed when the worker is destroyed. This is
  // done outside of the constructor to avoid the object being used while it's
  // still being built.
  // Must be called immediately after the constructor, prior to using other
  // methods.
  void ConnectSync(std::unique_ptr<DataTypeProcessor> data_type_processor);

  DataType GetDataType() const;

  // Makes this an encrypted type, which means:
  // a) Commits will be encrypted using the cryptographer passed on
  // construction. Note that updates are always decrypted if possible,
  // regardless of this method.
  // b) The worker can only commit or push updates once the cryptographer has
  // selected a default key to encrypt data (Cryptographer::CanEncrypt()). That
  // used key will be listed in DataTypeState.
  // This is a no-op if encryption was already enabled on construction or by
  // a previous call to this method.
  void EnableEncryption();

  // Must be called on every change to the state of the cryptographer passed on
  // construction.
  void OnCryptographerChange();

  void UpdatePassphraseType(PassphraseType type);

  // UpdateHandler implementation.
  bool IsInitialSyncEnded() const override;
  const sync_pb::DataTypeProgressMarker& GetDownloadProgress() const override;
  const sync_pb::DataTypeContext& GetDataTypeContext() const override;
  void ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const sync_pb::DataTypeContext& mutated_context,
      const SyncEntityList& applicable_updates,
      StatusController* status) override;
  void ApplyUpdates(StatusController* status, bool cycle_done) override;
  void RecordRemoteInvalidation(
      std::unique_ptr<SyncInvalidation> incoming) override;
  void CollectPendingInvalidations(sync_pb::GetUpdateTriggers* msg) override;
  bool HasPendingInvalidations() const override;

  // CommitQueue implementation.
  void NudgeForCommit() override;

  // CommitContributor implementation.
  std::unique_ptr<CommitContribution> GetContribution(
      size_t max_entries) override;

  // Public for testing.
  // Returns true if this type should stop communicating because of outstanding
  // encryption issues and must wait for keys to be updated.
  bool BlockForEncryption() const;

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  bool HasLocalChanges() const;

  bool IsEncryptionEnabledForTest() const { return encryption_enabled_; }

  static constexpr size_t kMaxPendingInvalidations = 10u;

 private:
  struct UnknownEncryptionKeyInfo {
    // Not increased if the cryptographer knows it's in a pending state
    // (cf. Cryptographer::CanEncrypt()).
    int get_updates_while_should_have_been_known = 0;
  };
  struct PendingInvalidation {
    PendingInvalidation();
    PendingInvalidation(const PendingInvalidation&) = delete;
    PendingInvalidation& operator=(const PendingInvalidation&) = delete;
    PendingInvalidation(PendingInvalidation&&);
    PendingInvalidation& operator=(PendingInvalidation&&);
    PendingInvalidation(std::unique_ptr<SyncInvalidation> invalidation,
                        bool is_processed);
    ~PendingInvalidation();

    std::unique_ptr<SyncInvalidation> pending_invalidation;
    // |is_processed| is true, if the invalidation included to GetUpdates
    // trigger message.
    bool is_processed = false;
  };

  // Sends |pending_updates_| and |data_type_state_| to the processor if there
  // are no encryption pendencies and initial sync is done. This is called in
  // ApplyUpdates() during a GetUpdates cycle, but also if the processor must be
  // informed of a new encryption key, or the worker just managed to decrypt
  // some pending updates.
  // If initial sync isn't done yet, the first ApplyUpdates() will take care of
  // pushing the data in such cases instead (the processor relies on this).
  void SendPendingUpdatesToProcessorIfReady();

  // Returns true if this type is prepared to commit items. Currently, this
  // depends on having downloaded the initial data and having the encryption
  // settings in a good state.
  bool CanCommitItems() const;

  // If |encryption_enabled_| is false, sets the encryption key name in
  // |data_type_state_| to the empty string. This should usually be a no-op.
  // If |encryption_enabled_| is true *and* the cryptographer has selected a
  // (non-empty) default key, sets the value to that default key.
  // Returns whether the |data_type_state_| key name changed.
  bool UpdateTypeEncryptionKeyName();

  // Iterates through all elements in |entries_pending_decryption_| and tries to
  // decrypt anything that has encrypted data.
  // Should only be called during a GetUpdates cycle.
  void DecryptStoredEntities();

  // Nudges nudge_handler_ when initial sync is done, processor has local
  // changes and either encryption is disabled for the type or cryptographer is
  // ready (doesn't have pending keys).
  void NudgeIfReadyToCommit();

  // Filters our duplicate updates from |pending_updates_| based on the server
  // id. It discards all of them except the last one.
  void DeduplicatePendingUpdatesBasedOnServerId();

  // Filters our duplicate updates from |pending_updates_| based on the client
  // tag hash. It discards all of them except the last one.
  void DeduplicatePendingUpdatesBasedOnClientTagHash();

  // Filters our duplicate updates from |pending_updates_| based on the
  // originator item ID (in practice used for bookmarks only). It discards all
  // of them except the last one.
  void DeduplicatePendingUpdatesBasedOnOriginatorClientItemId();

  // Callback for when our contribution gets a response.
  void OnCommitResponse(
      const CommitResponseDataList& committed_response_list,
      const FailedCommitResponseDataList& error_response_list);

  // Callback when there is no response or server returns an error without
  // response body.
  void OnFullCommitFailure(SyncCommitError commit_error);

  // Returns true for keys that have remained unknown for so long that they are
  // not expected to arrive anytime soon. The worker ignores incoming updates
  // encrypted with them, and drops pending ones on the next GetUpdates.
  // Those keys remain in |unknown_encryption_keys_by_name_|.
  bool ShouldIgnoreUpdatesEncryptedWith(const std::string& key_name);

  // If |key_name| should be ignored (cf. ShouldIgnoreUpdatesEncryptedWith()),
  // drops elements of |entries_pending_decryption_| encrypted with it.
  void MaybeDropPendingUpdatesEncryptedWith(const std::string& key_name);

  // Removes elements of |unknown_encryption_keys_by_name_| that no longer fit
  // the definition of an unknown key.
  void RemoveKeysNoLongerUnknown();

  // Sends copy of |pending_invalidations_| vector to |data_type_processor_|
  // to store them in storage along |data_type_state_|.
  void SendPendingInvalidationsToProcessor();

  // Copies |pending_invalidations_| vector to |data_type_state_|.
  void UpdateDataTypeStateInvalidations();

  // Encrypts the specifics and hides the title if necessary.
  void EncryptPasswordSpecificsData(CommitRequestDataList* request_data_list);

  // Encrypts password sharing invitation using cross user sharing encryption.
  void EncryptOutgoingPasswordSharingInvitations(
      CommitRequestDataList* request_data_list);

  // Encrypts the specifics, must be called only when encryption is enabled.
  // Note that Passwords and OutgoingPasswordSharingInvitations have their own
  // encryption scheme.
  void EncryptSpecifics(CommitRequestDataList* request_data_list);

  // The (up to kMaxPayloads) most recent invalidations received since the last
  // successful sync cycle.
  std::vector<PendingInvalidation> pending_invalidations_;

  // Whether any invalidations were dropped due to overflow since the last
  // GetUpdates cycle.
  bool has_dropped_invalidation_ = false;

  // Returns whether |pending_updates_| contain any non-deletion update.
  bool HasNonDeletionUpdates() const;

  // Extraxts GC directive from the progress marker to handle it independently
  // of |data_type_state_|.
  void ExtractGcDirective();

  const DataType type_;

  const raw_ptr<Cryptographer> cryptographer_;

  // Interface used to access and send nudges to the sync scheduler. Not owned.
  const raw_ptr<NudgeHandler> nudge_handler_;

  // Cancellation signal is used to cancel blocking operation on engine
  // shutdown.
  const raw_ptr<CancelationSignal> cancelation_signal_;

  // Pointer to the DataTypeProcessor associated with this worker. Initialized
  // with ConnectSync().
  // Note that in practice, this is typically a proxy object to the actual
  // processor (which lives on the model sequence).
  std::unique_ptr<DataTypeProcessor> data_type_processor_;

  // State that applies to the entire data type.
  sync_pb::DataTypeState data_type_state_;

  bool encryption_enabled_;

  // A private copy of the most recent passphrase type. Initialized at
  // construction time and updated with UpdatePassphraseType().
  PassphraseType passphrase_type_;

  // A map of sync entities, keyed by server_id. Holds updates encrypted with
  // pending keys. Entries are stored in a map for de-duplication (applying only
  // the latest).
  // TODO(crbug.com/40141634): Use a name mentioning "updates" and "server id".
  std::map<std::string, sync_pb::SyncEntity> entries_pending_decryption_;

  // A key is said to be unknown if one of these is true:
  // a) It encrypts some updates(s) in |entries_pending_decryption_|.
  // b) (a) happened for so long that the worker dropped those updates in an
  // attempt to unblock itself (cf. ShouldIgnoreUpdatesEncryptedWith()).
  // The key is added here when the worker receives the first update entity
  // encrypted with it.
  std::map<std::string, UnknownEncryptionKeyInfo>
      unknown_encryption_keys_by_name_;

  // Accumulates all the updates from a single GetUpdates cycle in memory so
  // they can all be sent to the processor at once. Some updates may be
  // deduplicated, e.g. in DeduplicatePendingUpdatesBasedOnServerId(). The
  // ordering here is NOT guaranteed to stick to the download ordering or any
  // other.
  UpdateResponseDataList pending_updates_;

  // Pending GC directive if received during the current sync cycle. If there
  // are several pending GC directives, the latest one will be stored.
  std::optional<sync_pb::GarbageCollectionDirective> pending_gc_directive_;

  // Indicates if processor has local changes. Processor only nudges worker once
  // and worker might not be ready to commit entities at the time.
  HasLocalChangesState has_local_changes_state_ = kNoNudgedLocalChanges;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DataTypeWorker> weak_ptr_factory_{this};
};

// GetLocalChangesRequest is a container for GetLocalChanges call response. It
// allows sync sequence to block waiting for model sequence to call SetResponse.
// This class supports canceling blocking call through CancelationSignal during
// sync engine shutdown.
//
// It should be used in the following manner:
// scoped_refptr<GetLocalChangesRequest> request =
//     base::MakeRefCounted<GetLocalChangesRequest>(cancelation_signal_);
// data_type_processor_->GetLocalChanges(
//     max_entries,
//     base::BindOnce(&GetLocalChangesRequest::SetResponse, request));
// request->WaitForResponseOrCancelation();
// CommitRequestDataList response;
// if (!request->WasCancelled())
//   response = request->ExtractResponse();
class GetLocalChangesRequest
    : public base::RefCountedThreadSafe<GetLocalChangesRequest>,
      public CancelationSignal::Observer {
 public:
  GetLocalChangesRequest();

  GetLocalChangesRequest(const GetLocalChangesRequest&) = delete;
  GetLocalChangesRequest& operator=(const GetLocalChangesRequest&) = delete;

  // CancelationSignal::Observer implementation.
  void OnCancelationSignalReceived() override;

  // Blocks current sequence until either SetResponse is called or
  // cancelation_signal_ is signaled.
  void WaitForResponseOrCancelation(CancelationSignal* cancelation_signal);

  // SetResponse takes ownership of |local_changes| and unblocks
  // WaitForResponseOrCancelation call. It is called by data type through
  // callback passed to GetLocalChanges.
  void SetResponse(CommitRequestDataList&& local_changes);

  // Returns response set by SetResponse().
  CommitRequestDataList&& ExtractResponse();

 private:
  friend class base::RefCountedThreadSafe<GetLocalChangesRequest>;
  ~GetLocalChangesRequest() override;

  base::WaitableEvent response_accepted_;
  CommitRequestDataList response_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_DATA_TYPE_WORKER_H_
