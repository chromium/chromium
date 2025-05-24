// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_COORDINATOR_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_COORDINATOR_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/core/background/connection_notifier.h"
#include "components/offline_pages/core/background/device_conditions.h"
#include "components/offline_pages/core/background/pending_state_updater.h"
#include "components/offline_pages/core/background/request_coordinator_event_logger.h"
#include "components/offline_pages/core/background/request_notifier.h"
#include "components/offline_pages/core/background/request_queue.h"
#include "components/offline_pages/core/background/scheduler.h"
#include "net/nqe/network_quality_estimator.h"
#include "url/gurl.h"

namespace network {
class NetworkQualityTracker;
}

namespace offline_pages {

struct ClientId;
class OfflinerClient;
class OfflinerPolicy;
class Offliner;
class SavePageRequest;

// Coordinates queueing and processing save page later requests.
class RequestCoordinator : public KeyedService,
                           public RequestNotifier,
                           public base::SupportsUserData {
 public:
  // Nested observer class.  To make sure that no events are missed, the client
  // code should first register for notifications, then |GetAllRequests|, and
  // ignore all events before the return from |GetAllRequests|, and consume
  // events after the return callback from |GetAllRequests|.
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnAdded(const SavePageRequest& request) = 0;
    virtual void OnCompleted(
        const SavePageRequest& request,
        RequestNotifier::BackgroundSavePageResult status) = 0;
    virtual void OnChanged(const SavePageRequest& request) = 0;
    virtual void OnNetworkProgress(const SavePageRequest& request,
                                   int64_t received_bytes) = 0;
  };

  class ActiveTabInfo {
   public:
    virtual ~ActiveTabInfo() = default;
    // Returns true if the active tab's URL matches |url|. If Chrome is in the
    // background, this should return false.
    virtual bool DoesActiveTabMatch(const GURL& url) = 0;
  };

  enum class RequestAvailability {
    ENABLED_FOR_OFFLINER,
    DISABLED_FOR_OFFLINER,
  };

  enum class RequestCoordinatorState {
    IDLE,
    PICKING,
    OFFLINING,
  };

  // Describes the parameters to control how to save a page when system
  // conditions allow.
  struct SavePageLaterParams {
    SavePageLaterParams();
    SavePageLaterParams(const SavePageLaterParams& other);
    ~SavePageLaterParams();

    // The last committed URL of the page to save.
    GURL url;

    // The identification used by the client.
    ClientId client_id;

    // Whether the user requests the save action. Defaults to true.
    bool user_requested;

    // Request availability. Defaults to ENABLED_FOR_OFFLINER.
    RequestAvailability availability;

    // The original URL of the page to save. Empty if no redirect occurs.
    GURL original_url;

    // The origin of the request, if any.
    std::string request_origin;

    // Additional options for adding the request.
    RequestQueue::AddOptions add_options;
  };

  // Callback specifying which request IDs were actually removed.
  typedef base::OnceCallback<void(const MultipleItemStatuses&)>
      RemoveRequestsCallback;

  // Callback that receives the response for GetAllRequests.
  typedef base::OnceCallback<void(
      std::vector<std::unique_ptr<SavePageRequest>>)>
      GetRequestsCallback;

  // Callback for SavePageLater calls.
  typedef base::OnceCallback<void(AddRequestResult)> SavePageLaterCallback;

  RequestCoordinator(std::unique_ptr<OfflinerPolicy> policy,
                     std::unique_ptr<Offliner> offliner,
                     std::unique_ptr<RequestQueue> queue,
                     std::unique_ptr<Scheduler> scheduler,
                     network::NetworkQualityTracker* network_quality_tracker,
                     std::unique_ptr<ActiveTabInfo> active_tab_info);

  RequestCoordinator(const RequestCoordinator&) = delete;
  RequestCoordinator& operator=(const RequestCoordinator&) = delete;

  ~RequestCoordinator() override;

  // Queues |request| to later load and save when system conditions allow.
  // Returns an id if the page could be queued successfully, 0L otherwise.
  int64_t SavePageLater(const SavePageLaterParams& save_page_later_params,
                        SavePageLaterCallback save_page_later_callback);

  // Remove a list of requests by |request_id|.  This removes requests from the
  // request queue, and cancels an in-progress offliner.
  void RemoveRequests(const std::vector<int64_t>& request_ids,
                      RemoveRequestsCallback callback);

  // Invokes |remove_predicate| for all requests in the queue, and removes each
  // request where |remove_predicate| returns true. Note: |remove_predicate| is
  // called from a background thread.
  void RemoveRequestsIf(const base::RepeatingCallback<
                            bool(const SavePageRequest&)>& remove_predicate,
                        RemoveRequestsCallback done_callback);

