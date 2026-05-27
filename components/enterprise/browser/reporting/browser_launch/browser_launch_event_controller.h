// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_EVENT_CONTROLLER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_EVENT_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "net/base/backoff_entry.h"

namespace enterprise_reporting {

class BrowserLaunchEventUploader;

// Orchestrates the capture and reporting of browser launch data.
//
// This class is responsible for pulling the launch data from a
// platform-specific delegate, populating the event proto, and managing the
// upload through an uploader interface. It implements an in-memory retry
// mechanism using exponential backoff for transient upload failures.
class BrowserLaunchEventController {
 public:
  // The collector abstracts platform-specific data capture, such as retrieving
  // the unpolluted command-line switches and process creation time.
  class LaunchDataCollector {
   public:
    virtual ~LaunchDataCollector() = default;

    // Returns a fully populated BrowserLaunchEvent proto.
    virtual ::chrome::cros::reporting::proto::BrowserLaunchEvent&&
    GetEvent() = 0;
  };

  explicit BrowserLaunchEventController(
      std::unique_ptr<LaunchDataCollector> collector,
      std::unique_ptr<BrowserLaunchEventUploader> uploader);
  BrowserLaunchEventController(const BrowserLaunchEventController&) = delete;
  BrowserLaunchEventController& operator=(const BrowserLaunchEventController&) =
      delete;
  ~BrowserLaunchEventController();

  // Pulls data from the collector once and initiates the first upload attempt.
  // This should only be called once per browser session. Subsequent calls
  // will trigger a CHECK.
  void CollectAndUpload();

 private:
  // Initiates an upload attempt. Must not be called while the backoff timer is
  // active (will trigger a CHECK).
  void AttemptUpload();

  // Callback invoked by the uploader. If successful, the pending event is
  // cleared. If it fails, a retry is scheduled based on the backoff policy.
  void OnEventUploaded(policy::CloudPolicyClient::Result result);

  std::unique_ptr<LaunchDataCollector> collector_;
  std::unique_ptr<BrowserLaunchEventUploader> uploader_;

  // The event captured at launch. This is also used as a flag to ensure
  // collection only happens once: if this has a value, collection has already
  // started.
  std::optional<::chrome::cros::reporting::proto::BrowserLaunchEvent>
      pending_upload_event_;

  net::BackoffEntry retry_backoff_;
  base::OneShotTimer retry_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BrowserLaunchEventController> weak_ptr_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_EVENT_CONTROLLER_H_
