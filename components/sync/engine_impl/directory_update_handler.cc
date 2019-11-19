// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/directory_update_handler.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/engine_impl/conflict_resolver.h"
#include "components/sync/engine_impl/cycle/data_type_debug_info_emitter.h"
#include "components/sync/engine_impl/cycle/status_controller.h"
#include "components/sync/engine_impl/update_applicator.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/directory_cryptographer.h"
#include "components/sync/syncable/model_neutral_mutable_entry.h"
#include "components/sync/syncable/syncable_changes_version.h"
#include "components/sync/syncable/syncable_model_neutral_write_transaction.h"
#include "components/sync/syncable/syncable_write_transaction.h"

namespace syncer {

using syncable::SYNCER;

DirectoryUpdateHandler::DirectoryUpdateHandler(
    syncable::Directory* dir,
    ModelType type,
    scoped_refptr<ModelSafeWorker> worker,
    DataTypeDebugInfoEmitter* debug_info_emitter)
    : dir_(dir),
      type_(type),
      worker_(worker),
      debug_info_emitter_(debug_info_emitter),
      cached_gc_directive_version_(0),
      cached_gc_directive_aged_out_day_(base::Time::FromDoubleT(0)) {}

DirectoryUpdateHandler::~DirectoryUpdateHandler() {}

bool DirectoryUpdateHandler::IsInitialSyncEnded() const {
  return dir_->InitialSyncEndedForType(type_);
}

void DirectoryUpdateHandler::GetDownloadProgress(
    sync_pb::DataTypeProgressMarker* progress_marker) const {
  dir_->GetDownloadProgress(type_, progress_marker);
}

void DirectoryUpdateHandler::GetDataTypeContext(
    sync_pb::DataTypeContext* context) const {
  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir_);
  dir_->GetDataTypeContext(&trans, type_, context);
}

SyncerError DirectoryUpdateHandler::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const sync_pb::DataTypeContext& mutated_context,
    const SyncEntityList& applicable_updates,
    StatusController* status) {
  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir_);
  if (mutated_context.has_context()) {
    sync_pb::DataTypeContext local_context;
    dir_->GetDataTypeContext(&trans, type_, &local_context);

    // Only update the local context if it is still relevant. If the local
    // version is higher, it means a local change happened while the mutation
    // was in flight, and the local context takes priority.
    if (mutated_context.version() >= local_context.version() &&
        local_context.context() != mutated_context.context()) {
      dir_->SetDataTypeContext(&trans, type_, mutated_context);
      // TODO(zea): trigger the datatype's UpdateDataTypeContext method.
    } else if (mutated_context.version() < local_context.version()) {
      // A GetUpdates using the old context was in progress when the context was
      // set. Fail this get updates cycle, to force a retry.
      DVLOG(1) << "GU Context conflict detected, forcing GU retry.";
      debug_info_emitter_->EmitUpdateCountersUpdate();
      return SyncerError(SyncerError::DATATYPE_TRIGGERED_RETRY);
    }
  }

  // Auto-create permanent folder for the type if the progress marker
  // changes from empty to non-empty.
  if (IsTypeWithClientGeneratedRoot(type_) &&
      dir_->HasEmptyDownloadProgress(type_) &&
      IsValidProgressMarker(progress_marker)) {
    CreateTypeRoot(&trans);
  }

  UpdateSyncEntities(
      &trans, applicable_updates,
      /*is_initial_sync=*/!dir_->InitialSyncEndedForType(&trans, type_),
      status);

  if (IsValidProgressMarker(progress_marker)) {
    ExpireEntriesIfNeeded(&trans, progress_marker);
    UpdateProgressMarker(progress_marker);
  }

  debug_info_emitter_->EmitUpdateCountersUpdate();
  return SyncerError(SyncerError::SYNCER_OK);
}

void DirectoryUpdateHandler::CreateTypeRoot(
    syncable::ModelNeutralWriteTransaction* trans) {
  syncable::ModelNeutralMutableEntry entry(
      trans, syncable::CREATE_NEW_TYPE_ROOT, type_);
  if (!entry.good()) {
    // This will fail only if matching entry already exists, for example
    // if the type gets disabled and its progress marker gets cleared,
    // then the type gets re-enabled again.
    DVLOG(1) << "Type root folder " << ModelTypeToRootTag(type_)
             << " already exists.";
    return;
  }

  entry.PutServerIsDir(true);
  entry.PutUniqueServerTag(ModelTypeToRootTag(type_));
}

