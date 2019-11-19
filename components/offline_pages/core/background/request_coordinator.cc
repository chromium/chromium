// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/request_coordinator.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/background/offliner_client.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace offline_pages {

namespace {
const bool kUserRequest = true;
const bool kStartOfProcessing = true;
constexpr base::TimeDelta kMinDuration = base::TimeDelta::FromSeconds(1);
constexpr base::TimeDelta kMaxDuration = base::TimeDelta::FromDays(7);
const int kDurationBuckets = 50;
const int kDisabledTaskRecheckSeconds = 5;

// TODO(dougarnett): Move to util location and share with model impl.
std::string AddHistogramSuffix(const ClientId& client_id,
                               const char* histogram_name) {
  if (client_id.name_space.empty()) {
    NOTREACHED();
    return histogram_name;
  }
  std::string adjusted_histogram_name(histogram_name);
  adjusted_histogram_name += ".";
  adjusted_histogram_name += client_id.name_space;
  return adjusted_histogram_name;
}

// Records the request status UMA for an offlining request. This should
// only be called once per Offliner::LoadAndSave request.
void RecordOfflinerResultUMA(const ClientId& client_id,
                             const base::Time& request_creation_time,
                             Offliner::RequestStatus request_status) {
  base::UmaHistogramEnumeration(
      AddHistogramSuffix(client_id,
                         "OfflinePages.Background.OfflinerRequestStatus"),
      request_status);

  // For successful requests also record time from request to save.
  if (request_status == Offliner::RequestStatus::SAVED ||
      request_status == Offliner::RequestStatus::SAVED_ON_LAST_RETRY) {
    base::TimeDelta duration = OfflineTimeNow() - request_creation_time;
    base::UmaHistogramCustomCounts(
        AddHistogramSuffix(client_id, "OfflinePages.Background.TimeToSaved"),
        duration.InSeconds(), kMinDuration.InSeconds(),
        kMaxDuration.InSeconds(), kDurationBuckets);
  }
}

// Records final request status UMA for an offlining request. This should only
// be called once per Offliner::LoadAndSave request. Every Offliner::LoadAndSave
// request should also call this once.
void RecordSavePageResultUMA(
    const ClientId& client_id,
    RequestNotifier::BackgroundSavePageResult request_status) {
  base::UmaHistogramEnumeration(
      AddHistogramSuffix(client_id,
                         "OfflinePages.Background.FinalSavePageResult"),
      request_status);
}

// Records whether the request comes from CCT or not
void RecordSavePageResultCCTUMA(const ClientId& client_id,
                                const std::string& origin) {
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      AddHistogramSuffix(client_id, "OfflinePages.Background.SavePageFromCCT"),
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddBoolean(!origin.empty());
}

void RecordStartTimeUMA(const SavePageRequest& request) {
  std::string histogram_name("OfflinePages.Background.TimeToStart");
  if (base::SysInfo::IsLowEndDevice()) {
    histogram_name += ".Svelte";
  }

  base::TimeDelta duration = OfflineTimeNow() - request.creation_time();
  base::UmaHistogramCustomTimes(
      AddHistogramSuffix(request.client_id(), histogram_name.c_str()), duration,
      base::TimeDelta::FromMilliseconds(100), base::TimeDelta::FromDays(7), 50);
}

void RecordCancelTimeUMA(const SavePageRequest& canceled_request) {
  // Using regular histogram (with dynamic suffix) rather than time-oriented
  // one to record samples in seconds rather than milliseconds.
  base::TimeDelta duration =
      OfflineTimeNow() - canceled_request.creation_time();
  base::UmaHistogramCustomCounts(
      AddHistogramSuffix(canceled_request.client_id(),
                         "OfflinePages.Background.TimeToCanceled"),
      duration.InSeconds(), kMinDuration.InSeconds(), kMaxDuration.InSeconds(),
      kDurationBuckets);
}

// Records the number of started attempts for completed requests (whether
// successful or not).
void RecordAttemptCount(const SavePageRequest& request,
                        RequestNotifier::BackgroundSavePageResult status) {
  if (status == RequestNotifier::BackgroundSavePageResult::SUCCESS) {
    // TODO(dougarnett): Also record UMA for completed attempts here.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OfflinePages.Background.RequestSuccess.StartedAttemptCount",
        request.started_attempt_count(), 1, 10, 11);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OfflinePages.Background.RequestFailure.StartedAttemptCount",
        request.started_attempt_count(), 1, 10, 11);
  }
}

// Record the network quality at request creation time per namespace.
void RecordSavePageLaterNetworkQuality(
    const ClientId& client_id,
    const net::EffectiveConnectionType effective_connection) {
  // The histogram below is an expansion of the UMA_HISTOGRAM_ENUMERATION
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      AddHistogramSuffix(
          client_id,
          "OfflinePages.Background.EffectiveConnectionType.SavePageLater"),
      1, net::EFFECTIVE_CONNECTION_TYPE_LAST - 1,
      net::EFFECTIVE_CONNECTION_TYPE_LAST,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(effective_connection);
}

// Record the network quality at request creation time per namespace.
void RecordNetworkQualityAtRequestStartForFailedRequest(
    const ClientId& client_id,
    const net::EffectiveConnectionType effective_connection) {
  // The histogram below is an expansion of the UMA_HISTOGRAM_ENUMERATION
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      AddHistogramSuffix(
          client_id,
          "OfflinePages.Background.EffectiveConnectionType.OffliningStartType"),
      1, net::EFFECTIVE_CONNECTION_TYPE_LAST - 1,
      net::EFFECTIVE_CONNECTION_TYPE_LAST,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(effective_connection);
}

// Returns whether |result| is a successful result for a single request.
bool IsSingleSuccessResult(const UpdateRequestsResult& result) {
  return result.store_state == StoreState::LOADED &&
         result.item_statuses.size() == 1 &&
         result.item_statuses.at(0).second == ItemActionStatus::SUCCESS;
}