  // Pause a list of requests by |request_id|.  This will change the state
  // in the request queue so the request cannot be started.
  void PauseRequests(const std::vector<int64_t>& request_ids);

  // Resume a list of previously paused requests, making them available.
  void ResumeRequests(const std::vector<int64_t>& request_ids);

  // Get all save page request items in the callback.
  void GetAllRequests(GetRequestsCallback callback);

  // Calls |RequestQueueStore::SetAutoFetchNotificationState|.
  void SetAutoFetchNotificationState(
      int64_t request_id,
      SavePageRequest::AutoFetchNotificationState state,
      base::OnceCallback<void(bool updated)> callback);

  // Starts processing of one or more queued save page later requests
  // in scheduled background mode.
  // Returns whether processing was started and that caller should expect
  // a callback. If processing was already active, returns false.
  bool StartScheduledProcessing(
      const DeviceConditions& device_conditions,
      const base::RepeatingCallback<void(bool)>& callback);

  // Cancel previously scheduled processing of requests.
  void CancelProcessing();

  // Attempts to starts processing of one or more queued save page later
  // requests (if device conditions are suitable) in immediate mode
  // (opposed to scheduled background mode). This method is suitable to call
  // when there is some user action that suggests the user wants to do this
  // operation now, if possible, vs. trying to do it in the background when
  // idle.
  // Returns whether processing was started and that caller should expect
  // a callback. If processing was already active or some condition was
  // not suitable for immediate processing (e.g., network or low-end device),
  // returns false.
  bool StartImmediateProcessing(
      const base::RepeatingCallback<void(bool)>& callback);

  // Used to denote that the foreground thread is ready for the offliner
  // to start work on a previously entered, but unavailable request.
  void EnableForOffliner(int64_t request_id, const ClientId& client_id);

  // If a request that is unavailable to the offliner is finished elsewhere,
  // (by the tab helper synchronous download), send a notificaiton that it
  // succeeded through our notificaiton system.
  void MarkRequestCompleted(int64_t request_id);

  const Scheduler::TriggerConditions GetTriggerConditions(
      const bool user_requested);

  // A way for tests to set the callback in use when an operation is over.
  void SetProcessingCallbackForTest(
      const base::RepeatingCallback<void(bool)>& callback) {
    scheduler_callback_ = callback;
  }

  // A way to set the callback which would be called if processing will be
  // triggered immediately internally by the coordinator. Used by testing
  // harness to determine if a request has been processed.
  void SetInternalStartProcessingCallbackForTest(
      const base::RepeatingCallback<void(bool)>& callback) {
    internal_start_processing_callback_ = callback;
  }

  // Observers implementing the RequestCoordinator::Observer interface can
  // register here to get notifications of changes to request state.  This
  // pointer is not owned, and it is the callers responsibility to remove the
  // observer before the observer is deleted.
  void AddObserver(RequestCoordinator::Observer* observer);

  void RemoveObserver(RequestCoordinator::Observer* observer);

  // Implement RequestNotifier
  void NotifyAdded(const SavePageRequest& request) override;
  void NotifyCompleted(
      const SavePageRequest& request,
      RequestNotifier::BackgroundSavePageResult status) override;
  void NotifyChanged(const SavePageRequest& request) override;
  void NotifyNetworkProgress(const SavePageRequest& request,
                             int64_t received_bytes) override;

  // Returns the request queue used for requests.  Coordinator keeps ownership.
  RequestQueue* queue_for_testing() { return queue_.get(); }

  // Return an unowned pointer to the Scheduler.
  Scheduler* scheduler() { return scheduler_.get(); }

  OfflinerPolicy* policy() { return policy_.get(); }

  // Return the state of the request coordinator.
  RequestCoordinatorState state() { return state_; }

  // Tracks whether the last offlining attempt got canceled.  This is reset by
  // the next call to start processing.
  bool is_canceled() const {
    return processing_state_ == ProcessingWindowState::STOPPED;
  }

  RequestCoordinatorEventLogger* GetLogger() { return &event_logger_; }

 private:
  // Immediate start attempt status code for UMA.
  // These values are written to logs. New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  // For any additions, also update corresponding histogram in histograms.xml.
  enum OfflinerImmediateStartStatus {
    // Did start processing request.
    STARTED = 0,
    // Already busy processing a request.
    BUSY = 1,
    // The Offliner did not accept processing the request.
    NOT_ACCEPTED = 2,
    // No current network connection.
    NO_CONNECTION = 3,
    // Weak network connection (worse than 2G speed)
    // according to network quality estimator.
    WEAK_CONNECTION = 4,
    // Did not start because this is svelte device.
    NOT_STARTED_ON_SVELTE = 5,
    // NOTE: insert new values above this line and update histogram enum too.
    STATUS_COUNT = 6,
  };

