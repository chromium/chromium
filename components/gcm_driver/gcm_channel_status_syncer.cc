// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_channel_status_syncer.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/gcm_driver/gcm_channel_status_request.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace gcm {

namespace {

// A small delay to avoid sending request at browser startup time for first-time
// request.
const int kFirstTimeDelaySeconds = 1 * 60;  // 1 minute.

// The fuzzing variation added to the polling delay.
const int kGCMChannelRequestTimeJitterSeconds = 15 * 60;  // 15 minues.

// The minimum poll interval that can be overridden to.
const int kMinCustomPollIntervalMinutes = 2;

// Custom poll interval could not be used more than the limit below.
const int kMaxNumberToUseCustomPollInterval = 10;

}  // namespace

namespace prefs {

// The GCM channel's enabled state.
const char kGCMChannelStatus[] = "gcm.channel_status";

// The GCM channel's polling interval (in seconds).
const char kGCMChannelPollIntervalSeconds[] = "gcm.poll_interval";

// Last time when checking with the GCM channel status server is done.
const char kGCMChannelLastCheckTime[] = "gcm.check_time";

}  // namepsace prefs

namespace switches {

// Override the default poll interval for testing purpose.
const char kCustomPollIntervalMinutes[] = "gcm-channel-poll-interval";

}  // namepsace switches

// static
void GCMChannelStatusSyncer::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGCMChannelStatus, true);
  registry->RegisterIntegerPref(
      prefs::kGCMChannelPollIntervalSeconds,
      GCMChannelStatusRequest::default_poll_interval_seconds());
  registry->RegisterInt64Pref(prefs::kGCMChannelLastCheckTime, 0);
}

// static
void GCMChannelStatusSyncer::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kGCMChannelStatus, true);
  registry->RegisterIntegerPref(
      prefs::kGCMChannelPollIntervalSeconds,
      GCMChannelStatusRequest::default_poll_interval_seconds());
  registry->RegisterInt64Pref(prefs::kGCMChannelLastCheckTime, 0);
}

// static
int GCMChannelStatusSyncer::first_time_delay_seconds() {
  return kFirstTimeDelaySeconds;
}

GCMChannelStatusSyncer::GCMChannelStatusSyncer(
    GCMDriver* driver,
    PrefService* prefs,
    const std::string& channel_status_request_url,
    const std::string& user_agent,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : driver_(driver),
      prefs_(prefs),
      channel_status_request_url_(channel_status_request_url),
      user_agent_(user_agent),
      url_loader_factory_(std::move(url_loader_factory)),
      started_(false),
      gcm_enabled_(true),
      poll_interval_seconds_(
          GCMChannelStatusRequest::default_poll_interval_seconds()),
      custom_poll_interval_use_count_(0),
      delay_removed_for_testing_(false) {
  gcm_enabled_ = prefs_->GetBoolean(prefs::kGCMChannelStatus);
  poll_interval_seconds_ = prefs_->GetInteger(
      prefs::kGCMChannelPollIntervalSeconds);
  if (poll_interval_seconds_ <
      GCMChannelStatusRequest::min_poll_interval_seconds()) {
    poll_interval_seconds_ =
        GCMChannelStatusRequest::min_poll_interval_seconds();
  }
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kCustomPollIntervalMinutes)) {
    std::string value(command_line.GetSwitchValueASCII(
        switches::kCustomPollIntervalMinutes));
    int minutes = 0;
    if (base::StringToInt(value, &minutes)) {
      DCHECK_GE(minutes, kMinCustomPollIntervalMinutes);
      if (minutes >= kMinCustomPollIntervalMinutes) {
        poll_interval_seconds_ = minutes * 60;
        custom_poll_interval_use_count_ = kMaxNumberToUseCustomPollInterval;
      }
    }
  }
  last_check_time_ = base::Time::FromInternalValue(
      prefs_->GetInt64(prefs::kGCMChannelLastCheckTime));
}

GCMChannelStatusSyncer::~GCMChannelStatusSyncer() {
}

void GCMChannelStatusSyncer::EnsureStarted() {
  // Bail out if the request is already scheduled or started.
  if (started_)
    return;
  started_ = true;

  ScheduleRequest();
}

void GCMChannelStatusSyncer::Stop() {
  started_ = false;
  request_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void GCMChannelStatusSyncer::OnRequestCompleted(bool update_received,
                                                bool enabled,
                                                int poll_interval_seconds) {
  DCHECK(request_);
  request_.reset();

  // Persist the current time as the last request complete time.
  last_check_time_ = base::Time::Now();
  prefs_->SetInt64(prefs::kGCMChannelLastCheckTime,
                   last_check_time_.ToInternalValue());

  if (update_received) {
    if (gcm_enabled_ != enabled) {
      gcm_enabled_ = enabled;
      prefs_->SetBoolean(prefs::kGCMChannelStatus, enabled);
      if (gcm_enabled_)
        driver_->Enable();
      else
        driver_->Disable();
    }

    // Skip updating poll interval if the custom one is still in effect.
    if (!custom_poll_interval_use_count_) {
      DCHECK_GE(poll_interval_seconds,
                GCMChannelStatusRequest::min_poll_interval_seconds());
      if (poll_interval_seconds_ != poll_interval_seconds) {
        poll_interval_seconds_ = poll_interval_seconds;
        prefs_->SetInteger(prefs::kGCMChannelPollIntervalSeconds,
                           poll_interval_seconds_);
      }
    }
  }

  // Do not schedule next request if syncer is stopped.
  if (started_)
    ScheduleRequest();
}

void GCMChannelStatusSyncer::ScheduleRequest() {
  current_request_delay_interval_ = GetRequestDelayInterval();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GCMChannelStatusSyncer::StartRequest,
                     weak_ptr_factory_.GetWeakPtr()),
      current_request_delay_interval_);

  if (custom_poll_interval_use_count_)
    custom_poll_interval_use_count_--;
}

void GCMChannelStatusSyncer::StartRequest() {
  DCHECK(!request_);

  if (channel_status_request_url_.empty())
    return;

  request_.reset(new GCMChannelStatusRequest(
      url_loader_factory_, channel_status_request_url_, user_agent_,
      base::Bind(&GCMChannelStatusSyncer::OnRequestCompleted,
                 weak_ptr_factory_.GetWeakPtr())));
  request_->Start();
}

base::TimeDelta GCMChannelStatusSyncer::GetRequestDelayInterval() const {
  // No delay during testing.
  if (delay_removed_for_testing_)
    return base::TimeDelta();

  // Make sure that checking with server occurs at polling interval, regardless
  // whether the browser restarts.
  int64_t delay_seconds = poll_interval_seconds_ -
                          (base::Time::Now() - last_check_time_).InSeconds();
  if (delay_seconds < 0)
    delay_seconds = 0;

  if (last_check_time_.is_null()) {
    // For the first-time request, add a small delay to avoid sending request at
    // browser startup time.
    DCHECK(!delay_seconds);
    delay_seconds = kFirstTimeDelaySeconds;
  } else {
    // Otherwise, add a fuzzing variation to the delay.
    // The fuzzing variation is off when the custom interval is used.
    if (!custom_poll_interval_use_count_)
      delay_seconds += base::RandInt(0, kGCMChannelRequestTimeJitterSeconds);
  }

  return base::TimeDelta::FromSeconds(delay_seconds);
}

}  // namespace gcm
