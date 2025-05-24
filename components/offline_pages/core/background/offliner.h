// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_H_

#include <string>

#include "base/functional/callback.h"

namespace offline_pages {

class SavePageRequest;

// Interface of a class responsible for constructing an offline page given
// a request with a URL.
class Offliner {
 public:
  // Status of processing an offline page request.
  // WARNING: You must update histograms.xml to match any changes made to
  // this enum (ie, OfflinePagesBackgroundOfflinerRequestStatus histogram enum).
  // Also update related switch code in RequestCoordinatorEventLogger.
  enum class RequestStatus {
    // No status determined/reported yet. Interim status, not sent in callback.
    UNKNOWN = 0,
    // Page loaded but not (yet) saved. Interim status, not sent in callback.
    LOADED = 1,
    // Offline page snapshot saved.
    SAVED = 2,
    // RequestCoordinator canceled request.
    REQUEST_COORDINATOR_CANCELED = 3,
    // Loading was canceled.
    LOADING_CANCELED = 4,
    // Loader failed to load page because the system or Chrome encountered an
    // error.
    LOADING_FAILED = 5,
    // Failed to save loaded page.
    SAVE_FAILED = 6,
    // Foreground transition canceled request.
    FOREGROUND_CANCELED = 7,
    // RequestCoordinator canceled request attempt per time limit.
    REQUEST_COORDINATOR_TIMED_OUT = 8,
    // Deprecated. The RequestCoordinator did not start loading the request.
    DEPRECATED_LOADING_NOT_STARTED = 9,
    // Loader failed with hard error so should not retry the request.
    LOADING_FAILED_NO_RETRY = 10,
    // Loader failed with some error that suggests we should not continue
    // processing another request at this time.
    LOADING_FAILED_NO_NEXT = 11,
    // The RequestCoordinator tried to start loading the request but the
    // loading request was not accepted.
    LOADING_NOT_ACCEPTED = 12,
    // The RequestCoordinator did not start loading the request because
    // updating the status in the request queue failed.
    QUEUE_UPDATE_FAILED = 13,
    // Scheduler canceled processing of requests.
    BACKGROUND_SCHEDULER_CANCELED = 14,
    // We saved a snapshot on the last retry, after timeout.
    SAVED_ON_LAST_RETRY = 15,
    // Indicates that attempt failed due to browser being killed.
    // There are 3 ways that might happen:
    // * System was running out of memory, while browser was running in the
    //   background.
    // * User swiped away the browser as it was offlining content.
    // * Offliner crashed.
    // We detect the situation in ReconcileTask after starting
    // RequestCoordinator.
    BROWSER_KILLED = 16,
    // The page initiated a download, we denied the downloads.
    LOADING_FAILED_DOWNLOAD = 17,
    // The page initiated a download, and we passed it on to downloads.
    DOWNLOAD_THROTTLED = 18,
    // Loader failed to load page due to net error.
    LOADING_FAILED_NET_ERROR = 19,
    // Loader failed to load page due to HTTP error.
    LOADING_FAILED_HTTP_ERROR = 20,
    // Loading was deferred because the active tab URL matches.
    LOADING_DEFERRED = 21,
    // The loaded page has a HTTPS certificate error.
    LOADED_PAGE_HAS_CERTIFICATE_ERROR = 22,
    // The loaded page is blocked by SafeBrowsing.
    LOADED_PAGE_IS_BLOCKED = 23,
    // The loaded page is an interstitial or an error page.
    LOADED_PAGE_IS_CHROME_INTERNAL = 24,

    kMaxValue = LOADED_PAGE_IS_CHROME_INTERNAL,
  };

  // Reports the load progress of a request.
  typedef base::RepeatingCallback<void(const SavePageRequest&,
                                       int64_t received_bytes)>
      ProgressCallback;
  // Reports the completion status of a request.
  typedef base::OnceCallback<void(const SavePageRequest&, RequestStatus)>
      CompletionCallback;
  // Reports that the cancel operation has completed.
  // TODO(chili): make save operation cancellable.
  typedef base::OnceCallback<void(const SavePageRequest&)> CancelCallback;

  Offliner() = default;
  virtual ~Offliner() = default;

  // Processes |request| to load and save an offline page.
  // Returns whether the request was accepted or not. |completion_callback| is
  // guaranteed to be called if the request was accepted and |Cancel()| is not
  // called on it. |progress_callback| is invoked periodically to report the
  // number of bytes received from the network (for UI purposes).
  virtual bool LoadAndSave(const SavePageRequest& request,
                           CompletionCallback completion_callback,
                           const ProgressCallback& progress_callback) = 0;

  // Clears the currently processing request, if any, and skips running its
  // CompletionCallback. Returns false if there is nothing to cancel, otherwise
  // returns true and canceled request will be delivered using callback.
  virtual bool Cancel(CancelCallback callback) = 0;

  // On some external condition changes (RAM pressure, browser backgrounded on
  // low-level devices, etc) it is needed to terminate a load if there is one
  // in progress. It is no-op if there is no active request loading.
  virtual void TerminateLoadIfInProgress() = 0;

  // Handles timeout scenario. Returns true if lowbar is met and try to do a
  // snapshot of the current webcontents. If that is the case, the result of
  // offlining will be provided by |completion_callback|.
  virtual bool HandleTimeout(int64_t request_id) = 0;

  // TODO(dougarnett): add policy support methods.

  static std::string RequestStatusToString(RequestStatus request_status);
};

// This operator is for testing only, implemented in ./test_util.cc.
// This is provided here to avoid ODR problems.
std::ostream& operator<<(std::ostream& out,
                         const Offliner::RequestStatus& value);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_H_