  enum class ProcessingWindowState {
    STOPPED,
    SCHEDULED_WINDOW,
    IMMEDIATE_WINDOW,
  };

  // Stops the current request processing if active. Whether or not a request is
  // active, ScheduleOrTryNextRequest() will be called to resume processing
  // eventually.
  void StopProcessing(Offliner::RequestStatus stop_status);

  // Receives the results of a get from the request queue, and turns that into
  // SavePageRequest objects for the caller of GetQueuedRequests.
  void GetQueuedRequestsCallback(
      GetRequestsCallback callback,
      GetRequestsResult result,
      std::vector<std::unique_ptr<SavePageRequest>> requests);

  // Receives the results of a get from the request queue, and turns that into
  // SavePageRequest objects for the caller of GetQueuedRequests.
  void GetRequestsForSchedulingCallback(
      GetRequestsResult result,
      std::vector<std::unique_ptr<SavePageRequest>> requests);

  // Receives the result of add requests to the request queue.
  void AddRequestResultCallback(SavePageLaterCallback save_page_later_callback,
                                RequestAvailability availability,
                                AddRequestResult result,
                                const SavePageRequest& request);

  void UpdateMultipleRequestsCallback(UpdateRequestsResult result);

  void ReconcileCallback(UpdateRequestsResult result);

  void HandleRemovedRequestsAndCallback(
      RemoveRequestsCallback callback,
      RequestNotifier::BackgroundSavePageResult status,
      UpdateRequestsResult result);

  void HandleRemovedRequests(RequestNotifier::BackgroundSavePageResult status,
                             UpdateRequestsResult result);

  void MarkDeferredAttemptCallback(UpdateRequestsResult result);

  bool StartProcessingInternal(
      const ProcessingWindowState processing_state,
      const base::RepeatingCallback<void(bool)>& callback);

  // Start processing now if connected (but with conservative assumption
  // as to other device conditions).
  void StartImmediatelyIfConnected();

  OfflinerImmediateStartStatus TryImmediateStart(
      const base::RepeatingCallback<void(bool)>& callback);

  // Requests a callback upon the next network connection to start processing.
  void RequestConnectedEventForStarting();

  // Clears the request for connected event if it was set.
  void ClearConnectedEventRequest();

  // Handles receiving a connection event. Will start immediate processing.
  void HandleConnectedEventForStarting();

  // Check the request queue, and schedule a task corresponding
  // to the least restrictive type of request in the queue.
  void ScheduleAsNeeded();

  // Callback from the request picker when it has chosen our next request.
  void RequestPicked(
      const SavePageRequest& request,
      std::unique_ptr<std::vector<SavePageRequest>> available_requests,
      bool cleanup_needed);

  // Callback from the request picker when no more requests are in the queue.
  // The parameter is a signal for what (if any) conditions to schedule future
  // processing for.
  void RequestNotPicked(bool non_user_requested_tasks_remaining,
                        bool cleanup_needed,
                        base::Time available_time);

  // Callback from request picker that receives the current available queued
  // request count as well as the total queued request count (which may be
  // different if unavailable requests are queued such as paused requests).
  // It also receives a flag as to whether this request picking is due to the
  // start of a request processing window.
  void RequestCounts(bool is_start_of_processing,
                     size_t total_requests,
                     size_t available_requests);

  // Marks attempt on the request and sends it to offliner in continuation.
  void SendRequestToOffliner(const SavePageRequest& request);

  // Continuation of |SendRequestToOffliner| after the request is marked as
  // started.
  void StartOffliner(int64_t request_id,
                     const std::string& client_namespace,
                     UpdateRequestsResult update_result);

  // Called by the offliner when an offlining request is completed. (and by
  // tests).
  void OfflinerDoneCallback(const SavePageRequest& request,
                            Offliner::RequestStatus status);

  // Called by the offliner periodically to report the accumulated count of
  // bytes received from the network.
  void OfflinerProgressCallback(const SavePageRequest& request,
                                int64_t received_bytes);

  // Records a completed or aborted attempt for the request and update it in
  // the queue (possibly removing it).
  void UpdateRequestForAttempt(const SavePageRequest& request,
                               Offliner::RequestStatus status);

  // Returns whether we should try another request based on the outcome
  // of the previous one.
  bool ShouldTryNextRequest(
      Offliner::RequestStatus previous_request_status) const;

  // Try to find and start offlining an available request.
  // |is_start_of_processing| identifies if this is the beginning of a
  // processing window (vs. continuing within a current processing window).
  void TryNextRequest(bool is_start_of_processing);

  // Either schedules to resume processing, or resumes processing immediately.
  // |previous_status| is the reason processing the previous request stopped.
  void ScheduleOrTryNextRequest(Offliner::RequestStatus previous_status);

