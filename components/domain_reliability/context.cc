// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/not_fatal_until.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "components/domain_reliability/dispatcher.h"
#include "components/domain_reliability/uploader.h"
#include "components/domain_reliability/util.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"

namespace domain_reliability {

// static
const int DomainReliabilityContext::kMaxUploadDepthToSchedule = 1;

// static
const size_t DomainReliabilityContext::kMaxQueuedBeacons = 150;

DomainReliabilityContext::DomainReliabilityContext(
    const MockableTime* time,
    const DomainReliabilityScheduler::Params& scheduler_params,
    const std::string& upload_reporter_string,
    const base::TimeTicks* last_network_change_time,
    const UploadAllowedCallback& upload_allowed_callback,
    DomainReliabilityDispatcher* dispatcher,
    DomainReliabilityUploader* uploader,
    std::unique_ptr<const DomainReliabilityConfig> config)
    : config_(std::move(config)),
      time_(time),
      upload_reporter_string_(upload_reporter_string),
      scheduler_(time,
                 config_->collectors.size(),
                 scheduler_params,
                 base::BindRepeating(&DomainReliabilityContext::ScheduleUpload,
                                     base::Unretained(this))),
      dispatcher_(dispatcher),
      uploader_(uploader),
      uploading_beacons_size_(0),
      last_network_change_time_(last_network_change_time),
      upload_allowed_callback_(upload_allowed_callback) {}

DomainReliabilityContext::~DomainReliabilityContext() {
  for (auto& beacon_ptr : beacons_) {
    beacon_ptr->outcome = DomainReliabilityBeacon::Outcome::kContextShutDown;
  }
}

void DomainReliabilityContext::OnBeacon(
    std::unique_ptr<DomainReliabilityBeacon> beacon) {
  bool success = (beacon->status == "ok");
  double sample_rate = beacon->details.quic_port_migration_detected
                           ? 1.0
                           : config().GetSampleRate(success);
  if (base::RandDouble() >= sample_rate)
    return;
  beacon->sample_rate = sample_rate;

  // Allow beacons about reports, but don't schedule an upload for more than
  // one layer of recursion, to avoid infinite report loops.
  if (beacon->upload_depth <= kMaxUploadDepthToSchedule)
    scheduler_.OnBeaconAdded();
  beacons_.push_back(std::move(beacon));
  bool should_evict = beacons_.size() > kMaxQueuedBeacons;
  if (should_evict)
    RemoveOldestBeacon();
}

void DomainReliabilityContext::ClearBeacons() {
  for (auto& beacon_ptr : beacons_) {
    beacon_ptr->outcome = DomainReliabilityBeacon::Outcome::kCleared;
  }
  beacons_.clear();
  uploading_beacons_size_ = 0;
}

void DomainReliabilityContext::GetQueuedBeaconsForTesting(
    std::vector<const DomainReliabilityBeacon*>* beacons_out) const {
  DCHECK(beacons_out);
  beacons_out->clear();
  for (const auto& beacon : beacons_)
    beacons_out->push_back(beacon.get());
}

void DomainReliabilityContext::ScheduleUpload(
    base::TimeDelta min_delay,
    base::TimeDelta max_delay) {
  dispatcher_->ScheduleTask(
      base::BindOnce(&DomainReliabilityContext::CallUploadAllowedCallback,
                     weak_factory_.GetWeakPtr()),
      min_delay, max_delay);
}

void DomainReliabilityContext::CallUploadAllowedCallback() {
  RemoveExpiredBeacons();
  if (beacons_.empty())
    return;

  upload_allowed_callback_->Run(
      config().origin,
      base::BindOnce(&DomainReliabilityContext::OnUploadAllowedCallbackComplete,
                     weak_factory_.GetWeakPtr()));
}

void DomainReliabilityContext::OnUploadAllowedCallbackComplete(bool allowed) {
  if (allowed)
    StartUpload();
}

void DomainReliabilityContext::StartUpload() {
  RemoveExpiredBeacons();
  if (beacons_.empty())
    return;

  // Find the first beacon with an `upload_depth` of at most
  // kMaxUploadDepthToSchedule, in preparation to create a report containing all
  // beacons with matching NetworkIsolationKeys.
  bool found_beacon_to_upload = false;
  for (const auto& beacon : beacons_) {
    if (beacon->upload_depth <= kMaxUploadDepthToSchedule) {
      uploading_beacons_isolation_info_ = beacon->isolation_info;
      found_beacon_to_upload = true;
      break;
    }
  }
  if (!found_beacon_to_upload)
    return;

  size_t collector_index = scheduler_.OnUploadStart();
  const GURL& collector_url = *config().collectors[collector_index];

  DCHECK(upload_time_.is_null());
  upload_time_ = time_->NowTicks();
  std::string report_json = "{}";
  int max_upload_depth = -1;
  bool wrote = base::JSONWriter::Write(
      CreateReport(upload_time_, collector_url, &max_upload_depth),
      &report_json);
  DCHECK(wrote);
  DCHECK_NE(-1, max_upload_depth);

  uploader_->UploadReport(
      report_json, max_upload_depth, collector_url,
      uploading_beacons_isolation_info_,
      base::BindOnce(&DomainReliabilityContext::OnUploadComplete,
                     weak_factory_.GetWeakPtr()));
}

void DomainReliabilityContext::OnUploadComplete(
    const DomainReliabilityUploader::UploadResult& result) {
  if (result.is_success()) {
    CommitUpload();
  } else {
    RollbackUpload();
  }
  scheduler_.OnUploadComplete(result);
  DCHECK(!upload_time_.is_null());
  last_upload_time_ = upload_time_;
  upload_time_ = base::TimeTicks();

  // If there are pending beacons with a low enough depth, inform the scheduler
  // - it's possible only some beacons were added because of NetworkIsolationKey
  // mismatches, rather than due to new beacons being created.
  if (GetMinBeaconUploadDepth() <= kMaxUploadDepthToSchedule)
    scheduler_.OnBeaconAdded();
}

base::Value DomainReliabilityContext::CreateReport(base::TimeTicks upload_time,
                                                   const GURL& collector_url,
                                                   int* max_upload_depth_out) {
  DCHECK_GT(beacons_.size(), 0u);
  DCHECK_EQ(0u, uploading_beacons_size_);

  int max_upload_depth = 0;

  base::Value::List beacons_value;
  for (const auto& beacon : beacons_) {
    // Only include beacons with a matching NetworkIsolationKey in the report.
    if (beacon->isolation_info.network_isolation_key() !=
        uploading_beacons_isolation_info_.network_isolation_key()) {
      continue;
    }

    beacons_value.Append(
        beacon->ToValue(upload_time, *last_network_change_time_, collector_url,
                        config().path_prefixes));
    if (beacon->upload_depth > max_upload_depth)
      max_upload_depth = beacon->upload_depth;
    ++uploading_beacons_size_;
  }

  DCHECK_GT(uploading_beacons_size_, 0u);

  base::Value::Dict report_value;
  report_value.Set("reporter", *upload_reporter_string_);
  report_value.Set("entries", std::move(beacons_value));

  *max_upload_depth_out = max_upload_depth;
  return base::Value(std::move(report_value));
}

void DomainReliabilityContext::CommitUpload() {
  auto current = beacons_.begin();
  while (uploading_beacons_size_ > 0) {
    CHECK(current != beacons_.end(), base::NotFatalUntil::M130);

    auto last = current;
    ++current;
    if ((*last)->isolation_info.network_isolation_key() ==
        uploading_beacons_isolation_info_.network_isolation_key()) {
      (*last)->outcome = DomainReliabilityBeacon::Outcome::kUploaded;
      beacons_.erase(last);
      --uploading_beacons_size_;
    }
  }
}

void DomainReliabilityContext::RollbackUpload() {
  uploading_beacons_size_ = 0;
}

void DomainReliabilityContext::RemoveOldestBeacon() {
  DCHECK(!beacons_.empty());

  DVLOG(1) << "Beacon queue for " << config().origin << " full; "
           << "removing oldest beacon";

  // If the beacon being removed has a NetworkIsolationKey that matches that of
  // the current upload, decrement `uploading_beacons_size_`.
  if (uploading_beacons_size_ > 0 &&
      beacons_.front()->isolation_info.network_isolation_key() ==
          uploading_beacons_isolation_info_.network_isolation_key()) {
    --uploading_beacons_size_;
  }

  beacons_.front()->outcome = DomainReliabilityBeacon::Outcome::kEvicted;
  beacons_.pop_front();
}

void DomainReliabilityContext::RemoveExpiredBeacons() {
  base::TimeTicks now = time_->NowTicks();
  const base::TimeDelta kMaxAge = base::Hours(1);
  while (!beacons_.empty() && now - beacons_.front()->start_time >= kMaxAge) {
    beacons_.front()->outcome = DomainReliabilityBeacon::Outcome::kExpired;
    beacons_.pop_front();
  }
}

// Gets the minimum depth of all entries in |beacons_|.
int DomainReliabilityContext::GetMinBeaconUploadDepth() const {
  int min = std::numeric_limits<int>::max();
  for (const auto& beacon : beacons_) {
    if (beacon->upload_depth < min)
      min = beacon->upload_depth;
  }
  return min;
}

}  // namespace domain_reliability
