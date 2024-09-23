// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/syncer.h"

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/active_devices_invalidation_info.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/commit.h"
#include "components/sync/engine/commit_processor.h"
#include "components/sync/engine/cycle/nudge_tracker.h"
#include "components/sync/engine/cycle/sync_cycle.h"
#include "components/sync/engine/get_updates_delegate.h"
#include "components/sync/engine/get_updates_processor.h"
#include "components/sync/engine/sync_protocol_error.h"
#include "components/sync/engine/syncer_error.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "net/http/http_status_code.h"

namespace syncer {

namespace {

SyncerErrorValueForUma GetSyncerErrorValueForUma(
    SyncProtocolErrorType protocol_error) {
  switch (protocol_error) {
    case SYNC_SUCCESS:
      NOTREACHED_IN_MIGRATION();
      return SyncerErrorValueForUma::kSyncerOk;
    case NOT_MY_BIRTHDAY:
      return SyncerErrorValueForUma::kServerReturnNotMyBirthday;
    case THROTTLED:
      return SyncerErrorValueForUma::kServerReturnThrottled;
    case TRANSIENT_ERROR:
      return SyncerErrorValueForUma::kServerReturnTransientError;
    case MIGRATION_DONE:
      return SyncerErrorValueForUma::kServerReturnMigrationDone;
    case DISABLED_BY_ADMIN:
      return SyncerErrorValueForUma::kServerReturnDisabledByAdmin;
    case PARTIAL_FAILURE:
      NOTREACHED_IN_MIGRATION();
      return SyncerErrorValueForUma::kServerReturnUnknownError;
    case CLIENT_DATA_OBSOLETE:
      return SyncerErrorValueForUma::kServerReturnClientDataObsolete;
    case ENCRYPTION_OBSOLETE:
      return SyncerErrorValueForUma::kServerReturnClientDataObsolete;
    case UNKNOWN_ERROR:
      return SyncerErrorValueForUma::kServerReturnUnknownError;
    case CONFLICT:
      return SyncerErrorValueForUma::kServerReturnConflict;
    case INVALID_MESSAGE:
      return SyncerErrorValueForUma::kServerReturnUnknownError;
  }
  NOTREACHED();
}

SyncerErrorValueForUma GetSyncerErrorValueForUma(const SyncerError& error) {
  switch (error.type()) {
    case SyncerError::Type::kSuccess:
      return SyncerErrorValueForUma::kSyncerOk;
    case SyncerError::Type::kNetworkError:
      return SyncerErrorValueForUma::kNetworkConnectionUnavailable;
    case SyncerError::Type::kHttpError:
      if (error.GetHttpErrorOrDie() == net::HTTP_UNAUTHORIZED) {
        return SyncerErrorValueForUma::kSyncAuthError;
      }
      return SyncerErrorValueForUma::kSyncServerError;
    case SyncerError::Type::kProtocolError:
      return GetSyncerErrorValueForUma(error.GetProtocolErrorOrDie());
    case SyncerError::Type::kProtocolViolationError:
      return SyncerErrorValueForUma::kServerResponseValidationFailed;
  }
  NOTREACHED();
}

// Returns invalidation info after applying updates. This is used to drop
// optimization flags if DeviceInfo has been just updated (and new subscriptions
// might be just received). Without that if a new device with enabled
// invalidations has been just received, it may be updated only in the next
// sync cycle due to delay between threads.
ActiveDevicesInvalidationInfo GetInvalidationInfo(const SyncCycle* cycle) {
  if (cycle->status_controller().get_updated_types().Has(DEVICE_INFO) &&
      base::FeatureList::IsEnabled(
          kSkipInvalidationOptimizationsWhenDeviceInfoUpdated)) {
    return ActiveDevicesInvalidationInfo::CreateUninitialized();
  }
  return cycle->context()->active_devices_invalidation_info();
}

void HandleCycleBegin(SyncCycle* cycle) {
  cycle->mutable_status_controller()->UpdateStartTime();
  cycle->mutable_status_controller()->clear_updated_types();
  cycle->SendEventNotification(SyncCycleEvent::SYNC_CYCLE_BEGIN);
}

}  // namespace

Syncer::Syncer(CancelationSignal* cancelation_signal)
    : cancelation_signal_(cancelation_signal) {}

Syncer::~Syncer() = default;

bool Syncer::IsSyncing() const {
  return is_syncing_;
}

bool Syncer::NormalSyncShare(DataTypeSet request_types,
                             NudgeTracker* nudge_tracker,
                             SyncCycle* cycle) {
  base::AutoReset<bool> is_syncing(&is_syncing_, true);
  HandleCycleBegin(cycle);
  if (nudge_tracker->IsGetUpdatesRequired(request_types)) {
    VLOG(1) << "Downloading types " << DataTypeSetToDebugString(request_types);
    if (!DownloadAndApplyUpdates(&request_types, cycle,
                                 NormalGetUpdatesDelegate(*nudge_tracker))) {
      return HandleCycleEnd(cycle, nudge_tracker->GetOrigin());
    }
  }

  SyncerError commit_result =
      BuildAndPostCommits(request_types, nudge_tracker, cycle);
  cycle->mutable_status_controller()->set_commit_result(commit_result);

  return HandleCycleEnd(cycle, nudge_tracker->GetOrigin());
}

bool Syncer::ConfigureSyncShare(const DataTypeSet& request_types,
                                sync_pb::SyncEnums::GetUpdatesOrigin origin,
                                SyncCycle* cycle) {
  base::AutoReset<bool> is_syncing(&is_syncing_, true);

  // It is possible during configuration that datatypes get unregistered from
  // DataTypeRegistry before scheduled configure sync cycle gets executed.
  // This happens either because DataTypeController::LoadModels fail and type
  // need to be stopped or during shutdown when all datatypes are stopped. When
  // it happens we should adjust set of types to download to only include
  // registered types.
  DataTypeSet still_enabled_types =
      Intersection(request_types, cycle->context()->GetConnectedTypes());
  VLOG(1) << "Configuring types "
          << DataTypeSetToDebugString(still_enabled_types);
  HandleCycleBegin(cycle);
  DownloadAndApplyUpdates(&still_enabled_types, cycle,
                          ConfigureGetUpdatesDelegate(origin));
  return HandleCycleEnd(cycle, origin);
}

bool Syncer::PollSyncShare(DataTypeSet request_types, SyncCycle* cycle) {
  base::AutoReset<bool> is_syncing(&is_syncing_, true);
  VLOG(1) << "Polling types " << DataTypeSetToDebugString(request_types);
  HandleCycleBegin(cycle);
  DownloadAndApplyUpdates(&request_types, cycle, PollGetUpdatesDelegate());
  return HandleCycleEnd(cycle, sync_pb::SyncEnums::PERIODIC);
}

bool Syncer::DownloadAndApplyUpdates(DataTypeSet* request_types,
                                     SyncCycle* cycle,
                                     const GetUpdatesDelegate& delegate) {
  // CommitOnlyTypes() should not be included in the GetUpdates, but should be
  // included in the Commit. We are given a set of types for our SyncShare,
  // and we must do this filtering. Note that |request_types| is also an out
  // param, see below where we update it.
  DataTypeSet requested_commit_only_types =
      Intersection(*request_types, CommitOnlyTypes());
  DataTypeSet download_types =
      Difference(*request_types, requested_commit_only_types);
  GetUpdatesProcessor get_updates_processor(
      cycle->context()->data_type_registry()->update_handler_map(), delegate);
  SyncerError download_result = SyncerError::Success();
  do {
    download_result =
        get_updates_processor.DownloadUpdates(&download_types, cycle);
  } while (get_updates_processor.HasMoreUpdatesToDownload());

  // It is our responsibility to propagate the removal of types that occurred in
  // GetUpdatesProcessor::DownloadUpdates().
  *request_types = Union(download_types, requested_commit_only_types);

  // Exit without applying if we're shutting down or an error was detected.
  if (download_result.type() != SyncerError::Type::kSuccess ||
      ExitRequested()) {
    return false;
  }

  {
    TRACE_EVENT0("sync", "ApplyUpdates");

    // Apply updates to the other types. May or may not involve cross-thread
    // traffic, depending on the underlying update handlers and the GU type's
    // delegate.
    get_updates_processor.ApplyUpdates(download_types,
                                       cycle->mutable_status_controller());

    cycle->SendEventNotification(SyncCycleEvent::STATUS_CHANGED);
  }

  return !ExitRequested();
}

SyncerError Syncer::BuildAndPostCommits(const DataTypeSet& request_types,
                                        NudgeTracker* nudge_tracker,
                                        SyncCycle* cycle) {
  VLOG(1) << "Committing from types "
          << DataTypeSetToDebugString(request_types);

  CommitProcessor commit_processor(
      request_types,
      cycle->context()->data_type_registry()->commit_contributor_map());

  // The ExitRequested() check is unnecessary, since we should start getting
  // errors from the ServerConnectionManager if an exist has been requested.
  // However, it doesn't hurt to check it anyway.
  while (!ExitRequested()) {
    std::unique_ptr<Commit> commit = Commit::Init(
        cycle->context()->GetConnectedTypes(),
        cycle->context()->max_commit_batch_size(),
        cycle->context()->account_name(), cycle->context()->cache_guid(),
        cycle->context()->cookie_jar_mismatch(), GetInvalidationInfo(cycle),
        &commit_processor, cycle->context()->extensions_activity());
    if (!commit) {
      break;
    }

    SyncerError error = commit->PostAndProcessResponse(
        nudge_tracker, cycle, cycle->mutable_status_controller(),
        cycle->context()->extensions_activity());
    base::UmaHistogramEnumeration("Sync.CommitResponse",
                                  GetSyncerErrorValueForUma(error));
    for (DataType type : commit->GetContributingDataTypes()) {
      const std::string kPrefix = "Sync.CommitResponse.";
      base::UmaHistogramEnumeration(kPrefix + DataTypeToHistogramSuffix(type),
                                    GetSyncerErrorValueForUma(error));
    }
    if (error.type() != SyncerError::Type::kSuccess) {
      return error;
    }
    nudge_tracker->RecordSuccessfulCommitMessage(
        commit->GetContributingDataTypes());
  }

  return SyncerError::Success();
}

bool Syncer::ExitRequested() {
  return cancelation_signal_->IsSignalled();
}

bool Syncer::HandleCycleEnd(SyncCycle* cycle,
                            sync_pb::SyncEnums::GetUpdatesOrigin origin) {
  if (ExitRequested()) {
    return false;
  }

  bool success =
      !HasSyncerError(cycle->status_controller().model_neutral_state());
  if (success && origin == sync_pb::SyncEnums::PERIODIC) {
    cycle->mutable_status_controller()->UpdatePollTime();
  }
  cycle->SendSyncCycleEndEventNotification(origin);

  return success;
}

}  // namespace syncer