FailState RequestStatusToFailState(Offliner::RequestStatus request_status) {
  if (request_status == Offliner::RequestStatus::SAVED ||
      request_status == Offliner::RequestStatus::SAVED_ON_LAST_RETRY) {
    return FailState::NO_FAILURE;
  } else if (request_status ==
             Offliner::RequestStatus::LOADING_FAILED_NET_ERROR) {
    return FailState::NETWORK_INSTABILITY;
  } else {
    return FailState::CANNOT_DOWNLOAD;
  }
}

// Returns true if |status| originates from the RequestCoordinator, as opposed
// to an |Offliner| result.
constexpr bool IsCanceledOrInternalFailure(Offliner::RequestStatus status) {
  switch (status) {
    case Offliner::RequestStatus::FOREGROUND_CANCELED:
    case Offliner::RequestStatus::LOADING_CANCELED:
    case Offliner::RequestStatus::QUEUE_UPDATE_FAILED:
    case Offliner::RequestStatus::LOADING_NOT_ACCEPTED:
    case Offliner::RequestStatus::LOADING_DEFERRED:
    case Offliner::RequestStatus::BACKGROUND_SCHEDULER_CANCELED:
    case Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED:
      return true;
    default:
      return false;
  }
}

// Returns the |BackgroundSavePageResult| appropriate for a single attempt
// status. Returns |base::nullopt| for indeterminate status values that can be
// retried.
base::Optional<RequestNotifier::BackgroundSavePageResult> SingleAttemptResult(
    Offliner::RequestStatus status) {
  switch (status) {
      // Success status values.
    case Offliner::RequestStatus::SAVED:
    case Offliner::RequestStatus::SAVED_ON_LAST_RETRY:
      return RequestNotifier::BackgroundSavePageResult::SUCCESS;

      // Cancellation status values.
    case Offliner::RequestStatus::FOREGROUND_CANCELED:
    case Offliner::RequestStatus::LOADING_CANCELED:
    case Offliner::RequestStatus::QUEUE_UPDATE_FAILED:
    case Offliner::RequestStatus::LOADING_NOT_ACCEPTED:
    case Offliner::RequestStatus::LOADING_DEFERRED:
    case Offliner::RequestStatus::BACKGROUND_SCHEDULER_CANCELED:
    case Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED:
      return base::nullopt;

      // Other failure status values.
    case Offliner::RequestStatus::LOADING_FAILED_NO_RETRY:
    case Offliner::RequestStatus::LOADING_FAILED_DOWNLOAD:
    case Offliner::RequestStatus::LOADED_PAGE_HAS_CERTIFICATE_ERROR:
    case Offliner::RequestStatus::LOADED_PAGE_IS_BLOCKED:
    case Offliner::RequestStatus::LOADED_PAGE_IS_CHROME_INTERNAL:
      return RequestNotifier::BackgroundSavePageResult::LOADING_FAILURE;
    case Offliner::RequestStatus::DOWNLOAD_THROTTLED:
      return RequestNotifier::BackgroundSavePageResult::DOWNLOAD_THROTTLED;
    case Offliner::RequestStatus::SAVE_FAILED:
    case Offliner::RequestStatus::LOADING_FAILED:
    case Offliner::RequestStatus::LOADING_FAILED_NET_ERROR:
    case Offliner::RequestStatus::LOADING_FAILED_HTTP_ERROR:
    case Offliner::RequestStatus::LOADING_FAILED_NO_NEXT:
    case Offliner::RequestStatus::REQUEST_COORDINATOR_TIMED_OUT:
      return base::nullopt;

    // Only used by |Offliner| internally.
    case Offliner::RequestStatus::UNKNOWN:
    case Offliner::RequestStatus::LOADED:
    // Deprecated.
    case Offliner::RequestStatus::DEPRECATED_LOADING_NOT_STARTED:
    // Only recorded by |RequestCoordinator| directly.
    case Offliner::RequestStatus::BROWSER_KILLED:
      DCHECK(false) << "Received invalid status: " << static_cast<int>(status);
      return base::nullopt;
  }
}

}  // namespace

RequestCoordinator::SavePageLaterParams::SavePageLaterParams()
    : user_requested(true),
      availability(RequestAvailability::ENABLED_FOR_OFFLINER) {}

RequestCoordinator::SavePageLaterParams::SavePageLaterParams(
    const SavePageLaterParams& other) = default;

RequestCoordinator::SavePageLaterParams::~SavePageLaterParams() = default;

RequestCoordinator::RequestCoordinator(
    std::unique_ptr<OfflinerPolicy> policy,
    std::unique_ptr<Offliner> offliner,
    std::unique_ptr<RequestQueue> queue,
    std::unique_ptr<Scheduler> scheduler,
    network::NetworkQualityTracker* network_quality_tracker,
    std::unique_ptr<ActiveTabInfo> active_tab_info)
    : is_low_end_device_(base::SysInfo::IsLowEndDevice()),
      state_(RequestCoordinatorState::IDLE),
      processing_state_(ProcessingWindowState::STOPPED),
      use_test_device_conditions_(false),
      policy_(std::move(policy)),
      queue_(std::move(queue)),
      scheduler_(std::move(scheduler)),
      network_quality_tracker_(network_quality_tracker),
      network_quality_at_request_start_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      last_offlining_status_(Offliner::RequestStatus::UNKNOWN),
      scheduler_callback_(base::DoNothing()),
      internal_start_processing_callback_(base::DoNothing()),
      pending_state_updater_(this),
      active_tab_info_(std::move(active_tab_info)) {
  DCHECK(policy_ != nullptr);
  DCHECK(network_quality_tracker_);
  offliner_client_ = std::make_unique<OfflinerClient>(
      std::move(offliner),
      base::BindRepeating(&RequestCoordinator::OfflinerProgressCallback,
                          weak_ptr_factory_.GetWeakPtr()));
  std::unique_ptr<CleanupTaskFactory> cleanup_factory(
      new CleanupTaskFactory(policy_.get(), this, &event_logger_));
  queue_->SetCleanupFactory(std::move(cleanup_factory));
  // If we exited with any items left in the OFFLINING state, move them back to
  // the AVAILABLE state, and update the UI by sending notifications.  Do this
  // before we cleanup, so any requests that are now OFFLINING which have
  // expired can be legitimate candidates for cleanup.
  queue_->ReconcileRequests(base::BindOnce(
      &RequestCoordinator::ReconcileCallback, weak_ptr_factory_.GetWeakPtr()));
  // Do a cleanup of expired or over tried requests at startup time.
  queue_->CleanupRequestQueue();
}

