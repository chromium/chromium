// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_SAVE_PAGE_REQUEST_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_SAVE_PAGE_REQUEST_H_

#include <stdint.h>
#include <iosfwd>

#include "base/time/time.h"
#include "components/offline_items_collection/core/fail_state.h"
#include "components/offline_items_collection/core/pending_state.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "url/gurl.h"

using offline_items_collection::FailState;
using offline_items_collection::PendingState;

namespace offline_pages {

// Class representing a request to save page.
class SavePageRequest {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offlinepages
  enum class RequestState : int {
    AVAILABLE = 0,  // Request can be scheduled when preconditions are met.
    PAUSED = 1,     // Request is not available until it is unpaused.
    OFFLINING = 2,  // Request is actively offlining.
  };

  enum class AutoFetchNotificationState : int {
    kUnknown = 0,
    kShown = 1,  // The auto-fetch notification was shown.
  };

  SavePageRequest(int64_t request_id,
                  const GURL& url,
                  const ClientId& client_id,
                  const base::Time& creation_time,
                  const bool user_requested);
  SavePageRequest(const SavePageRequest& other);
  ~SavePageRequest();

  bool operator==(const SavePageRequest& other) const;

  // Updates the |last_attempt_time_| and increments |attempt_count_|.
  void MarkAttemptStarted(const base::Time& start_time);

  // Marks attempt as completed and clears |last_attempt_time_|.
  // Updates the |fail_state_|.
  void MarkAttemptCompleted(FailState fail_state);

  // Marks attempt as aborted. This will change the state of an OFFLINING
  // request to be AVAILABLE.  It will not change the state of a PAUSED request.
  void MarkAttemptAborted();

  // Mark the attempt as paused.  It is not available for future background
  // loading until it has been explicitly unpaused.
  void MarkAttemptPaused();

  // Mark the attempt as deferred. This counts as a failed attempt so that
  // deferred attempts are not unlimited.
  void MarkAttemptDeferred(const base::Time& attempt_time);

  int64_t request_id() const { return request_id_; }

  const GURL& url() const { return url_; }

  const ClientId& client_id() const { return client_id_; }

  RequestState request_state() const { return state_; }
  void set_request_state(RequestState new_state) { state_ = new_state; }

  FailState fail_state() const { return fail_state_; }
  void set_fail_state(FailState new_state) { fail_state_ = new_state; }

  PendingState pending_state() const { return pending_state_; }
  void set_pending_state(PendingState new_state) { pending_state_ = new_state; }

  const base::Time& creation_time() const { return creation_time_; }

  int64_t started_attempt_count() const { return started_attempt_count_; }
  void set_started_attempt_count(int64_t started_attempt_count) {
    started_attempt_count_ = started_attempt_count;
  }

  int64_t completed_attempt_count() const { return completed_attempt_count_; }
  void set_completed_attempt_count(int64_t completed_attempt_count) {
    completed_attempt_count_ = completed_attempt_count;
  }

  const base::Time& last_attempt_time() const { return last_attempt_time_; }
  void set_last_attempt_time(const base::Time& last_attempt_time) {
    last_attempt_time_ = last_attempt_time;
  }

  bool user_requested() const { return user_requested_; }
  void set_user_requested(bool user_requested) {
    user_requested_ = user_requested;
  }

  const GURL& original_url() const { return original_url_; }
  void set_original_url(const GURL& original_url) {
    original_url_ = original_url;
  }

  const std::string& request_origin() const { return request_origin_; }
  void set_request_origin(const std::string& request_origin) {
    request_origin_ = request_origin;
  }

  AutoFetchNotificationState auto_fetch_notification_state() const {
    return auto_fetch_notification_state_;
  }
  void set_auto_fetch_notification_state(AutoFetchNotificationState state) {
    auto_fetch_notification_state_ = state;
  }

  // Implemented in test_util.cc.
  std::string ToString() const;

 private:
  // ID of this request.
  int64_t request_id_;

  // Online URL of a page to be offlined.
  GURL url_;

  // Client ID related to the request. Contains namespace and ID assigned by the
  // requester.
  ClientId client_id_;

  // Time when this request was created. (Alternative 2).
  base::Time creation_time_;

  // Number of attempts started to get the page.  This may be different than the
  // number of attempts completed because we could crash.
  int started_attempt_count_;

  // Number of attempts we actually completed to get the page.
  int completed_attempt_count_;

  // Timestamp of the last request starting.
  base::Time last_attempt_time_;

  // Whether the user specifically requested this page (as opposed to a client
  // like AGSA or Now.)
  bool user_requested_;

  // The current state of this request.
  RequestState state_;

  // The reason the request failed downloading.
  FailState fail_state_;

  // The reason the request is available.
  PendingState pending_state_;

  // The original URL of the page to be offlined. Empty if no redirect occurs.
  GURL original_url_;

  // The app package origin of this save page request. Empty if cannot be
  // determined or Chrome.
  std::string request_origin_;

  // Notification state for auto_fetch requests.
  AutoFetchNotificationState auto_fetch_notification_state_ =
      AutoFetchNotificationState::kUnknown;

  // Helper method to update the |fail_state_| of a request.
  void UpdateFailState(FailState fail_state);
};

// Implemented in test_util.cc.
std::ostream& operator<<(std::ostream& out,
                         SavePageRequest::AutoFetchNotificationState value);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_SAVE_PAGE_REQUEST_H_