void DirectoryUpdateHandler::ApplyUpdates(StatusController* status) {
  if (IsApplyUpdatesRequired()) {
    // This will invoke handlers that belong to the model and its thread, so we
    // switch to the appropriate thread before we start this work.
    WorkCallback c = base::BindOnce(
        &DirectoryUpdateHandler::ApplyUpdatesImpl,
        // We wait until the callback is executed.  We can safely use
        // Unretained.
        base::Unretained(this), base::Unretained(status));
    worker_->DoWorkAndWaitUntilDone(std::move(c));

    debug_info_emitter_->EmitUpdateCountersUpdate();
    debug_info_emitter_->EmitStatusCountersUpdate();
  }

  PostApplyUpdates();
}

void DirectoryUpdateHandler::PassiveApplyUpdates(StatusController* status) {
  if (IsApplyUpdatesRequired()) {
    // Just do the work here instead of deferring to another thread.
    ApplyUpdatesImpl(status);

    debug_info_emitter_->EmitUpdateCountersUpdate();
    debug_info_emitter_->EmitStatusCountersUpdate();
  }

  PostApplyUpdates();
}

SyncerError DirectoryUpdateHandler::ApplyUpdatesImpl(StatusController* status) {
  syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER, dir_);

  std::vector<int64_t> handles;
  dir_->GetUnappliedUpdateMetaHandles(&trans, FullModelTypeSet(type_),
                                      &handles);

  // First set of update application passes.
  UpdateApplicator applicator(dir_->GetCryptographer(&trans));
  applicator.AttemptApplications(&trans, handles);

  // The old StatusController counters.
  status->increment_num_updates_applied_by(applicator.updates_applied());
  status->increment_num_hierarchy_conflicts_by(
      applicator.hierarchy_conflicts());
  status->increment_num_encryption_conflicts_by(
      applicator.encryption_conflicts());

  // The new UpdateCounter counters.
  UpdateCounters* counters = debug_info_emitter_->GetMutableUpdateCounters();
  counters->num_updates_applied += applicator.updates_applied();
  counters->num_hierarchy_conflict_application_failures =
      applicator.hierarchy_conflicts();
  counters->num_encryption_conflict_application_failures +=
      applicator.encryption_conflicts();

  if (applicator.simple_conflict_ids().size() != 0) {
    // Resolve the simple conflicts we just detected.
    ConflictResolver resolver;
    resolver.ResolveConflicts(&trans, dir_->GetCryptographer(&trans),
                              applicator.simple_conflict_ids(), status,
                              counters);

    // Conflict resolution sometimes results in more updates to apply.
    handles.clear();
    dir_->GetUnappliedUpdateMetaHandles(&trans, FullModelTypeSet(type_),
                                        &handles);

    UpdateApplicator conflict_applicator(dir_->GetCryptographer(&trans));
    conflict_applicator.AttemptApplications(&trans, handles);

    // We count the number of updates from both applicator passes.
    status->increment_num_updates_applied_by(
        conflict_applicator.updates_applied());
    counters->num_updates_applied += conflict_applicator.updates_applied();

    // Encryption conflicts should remain unchanged by the resolution of simple
    // conflicts.  Those can only be solved by updating our nigori key bag.
    DCHECK_EQ(conflict_applicator.encryption_conflicts(),
              applicator.encryption_conflicts());

    // Hierarchy conflicts should also remain unchanged, for reasons that are
    // more subtle.  Hierarchy conflicts exist when the application of a pending
    // update from the server would make the local folder hierarchy
    // inconsistent.  The resolution of simple conflicts could never affect the
    // hierarchy conflicting item directly, because hierarchy conflicts are not
    // processed by the conflict resolver.  It could, in theory, modify the
    // local hierarchy on which hierarchy conflict detection depends.  However,
    // the conflict resolution algorithm currently in use does not allow this.
    DCHECK_EQ(conflict_applicator.hierarchy_conflicts(),
              applicator.hierarchy_conflicts());

    // There should be no simple conflicts remaining.  We know this because the
    // resolver should have resolved all the conflicts we detected last time
    // and, by the two previous assertions, that no conflicts have been
    // downgraded from encryption or hierarchy down to simple.
    DCHECK(conflict_applicator.simple_conflict_ids().empty());
  }

  return SyncerError(SyncerError::SYNCER_OK);
}

