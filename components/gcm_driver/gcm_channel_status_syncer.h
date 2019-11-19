// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_CHANNEL_STATUS_SYNCER_H_
#define COMPONENTS_GCM_DRIVER_GCM_CHANNEL_STATUS_SYNCER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

class PrefService;
class PrefRegistrySimple;

namespace network {
class SharedURLLoaderFactory;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace gcm {

class GCMChannelStatusRequest;
class GCMDriver;

namespace prefs {
// The GCM channel's enabled state.
extern const char kGCMChannelStatus[];
}  // namepsace prefs

// Syncing with the server for GCM channel status that controls if GCM
// functionality should be enabled or disabled.
class GCMChannelStatusSyncer {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  GCMChannelStatusSyncer(
      GCMDriver* driver,
      PrefService* prefs,
      const std::string& channel_status_request_url,
      const std::string& user_agent,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~GCMChannelStatusSyncer();

  void EnsureStarted();
  void Stop();

  bool gcm_enabled() const { return gcm_enabled_; }

  // For testing purpose.
  void set_delay_removed_for_testing(bool delay_removed) {
    delay_removed_for_testing_ = delay_removed;
  }
  base::TimeDelta current_request_delay_interval() const {
    return current_request_delay_interval_;
  }
  GCMChannelStatusRequest* request_for_testing() const {
    return request_.get();
  }
  static int first_time_delay_seconds();

 private:
  // Called when a request is completed.
  void OnRequestCompleted(bool update_received,
                          bool enabled,
                          int poll_interval_seconds);

  // Schedules next request to start after appropriate delay.
  void ScheduleRequest();

  // Creates and starts a request immediately.
  void StartRequest();

  // Computes and returns a delay with the fuzzing variation added if needed,
  // after which the request could start.
  base::TimeDelta GetRequestDelayInterval() const;

  // GCMDriver owns GCMChannelStatusSyncer instance.
  GCMDriver* driver_;
  PrefService* prefs_;
  const std::string channel_status_request_url_;
  const std::string user_agent_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<GCMChannelStatusRequest> request_;

  bool started_;
  bool gcm_enabled_;
  int poll_interval_seconds_;
  base::Time last_check_time_;

  // If non-zero, |poll_interval_seconds_| is overriden by the command line
  // options for testing purpose. Each time when the custom poll interval is
  // used, this count is subtracted by one. When it reaches zero, the default
  // poll interval will be used instead.
  int custom_poll_interval_use_count_;

  // The flag that indicates if the delay, including fuzzing variation and poll
  // interval, is removed for testing purpose.
  bool delay_removed_for_testing_;

  // Tracked for testing purpose.
  base::TimeDelta current_request_delay_interval_;

  // Used to pass a weak pointer to a task.
  base::WeakPtrFactory<GCMChannelStatusSyncer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GCMChannelStatusSyncer);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_CHANNEL_STATUS_SYNCER_H_