RequestCoordinator::~RequestCoordinator() {}

int64_t RequestCoordinator::SavePageLater(
    const SavePageLaterParams& save_page_later_params,
    SavePageLaterCallback save_page_later_callback) {
  DVLOG(2) << "URL is " << save_page_later_params.url << " " << __func__;

  if (!OfflinePageModel::CanSaveURL(save_page_later_params.url)) {
    DVLOG(1) << "Not able to save page for requested url: "
             << save_page_later_params.url;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(save_page_later_callback),
                                  AddRequestResult::URL_ERROR));
    return 0L;
  }

  int64_t id = store_utils::GenerateOfflineId();

  // Build a SavePageRequest.
  offline_pages::SavePageRequest request(
      id, save_page_later_params.url, save_page_later_params.client_id,
      OfflineTimeNow(), save_page_later_params.user_requested);
  request.set_original_url(save_page_later_params.original_url);
  request.set_request_origin(save_page_later_params.request_origin);
  pending_state_updater_.SetPendingState(request);

  // If the download manager is not done with the request, put it on the
  // disabled list.
  if (save_page_later_params.availability ==
      RequestAvailability::DISABLED_FOR_OFFLINER) {
    disabled_requests_.insert(id);
  }

  // Put the request on the request queue.
  queue_->AddRequest(
      request, save_page_later_params.add_options,
      base::BindOnce(&RequestCoordinator::AddRequestResultCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(save_page_later_callback),
                     save_page_later_params.availability));

  // Record the network quality when this request is made.
  RecordSavePageLaterNetworkQuality(
      save_page_later_params.client_id,
      network_quality_tracker_->GetEffectiveConnectionType());

  return id;
}