void DirectoryUpdateHandler::PostApplyUpdates() {
  // If this is a type with client generated root, the root node has been
  // created locally and didn't go through ApplyUpdatesImpl.
  // Mark it as having the initial download completed so that the type
  // reports as properly initialized (which is done by changing the root's
  // base version to a value other than CHANGES_VERSION).
  // This does nothing if the root's base version is already other than
  // CHANGES_VERSION.
  if (IsTypeWithClientGeneratedRoot(type_)) {
    syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir_);
    dir_->MarkInitialSyncEndedForType(&trans, type_);
  }
}

bool DirectoryUpdateHandler::IsApplyUpdatesRequired() {
  if (IsControlType(type_)) {
    return false;  // We don't process control types here.
  }

  return dir_->TypeHasUnappliedUpdates(type_);
}

void DirectoryUpdateHandler::UpdateSyncEntities(
    syncable::ModelNeutralWriteTransaction* trans,
    const SyncEntityList& applicable_updates,
    bool is_initial_sync,
    StatusController* status) {
  UpdateCounters* counters = debug_info_emitter_->GetMutableUpdateCounters();
  if (is_initial_sync) {
    counters->num_initial_updates_received += applicable_updates.size();
  } else {
    counters->num_non_initial_updates_received += applicable_updates.size();
  }
  ProcessDownloadedUpdates(dir_, trans, type_, applicable_updates,
                           is_initial_sync, status, counters);
}

bool DirectoryUpdateHandler::IsValidProgressMarker(
    const sync_pb::DataTypeProgressMarker& progress_marker) const {
  if (progress_marker.token().empty()) {
    return false;
  }
  int field_number = progress_marker.data_type_id();
  ModelType model_type = GetModelTypeFromSpecificsFieldNumber(field_number);
  if (!IsRealDataType(model_type) || type_ != model_type) {
    NOTREACHED() << "Update handler of type " << ModelTypeToString(type_)
                 << " asked to process progress marker with invalid type "
                 << field_number;
    return false;
  }
  return true;
}

void DirectoryUpdateHandler::UpdateProgressMarker(
    const sync_pb::DataTypeProgressMarker& progress_marker) {
  dir_->SetDownloadProgress(type_, progress_marker);
}

void DirectoryUpdateHandler::ExpireEntriesIfNeeded(
    syncable::ModelNeutralWriteTransaction* trans,
    const sync_pb::DataTypeProgressMarker& progress_marker) {
  if (!progress_marker.has_gc_directive())
    return;

  const sync_pb::GarbageCollectionDirective& new_gc_directive =
      progress_marker.gc_directive();

  if (new_gc_directive.has_version_watermark() &&
      (cached_gc_directive_version_ < new_gc_directive.version_watermark())) {
    ExpireEntriesByVersion(dir_, trans, type_,
                           new_gc_directive.version_watermark());
    cached_gc_directive_version_ = new_gc_directive.version_watermark();
  }

  if (new_gc_directive.has_age_watermark_in_days()) {
    DCHECK(new_gc_directive.age_watermark_in_days());
    // For saving resource purpose(ex. cpu, battery), We round up garbage
    // collection age to day, so we only run GC once a day if server did not
    // change the |age_watermark_in_days|.
    base::Time to_be_expired =
        base::Time::Now().LocalMidnight() -
        base::TimeDelta::FromDays(new_gc_directive.age_watermark_in_days());
    if (cached_gc_directive_aged_out_day_ != to_be_expired) {
      ExpireEntriesByAge(dir_, trans, type_,
                         new_gc_directive.age_watermark_in_days());
      cached_gc_directive_aged_out_day_ = to_be_expired;
    }
  }
}

}  // namespace syncer