  // Cross the JNI Bridge and get the current device conditions from Android.
  void UpdateCurrentConditionsFromAndroid();

  // If the active request is identified by |request_id|, cancel it.
  bool CancelActiveRequestIfItMatches(int64_t request_id);

  // Records an aborted attempt for the request and update it in the queue
  // (possibly removing it).
  void UpdateRequestForAbortedAttempt(const SavePageRequest& request);

  // Remove the attempted request from the queue with status to pass through to
  // any observers and UMA histogram.
  void RemoveAttemptedRequest(const SavePageRequest& request,
                              BackgroundSavePageResult status);

  // Marks the attempt as aborted. This makes the request available again
  // for offlining.
  void MarkAttemptAborted(int64_t request_id, const std::string& name_space);

  // Reports change from marking request, reports an error if it fails.
  void MarkAttemptDone(int64_t request_id,
                       const std::string& name_space,
                       UpdateRequestsResult result);

  // Reports offliner status through UMA and event logger.
  void RecordOfflinerResult(const SavePageRequest& request,
                            Offliner::RequestStatus status);

  void SetDeviceConditionsForTest(const DeviceConditions& current_conditions) {
    use_test_device_conditions_ = true;
    current_conditions_.reset(new DeviceConditions(current_conditions));
  }

  // KeyedService implementation:
  void Shutdown() override;

  friend class RequestCoordinatorTest;

  // Cached value of whether low end device. Overwritable for testing.
  bool is_low_end_device_;

  // Access to |Offliner|, always non-null.
  std::unique_ptr<OfflinerClient> offliner_client_;
  // Current state of the request coordinator.
  RequestCoordinatorState state_;
  // Identifies the type of current processing window or if processing stopped.
  ProcessingWindowState processing_state_;
  // True if we should use the test device conditions instead of actual
  // conditions.
  bool use_test_device_conditions_;
  // For use by tests, a fake network connection type
  net::NetworkChangeNotifier::ConnectionType test_connection_type_;
  base::Time operation_start_time_;
  // The observers.
  base::ObserverList<Observer>::Unchecked observers_;
  // Last known conditions for network, battery
  std::unique_ptr<DeviceConditions> current_conditions_;
  // RequestCoordinator takes over ownership of the policy
  std::unique_ptr<OfflinerPolicy> policy_;
  // RequestQueue.  Used to store incoming requests. Owned.
  std::unique_ptr<RequestQueue> queue_;
  // Scheduler. Used to request a callback when network is available.  Owned.
  std::unique_ptr<Scheduler> scheduler_;
  // Unowned pointer. Guaranteed to be non-null during the lifetime of |this|.
  // Must be accessed only on the UI thread.
  raw_ptr<network::NetworkQualityTracker> network_quality_tracker_;
  net::EffectiveConnectionType network_quality_at_request_start_;
  // Holds an ID of the currently active request.
  int64_t active_request_id_;
  // Status of the most recent offlining, retained for testing only.
  Offliner::RequestStatus last_offlining_status_;
  // A set of request_ids that we are holding off until the download manager is
  // done with them.
  std::set<int64_t> disabled_requests_;
  // The processing callback to call when processing the current processing
  // window stops. It is set from the Start*Processing() call that triggered
  // the processing or it may be the |internal_start_processing_callback_| if
  // processing was triggered internally.
  // For StartScheduledProcessing() processing, calling its callback returns
  // to the scheduler across the JNI bridge.
  base::RepeatingCallback<void(bool)> scheduler_callback_;
  // Callback invoked when internally triggered processing is done. It is
  // kept as a class member so that it may be overridden for test visibility.
  base::RepeatingCallback<void(bool)> internal_start_processing_callback_;
  // Logger to record events.
  RequestCoordinatorEventLogger event_logger_;
  // Used for potential immediate processing when we get network connection.
  std::unique_ptr<ConnectionNotifier> connection_notifier_;
  // Used to track prioritized requests.
  // The requests can only be added by RC when they are resumed and there are
  // two places where deletion from the |prioritized_requests_| would happen:
  //   1. When request is paused RC will remove it.
  //   2. When a task is not available to be picked by PickRequestTask (because
  //   it was completed or cancelled), the task will remove it.
  // Currently it's used as LIFO.
  // TODO(romax): see if LIFO is a good idea or change to FIFO. crbug.com/705106
  base::circular_deque<int64_t> prioritized_requests_;
  // Updates a request's PendingState.
  PendingStateUpdater pending_state_updater_;

  std::unique_ptr<ActiveTabInfo> active_tab_info_;
  // Allows us to pass a weak pointer to callbacks.
  base::WeakPtrFactory<RequestCoordinator> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_COORDINATOR_H_