void RequestCoordinator::GetAllRequests(GetRequestsCallback callback) {
  // Get all matching requests from the request queue, send them to our
  // callback.  We bind the namespace and callback to the front of the callback
  // param set.
  queue_->GetRequests(
      base::BindOnce(&RequestCoordinator::GetQueuedRequestsCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RequestCoordinator::SetAutoFetchNotificationState(
    int64_t request_id,
    SavePageRequest::AutoFetchNotificationState state,
    base::OnceCallback<void(bool updated)> callback) {
  queue_->SetAutoFetchNotificationState(request_id, state, std::move(callback));
}

void RequestCoordinator::GetQueuedRequestsCallback(
    GetRequestsCallback callback,
    GetRequestsResult result,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  for (auto& request : requests) {
    pending_state_updater_.SetPendingState(*request);
  }
  std::move(callback).Run(std::move(requests));
}

void RequestCoordinator::StopProcessing(Offliner::RequestStatus stop_status) {
  if (offliner_client_ && offliner_client_->Active() &&
      state_ == RequestCoordinatorState::OFFLINING) {
    offliner_client_->Stop(stop_status);
    return;
  }
  last_offlining_status_ = stop_status;
  state_ = RequestCoordinatorState::IDLE;
  ScheduleOrTryNextRequest(stop_status);
}

void RequestCoordinator::GetRequestsForSchedulingCallback(
    GetRequestsResult result,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  bool user_requested = false;

  // Examine all requests, if we find a user requested one, we will use the less
  // restrictive conditions for user_requested requests.  Otherwise we will use
  // the more restrictive non-user-requested conditions.
  for (const auto& request : requests) {
    if (request->user_requested()) {
      user_requested = true;
      break;
    }
  }

  // In the get callback, determine the least restrictive, and call
  // GetTriggerConditions based on that.
  scheduler_->Schedule(GetTriggerConditions(user_requested));
}

bool RequestCoordinator::CancelActiveRequestIfItMatches(int64_t request_id) {
  // If we have a request in progress and need to cancel it, call the
  // offliner to cancel.
  if (offliner_client_->Active() &&
      offliner_client_->ActiveRequest()->request_id() == request_id) {
    StopProcessing(Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED);
    return true;
  }

  return false;
}

void RequestCoordinator::UpdateRequestForAbortedAttempt(
    const SavePageRequest& request) {
  if (request.started_attempt_count() >= policy_->GetMaxStartedTries()) {
    const RequestNotifier::BackgroundSavePageResult result(
        RequestNotifier::BackgroundSavePageResult::START_COUNT_EXCEEDED);
    event_logger_.RecordDroppedSavePageRequest(request.client_id().name_space,
                                               result, request.request_id());
    RemoveAttemptedRequest(request, result);
  } else {
    MarkAttemptAborted(request.request_id(), request.client_id().name_space);
  }
}

void RequestCoordinator::RemoveAttemptedRequest(
    const SavePageRequest& request,
    RequestNotifier::BackgroundSavePageResult result) {
  std::vector<int64_t> remove_requests;
  remove_requests.push_back(request.request_id());
  queue_->RemoveRequests(
      remove_requests,
      base::BindOnce(&RequestCoordinator::HandleRemovedRequests,
                     weak_ptr_factory_.GetWeakPtr(), result));
  RecordAttemptCount(request, result);
}

void RequestCoordinator::MarkAttemptAborted(int64_t request_id,
                                            const std::string& name_space) {
  queue_->MarkAttemptAborted(
      request_id,
      base::BindOnce(&RequestCoordinator::MarkAttemptDone,
                     weak_ptr_factory_.GetWeakPtr(), request_id, name_space));
}

void RequestCoordinator::MarkAttemptDone(int64_t request_id,
                                         const std::string& name_space,
                                         UpdateRequestsResult result) {
  // If the request succeeded, notify observer. If it failed, we can't really
  // do much, so just log it.
  if (IsSingleSuccessResult(result)) {
    NotifyChanged(result.updated_items.at(0));
  } else {
    DVLOG(1) << "Failed to mark attempt: " << request_id;
    UpdateRequestResult request_result =
        result.store_state != StoreState::LOADED
            ? UpdateRequestResult::STORE_FAILURE
            : UpdateRequestResult::REQUEST_DOES_NOT_EXIST;
    event_logger_.RecordUpdateRequestFailed(name_space, request_result);
  }
}

void RequestCoordinator::RemoveRequests(const std::vector<int64_t>& request_ids,
                                        RemoveRequestsCallback callback) {
  queue_->RemoveRequests(
      request_ids,
      base::BindOnce(&RequestCoordinator::HandleRemovedRequestsAndCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     RequestNotifier::BackgroundSavePageResult::USER_CANCELED));

  // Record the network quality when this request is removed.
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.Background.EffectiveConnectionType.RemoveRequests",
      network_quality_tracker_->GetEffectiveConnectionType(),
      net::EFFECTIVE_CONNECTION_TYPE_LAST);
}

void RequestCoordinator::RemoveRequestsIf(
    const base::RepeatingCallback<bool(const SavePageRequest&)>&
        remove_predicate,
    RemoveRequestsCallback callback) {
  queue_->RemoveRequestsIf(
      std::move(remove_predicate),
      base::BindOnce(&RequestCoordinator::HandleRemovedRequestsAndCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     RequestNotifier::BackgroundSavePageResult::USER_CANCELED));
}

void RequestCoordinator::PauseRequests(
    const std::vector<int64_t>& request_ids) {
  // Remove the paused requests from prioritized list.
  for (int64_t id : request_ids) {
    auto it = std::find(prioritized_requests_.begin(),
                        prioritized_requests_.end(), id);
    if (it != prioritized_requests_.end())
      prioritized_requests_.erase(it);
  }

  queue_->ChangeRequestsState(
      request_ids, SavePageRequest::RequestState::PAUSED,
      base::BindOnce(&RequestCoordinator::UpdateMultipleRequestsCallback,
                     weak_ptr_factory_.GetWeakPtr()));

  // Record the network quality when this request is paused.
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.Background.EffectiveConnectionType.PauseRequests",
      network_quality_tracker_->GetEffectiveConnectionType(),
      net::EFFECTIVE_CONNECTION_TYPE_LAST);
}

void RequestCoordinator::ResumeRequests(
    const std::vector<int64_t>& request_ids) {
  prioritized_requests_.insert(prioritized_requests_.end(), request_ids.begin(),
                               request_ids.end());
  queue_->ChangeRequestsState(
      request_ids, SavePageRequest::RequestState::AVAILABLE,
      base::BindOnce(&RequestCoordinator::UpdateMultipleRequestsCallback,
                     weak_ptr_factory_.GetWeakPtr()));

  // Record the network quality when this request is resumed.
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.Background.EffectiveConnectionType.ResumeRequests",
      network_quality_tracker_->GetEffectiveConnectionType(),
      net::EFFECTIVE_CONNECTION_TYPE_LAST);

  // Schedule a task, in case there is not one scheduled.
  ScheduleAsNeeded();
}

void RequestCoordinator::AddRequestResultCallback(
    SavePageLaterCallback save_page_later_callback,
    RequestAvailability availability,
    AddRequestResult result,
    const SavePageRequest& request) {
  if (result == AddRequestResult::SUCCESS) {
    NotifyAdded(request);
    // Inform the scheduler that we have an outstanding task.
    scheduler_->Schedule(GetTriggerConditions(kUserRequest));

    if (availability == RequestAvailability::DISABLED_FOR_OFFLINER) {
      // Mark attempt started (presuming it is disabled for background offliner
      // because foreground offlining is happening).
      queue_->MarkAttemptStarted(
          request.request_id(),
          base::BindOnce(&RequestCoordinator::MarkAttemptDone,
                         weak_ptr_factory_.GetWeakPtr(), request.request_id(),
                         request.client_id().name_space));
    } else if (request.user_requested()) {
      StartImmediatelyIfConnected();
    }
  } else {
    event_logger_.RecordAddRequestFailed(request.client_id().name_space,
                                         result);
  }
  std::move(save_page_later_callback).Run(result);
}

void RequestCoordinator::UpdateMultipleRequestsCallback(
    UpdateRequestsResult result) {
  for (auto& request : result.updated_items) {
    if (request.request_state() == SavePageRequest::RequestState::PAUSED) {
      CancelActiveRequestIfItMatches(request.request_id());
    }
    pending_state_updater_.SetPendingState(request);
    NotifyChanged(request);
  }

  bool available_user_request = false;
  for (const auto& request : result.updated_items) {
    if (!available_user_request && request.user_requested() &&
        request.request_state() == SavePageRequest::RequestState::AVAILABLE) {
      available_user_request = true;
    }
  }

  if (available_user_request)
    StartImmediatelyIfConnected();
}

void RequestCoordinator::ReconcileCallback(UpdateRequestsResult result) {
  for (auto& request : result.updated_items) {
    RecordOfflinerResult(request, Offliner::RequestStatus::BROWSER_KILLED);
    pending_state_updater_.SetPendingState(request);
    NotifyChanged(request);
  }
}

