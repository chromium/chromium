// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/sb_update_protocol_manager.h"

#include <optional>
#include <string>
#include <utility>

#include "base/rand_util.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using base::Time;

// TODO(crbug.com/362791941): Update/extract v4-specific parts of this file.
namespace safe_browsing {

// SBUpdateProtocolManager implementation --------------------------------

SBUpdateProtocolManager::SBUpdateProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config)
    : next_update_interval_(
          base::Seconds(base::RandIntInclusive(kTimerStartIntervalSecMin,
                                               kTimerStartIntervalSecMax))),
      config_(config),
      url_loader_factory_(url_loader_factory),
      update_error_count_(0),
      update_back_off_mult_(1) {
  // Do not auto-schedule updates. Let the owner (V4LocalDatabaseManager) do it
  // when it is ready to process updates.
}

SBUpdateProtocolManager::~SBUpdateProtocolManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SBUpdateProtocolManager::ResetUpdateErrors() {
  update_error_count_ = 0;
  update_back_off_mult_ = 1;
}

bool SBUpdateProtocolManager::IsUpdateScheduled() const {
  return update_timer_.IsRunning();
}

// According to section 5 of the SafeBrowsing protocol specification, we must
// back off after a certain number of errors.
base::TimeDelta SBUpdateProtocolManager::GetNextUpdateInterval(bool back_off) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(next_update_interval_.is_positive() || next_update_interval_.is_zero());

  base::TimeDelta next = next_update_interval_;
  if (back_off) {
    next = SBProtocolManagerUtil::GetNextBackOffInterval(
        &update_error_count_, &update_back_off_mult_);
  }

  if (!last_response_time_.is_null()) {
    // The callback spent some time updating the database, including disk I/O.
    // Do not wait that extra time.
    base::TimeDelta callback_time = Time::Now() - last_response_time_;
    if (callback_time < next) {
      next -= callback_time;
    } else {
      // If the callback took too long, schedule the next update with no delay.
      next = base::TimeDelta();
    }
  }
  DVLOG(1) << "SBUpdateProtocolManager::GetNextUpdateInterval: "
           << "next_interval: " << next;
  return next;
}

void SBUpdateProtocolManager::CollectUpdateInfo(
    DatabaseManagerInfo::UpdateInfo* update_info) {
  if (last_response_code_) {
    update_info->set_network_status_code(last_response_code_);
  }

  if (last_response_time_.InMillisecondsSinceUnixEpoch()) {
    update_info->set_last_update_time_millis(
        last_response_time_.InMillisecondsSinceUnixEpoch());
  }

  if (next_update_time_) {
    update_info->set_next_update_time_millis(
        next_update_time_.value().InMillisecondsSinceUnixEpoch());
  }
}

const base::Time& SBUpdateProtocolManager::last_response_time() const {
  return last_response_time_;
}

}  // namespace safe_browsing