void RequestCoordinator::HandleRemovedRequestsAndCallback(
    RemoveRequestsCallback callback,
    RequestNotifier::BackgroundSavePageResult status,
    UpdateRequestsResult result) {
  // TODO(dougarnett): Define status code for user/api cancel and use here
  // to determine whether to record cancel time UMA.
  for (const auto& request : result.updated_items) {
    RecordCancelTimeUMA(request);
    CancelActiveRequestIfItMatches(request.request_id());
  }
  std::move(callback).Run(result.item_statuses);
  HandleRemovedRequests(status, std::move(result));
}

void RequestCoordinator::HandleRemovedRequests(
    RequestNotifier::BackgroundSavePageResult status,
    UpdateRequestsResult result) {
  for (const auto& request : result.updated_items)
    NotifyCompleted(request, status);
}

void RequestCoordinator::MarkDeferredAttemptCallback(
    UpdateRequestsResult result) {
  // This is called after the attempt has been marked as deferred in the
  // database. StopProcessing() is called to resume request processing.
  state_ = RequestCoordinatorState::IDLE;
  StopProcessing(Offliner::RequestStatus::LOADING_DEFERRED);
}

void RequestCoordinator::ScheduleAsNeeded() {
  // Get all requests from queue (there is no filtering mechanism).
  queue_->GetRequests(
      base::BindOnce(&RequestCoordinator::GetRequestsForSchedulingCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RequestCoordinator::CancelProcessing() {
  StopProcessing(Offliner::RequestStatus::BACKGROUND_SCHEDULER_CANCELED);
}

// Returns true if the caller should expect a callback, false otherwise. For
// instance, this would return false if a request is already in progress.
bool RequestCoordinator::StartScheduledProcessing(
    const DeviceConditions& device_conditions,
    const base::RepeatingCallback<void(bool)>& callback) {
  DVLOG(2) << "Scheduled " << __func__;
  current_conditions_.reset(new DeviceConditions(device_conditions));
  return StartProcessingInternal(ProcessingWindowState::SCHEDULED_WINDOW,
                                 callback);
}

// Returns true if the caller should expect a callback, false otherwise.
bool RequestCoordinator::StartImmediateProcessing(
    const base::RepeatingCallback<void(bool)>& callback) {
  UpdateCurrentConditionsFromAndroid();
  OfflinerImmediateStartStatus immediate_start_status =
      TryImmediateStart(callback);
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.Background.ImmediateStartStatus", immediate_start_status,
      RequestCoordinator::OfflinerImmediateStartStatus::STATUS_COUNT);
  return immediate_start_status == OfflinerImmediateStartStatus::STARTED;
}

// The current_conditions_ must be set sometime before calling
// StartProcessingInternal on all calling code paths.
bool RequestCoordinator::StartProcessingInternal(
    const ProcessingWindowState processing_state,
    const base::RepeatingCallback<void(bool)>& callback) {
  if (state_ != RequestCoordinatorState::IDLE)
    return false;
  processing_state_ = processing_state;
  scheduler_callback_ = callback;

  // Mark the time at which we started processing so we can check our time
  // budget.
  operation_start_time_ = OfflineTimeNow();

  TryNextRequest(kStartOfProcessing);

  return true;
}

void RequestCoordinator::StartImmediatelyIfConnected() {
  StartImmediateProcessing(internal_start_processing_callback_);
}

RequestCoordinator::OfflinerImmediateStartStatus
RequestCoordinator::TryImmediateStart(
    const base::RepeatingCallback<void(bool)>& callback) {
  DVLOG(2) << "Immediate " << __func__;
  // Make sure not already busy processing.
  if (state_ == RequestCoordinatorState::OFFLINING)
    return OfflinerImmediateStartStatus::BUSY;

  // Make sure we are not on svelte device to start immediately.
  if (is_low_end_device_) {
    DVLOG(2) << "low end device, returning";
    // Let the scheduler know we are done processing and failed due to svelte.
    callback.Run(false);
    return OfflinerImmediateStartStatus::NOT_STARTED_ON_SVELTE;
  }

  if (current_conditions_->GetNetConnectionType() ==
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE) {
    RequestConnectedEventForStarting();
    return OfflinerImmediateStartStatus::NO_CONNECTION;
  }
  // Clear any pending connected event request since we have connection
  // and will start processing.
  ClearConnectedEventRequest();

  if (StartProcessingInternal(ProcessingWindowState::IMMEDIATE_WINDOW,
                              callback)) {
    return OfflinerImmediateStartStatus::STARTED;
  }
  return OfflinerImmediateStartStatus::NOT_ACCEPTED;
}

void RequestCoordinator::RequestConnectedEventForStarting() {
  connection_notifier_.reset(new ConnectionNotifier(
      base::BindOnce(&RequestCoordinator::HandleConnectedEventForStarting,
                     weak_ptr_factory_.GetWeakPtr())));
}

void RequestCoordinator::ClearConnectedEventRequest() {
  connection_notifier_.reset(nullptr);
}

void RequestCoordinator::HandleConnectedEventForStarting() {
  ClearConnectedEventRequest();
  StartImmediatelyIfConnected();
}

void RequestCoordinator::UpdateCurrentConditionsFromAndroid() {
  // If we have already set the connection type for testing, don't get it from
  // android, but use what the test already set up.
  if (use_test_device_conditions_)
    return;

  current_conditions_ = std::make_unique<DeviceConditions>(
      scheduler_->GetCurrentDeviceConditions());
}

void RequestCoordinator::TryNextRequest(bool is_start_of_processing) {
  state_ = RequestCoordinatorState::PICKING;

  // Connection type at previous check.
  net::NetworkChangeNotifier::ConnectionType previous_connection_type =
      current_conditions_->GetNetConnectionType();

  // If this is the first call, the device conditions are current, no need to
  // update them.
  if (!is_start_of_processing) {
    // Get current device conditions from the Java side across the bridge.
    // NetworkChangeNotifier will not have the right conditions if chromium is
    // in the background in android, so prefer to always get the conditions via
    // the android APIs.
    UpdateCurrentConditionsFromAndroid();
  }

  // Current connection type.
  net::NetworkChangeNotifier::ConnectionType connection_type =
      current_conditions_->GetNetConnectionType();

  // If there was loss of network, update all available requests to waiting for
  // network connection.
  if (connection_type != previous_connection_type &&
      connection_type ==
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE) {
    pending_state_updater_.UpdateRequestsOnLossOfNetwork();
  }

  base::TimeDelta processing_time_budget;
  if (processing_state_ == ProcessingWindowState::SCHEDULED_WINDOW) {
    processing_time_budget = base::TimeDelta::FromSeconds(
        policy_->GetProcessingTimeBudgetWhenBackgroundScheduledInSeconds());
  } else {
    DCHECK(processing_state_ == ProcessingWindowState::IMMEDIATE_WINDOW);
    processing_time_budget = base::TimeDelta::FromSeconds(
        policy_->GetProcessingTimeBudgetForImmediateLoadInSeconds());
  }

  // If there is no network or no time left in the budget, return to the
  // scheduler. We do not remove the pending scheduler task that was set
  // up earlier in case we run out of time, so the background scheduler
  // will return to us at the next opportunity to run background tasks.
  if (connection_type ==
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE ||
      (OfflineTimeNow() - operation_start_time_) > processing_time_budget) {
    state_ = RequestCoordinatorState::IDLE;

    // If we were doing immediate processing, try to start it again
    // when we get connected.
    if (processing_state_ == ProcessingWindowState::IMMEDIATE_WINDOW)
      RequestConnectedEventForStarting();

    // Let the scheduler know we are done processing.
    scheduler_callback_.Run(true);
    DVLOG(2) << " out of time, giving up. " << __func__;

    return;
  }

  // Ask request queue to make a new PickRequestTask object, then put it on
  // the task queue.
  queue_->PickNextRequest(
      policy_.get(),
      base::BindOnce(&RequestCoordinator::RequestPicked,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&RequestCoordinator::RequestNotPicked,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&RequestCoordinator::RequestCounts,
                     weak_ptr_factory_.GetWeakPtr(), is_start_of_processing),
      *current_conditions_, disabled_requests_, &prioritized_requests_);
}

// Called by the request picker when a request has been picked.
void RequestCoordinator::RequestPicked(
    const SavePageRequest& picked_request,
    std::unique_ptr<std::vector<SavePageRequest>> available_requests,
    bool cleanup_needed) {
  DVLOG(2) << picked_request.url() << " " << __func__;

  // Make sure we were not stopped while picking, since any kind of cancel/stop
  // will reset the state back to IDLE.
  if (state_ == RequestCoordinatorState::PICKING) {
    state_ = RequestCoordinatorState::OFFLINING;
    // Send the request on to the offliner.
    SendRequestToOffliner(picked_request);

    // Update the pending state for available requests if needed.
    pending_state_updater_.UpdateRequestsOnRequestPicked(
        picked_request.request_id(), std::move(available_requests));
  }

  // Schedule a queue cleanup if needed.
  if (cleanup_needed)
    queue_->CleanupRequestQueue();
}

void RequestCoordinator::RequestNotPicked(
    bool non_user_requested_tasks_remaining,
    bool cleanup_needed,
    base::Time available_time) {
  DVLOG(2) << __func__;
  state_ = RequestCoordinatorState::IDLE;

  // Clear the outstanding "safety" task in the scheduler.
  scheduler_->Unschedule();

  // If disabled tasks remain, post a new safety task for 5 sec from now.
  if (disabled_requests_.size() > 0) {
    scheduler_->BackupSchedule(GetTriggerConditions(kUserRequest),
                               kDisabledTaskRecheckSeconds);
  } else if (non_user_requested_tasks_remaining) {
    // If we don't have any of those, check for non-user-requested tasks.
    scheduler_->Schedule(GetTriggerConditions(!kUserRequest));
  } else if (!available_time.is_null()) {
    scheduler_->BackupSchedule(
        GetTriggerConditions(kUserRequest),
        (available_time - OfflineTimeNow()).InSeconds() +
            1 /*Add an extra second to avoid rounding down.*/);
  }

  // Schedule a queue cleanup if needed.
  if (cleanup_needed)
    queue_->CleanupRequestQueue();

  // Let the scheduler know we are done processing.
  scheduler_callback_.Run(true);
}

void RequestCoordinator::RequestCounts(bool is_start_of_processing,
                                       size_t total_requests,
                                       size_t available_requests) {
  // Only capture request counts for the start of processing (not for
  // continued processing in the same window).
  if (!is_start_of_processing)
    return;

  if (processing_state_ == ProcessingWindowState::SCHEDULED_WINDOW) {
    if (is_low_end_device_) {
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ScheduledStart.AvailableRequestCount."
          "Svelte",
          available_requests);
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ScheduledStart.UnavailableRequestCount."
          "Svelte",
          total_requests - available_requests);
    } else {
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ScheduledStart.AvailableRequestCount",
          available_requests);
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ScheduledStart.UnavailableRequestCount",
          total_requests - available_requests);
    }
  } else if (processing_state_ == ProcessingWindowState::IMMEDIATE_WINDOW) {
    if (is_low_end_device_) {
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ImmediateStart.AvailableRequestCount."
          "Svelte",
          available_requests);
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ImmediateStart.UnavailableRequestCount."
          "Svelte",
          total_requests - available_requests);
    } else {
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ImmediateStart.AvailableRequestCount",
          available_requests);
      UMA_HISTOGRAM_COUNTS_1000(
          "OfflinePages.Background.ImmediateStart.UnavailableRequestCount",
          total_requests - available_requests);
    }
  }
}

void RequestCoordinator::SendRequestToOffliner(const SavePageRequest& request) {
  DCHECK(state_ == RequestCoordinatorState::OFFLINING);
  // Record start time if this is first attempt.
  if (request.started_attempt_count() == 0) {
    RecordStartTimeUMA(request);
  }
  const OfflinePageClientPolicy& policy =
      GetPolicy(request.client_id().name_space);
  if (policy.defer_background_fetch_while_page_is_active &&
      active_tab_info_->DoesActiveTabMatch(request.url())) {
    queue_->MarkAttemptDeferred(
        request.request_id(),
        base::BindOnce(&RequestCoordinator::MarkDeferredAttemptCallback,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Mark attempt started in the database and start offliner when completed.
  queue_->MarkAttemptStarted(
      request.request_id(),
      base::BindOnce(&RequestCoordinator::StartOffliner,
                     weak_ptr_factory_.GetWeakPtr(), request.request_id(),
                     request.client_id().name_space));
}

void RequestCoordinator::StartOffliner(int64_t request_id,
                                       const std::string& client_namespace,
                                       UpdateRequestsResult update_result) {
  // If the state changed from OFFLINING, another call to RequestCoordinator has
  // resulted in a state change. In this case, it's safe to abort here.
  if (state_ != RequestCoordinatorState::OFFLINING)
    return;
  DCHECK_NE(ProcessingWindowState::STOPPED, processing_state_);

  if (update_result.store_state != StoreState::LOADED ||
      update_result.item_statuses.size() != 1 ||
      update_result.item_statuses.at(0).first != request_id ||
      update_result.item_statuses.at(0).second != ItemActionStatus::SUCCESS) {
    state_ = RequestCoordinatorState::IDLE;
    StopProcessing(Offliner::RequestStatus::QUEUE_UPDATE_FAILED);
    DVLOG(1) << "Failed to mark attempt started: " << request_id;
    UpdateRequestResult request_result =
        update_result.store_state != StoreState::LOADED
            ? UpdateRequestResult::STORE_FAILURE
            : UpdateRequestResult::REQUEST_DOES_NOT_EXIST;
    event_logger_.RecordUpdateRequestFailed(client_namespace, request_result);
    return;
  }

  network_quality_at_request_start_ =
      network_quality_tracker_->GetEffectiveConnectionType();

  base::TimeDelta timeout;
  if (processing_state_ == ProcessingWindowState::SCHEDULED_WINDOW) {
    timeout = base::TimeDelta::FromSeconds(
        policy_->GetSinglePageTimeLimitWhenBackgroundScheduledInSeconds());
  } else {
    timeout = base::TimeDelta::FromSeconds(
        policy_->GetSinglePageTimeLimitForImmediateLoadInSeconds());
  }
  // Start the load and save process in the offliner (Async).
  if (offliner_client_->LoadAndSave(
          update_result.updated_items.at(0), timeout,
          base::BindOnce(&RequestCoordinator::OfflinerDoneCallback,
                         weak_ptr_factory_.GetWeakPtr()))) {
    // Inform observer of active request.
    NotifyChanged(update_result.updated_items.at(0));
  } else {
    state_ = RequestCoordinatorState::IDLE;
    DVLOG(0) << "Unable to start LoadAndSave";
    StopProcessing(Offliner::RequestStatus::LOADING_NOT_ACCEPTED);

    // We need to undo the MarkAttemptStarted that brought us to this
    // method since we didn't success in starting after all.
    MarkAttemptAborted(request_id, client_namespace);
  }
}

void RequestCoordinator::OfflinerDoneCallback(const SavePageRequest& request,
                                              Offliner::RequestStatus status) {
  DVLOG(2) << "offliner finished, saved: "
           << (status == Offliner::RequestStatus::SAVED)
           << ", status: " << static_cast<int>(status) << ", " << __func__;
  RecordOfflinerResult(request, status);
  last_offlining_status_ = status;
  state_ = RequestCoordinatorState::IDLE;
  UpdateRequestForAttempt(request, status);

  ScheduleOrTryNextRequest(status);
}

void RequestCoordinator::UpdateRequestForAttempt(
    const SavePageRequest& request,
    Offliner::RequestStatus status) {
  base::Optional<RequestNotifier::BackgroundSavePageResult> attempt_result =
      SingleAttemptResult(status);

  // If the request failed, report the connection type as of the start of the
  // request.
  if (!attempt_result ||
      attempt_result.value() !=
          RequestNotifier::BackgroundSavePageResult::SUCCESS) {
    RecordNetworkQualityAtRequestStartForFailedRequest(
        request.client_id(), network_quality_at_request_start_);
  }

  if (IsCanceledOrInternalFailure(status)) {
    UpdateRequestForAbortedAttempt(request);
  } else if (attempt_result) {
    RemoveAttemptedRequest(request, attempt_result.value());
  } else if (request.completed_attempt_count() + 1 >=
             policy_->GetMaxCompletedTries()) {
    // Remove from the request queue if we exceeded max retries. The +1
    // represents the request that just completed.
    const RequestNotifier::BackgroundSavePageResult result =
        RequestNotifier::BackgroundSavePageResult::RETRY_COUNT_EXCEEDED;
    event_logger_.RecordDroppedSavePageRequest(request.client_id().name_space,
                                               result, request.request_id());
    RemoveAttemptedRequest(request, result);
  } else {
    // If we failed, but are not over the limit, update the request in the
    // queue.
    queue_->MarkAttemptCompleted(
        request.request_id(), RequestStatusToFailState(status),
        base::BindOnce(&RequestCoordinator::MarkAttemptDone,
                       weak_ptr_factory_.GetWeakPtr(), request.request_id(),
                       request.client_id().name_space));
  }
}

void RequestCoordinator::ScheduleOrTryNextRequest(
    Offliner::RequestStatus previous_status) {
  if (ShouldTryNextRequest(previous_status)) {
    TryNextRequest(!kStartOfProcessing);
  } else {
    processing_state_ = ProcessingWindowState::STOPPED;
    scheduler_callback_.Run(true);
  }
}

void RequestCoordinator::OfflinerProgressCallback(
    const SavePageRequest& request,
    int64_t received_bytes) {
  DVLOG(2) << "offliner progress, received bytes: " << received_bytes;
  DCHECK_GE(received_bytes, 0);
  NotifyNetworkProgress(request, received_bytes);
}

bool RequestCoordinator::ShouldTryNextRequest(
    Offliner::RequestStatus previous_request_status) const {
  switch (previous_request_status) {
    case Offliner::RequestStatus::SAVED:
    case Offliner::RequestStatus::SAVE_FAILED:
    case Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED:
    case Offliner::RequestStatus::LOADING_FAILED:
    case Offliner::RequestStatus::LOADING_FAILED_NET_ERROR:
    case Offliner::RequestStatus::LOADING_FAILED_HTTP_ERROR:
    case Offliner::RequestStatus::LOADING_FAILED_NO_RETRY:
    case Offliner::RequestStatus::LOADING_FAILED_DOWNLOAD:
    case Offliner::RequestStatus::DOWNLOAD_THROTTLED:
    case Offliner::RequestStatus::LOADED_PAGE_HAS_CERTIFICATE_ERROR:
    case Offliner::RequestStatus::LOADED_PAGE_IS_BLOCKED:
    case Offliner::RequestStatus::LOADED_PAGE_IS_CHROME_INTERNAL:
      return true;
    case Offliner::RequestStatus::FOREGROUND_CANCELED:
    case Offliner::RequestStatus::LOADING_CANCELED:
    case Offliner::RequestStatus::LOADING_FAILED_NO_NEXT:
    case Offliner::RequestStatus::BACKGROUND_SCHEDULER_CANCELED:
    case Offliner::RequestStatus::QUEUE_UPDATE_FAILED:
    case Offliner::RequestStatus::LOADING_NOT_ACCEPTED:
    case Offliner::RequestStatus::LOADING_DEFERRED:
      // No further processing in this service window.
      return false;
    case Offliner::RequestStatus::REQUEST_COORDINATOR_TIMED_OUT:
    case Offliner::RequestStatus::SAVED_ON_LAST_RETRY:
      // If we timed out, check to see that there is time budget.
      return processing_state_ == ProcessingWindowState::IMMEDIATE_WINDOW;
    case Offliner::RequestStatus::UNKNOWN:
    case Offliner::RequestStatus::LOADED:
    case Offliner::RequestStatus::DEPRECATED_LOADING_NOT_STARTED:
    case Offliner::RequestStatus::BROWSER_KILLED:
      // Should not be possible to receive these values.
      // Make explicit choice about new status codes that actually reach here.
      // Their default is no further processing in this service window.
      NOTREACHED();
      return false;
  }
}

void RequestCoordinator::EnableForOffliner(int64_t request_id,
                                           const ClientId& client_id) {
  // Since the recent tab helper might call multiple times, ignore subsequent
  // calls for a particular request_id.
  if (disabled_requests_.find(request_id) == disabled_requests_.end())
    return;

  // Clear from disabled list.
  disabled_requests_.erase(request_id);

  // Mark the request as now in available state.
  MarkAttemptAborted(request_id, client_id.name_space);

  // If we are not busy, start processing right away.
  StartImmediatelyIfConnected();
}

void RequestCoordinator::MarkRequestCompleted(int64_t request_id) {
  // Since the recent tab helper might call multiple times, ignore subsequent
  // calls for a particular request_id.
  if (disabled_requests_.find(request_id) == disabled_requests_.end())
    return;
  // Clear from disabled list.
  disabled_requests_.erase(request_id);

  // Remove the request, but send out SUCCEEDED instead of removed.
  // Note: since it had been disabled, it will not have been active in a
  // background offliner, so it is not appropriate to TryNextRequest here.
  std::vector<int64_t> request_ids { request_id };
  queue_->RemoveRequests(
      request_ids,
      base::BindOnce(&RequestCoordinator::HandleRemovedRequests,
                     weak_ptr_factory_.GetWeakPtr(),
                     RequestNotifier::BackgroundSavePageResult::SUCCESS));
}

const Scheduler::TriggerConditions RequestCoordinator::GetTriggerConditions(
    const bool user_requested) {
  return Scheduler::TriggerConditions(
      policy_->PowerRequired(user_requested),
      policy_->BatteryPercentageRequired(user_requested),
      policy_->UnmeteredNetworkRequired(user_requested));
}

void RequestCoordinator::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void RequestCoordinator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RequestCoordinator::NotifyAdded(const SavePageRequest& request) {
  for (Observer& observer : observers_)
    observer.OnAdded(request);
}

void RequestCoordinator::NotifyCompleted(
    const SavePageRequest& request,
    RequestNotifier::BackgroundSavePageResult status) {
  RecordSavePageResultUMA(request.client_id(), status);
  RecordSavePageResultCCTUMA(request.client_id(), request.request_origin());
  for (Observer& observer : observers_)
    observer.OnCompleted(request, status);
}

void RequestCoordinator::NotifyChanged(const SavePageRequest& request) {
  for (Observer& observer : observers_)
    observer.OnChanged(request);
}

void RequestCoordinator::NotifyNetworkProgress(const SavePageRequest& request,
                                               int64_t received_bytes) {
  for (Observer& observer : observers_)
    observer.OnNetworkProgress(request, received_bytes);
}

void RequestCoordinator::RecordOfflinerResult(const SavePageRequest& request,
                                              Offliner::RequestStatus status) {
  event_logger_.RecordOfflinerResult(request.client_id().name_space, status,
                                     request.request_id());
  RecordOfflinerResultUMA(request.client_id(), request.creation_time(), status);
}

void RequestCoordinator::Shutdown() {
}

}  // namespace offline_pages
